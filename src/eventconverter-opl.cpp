/**
 * @file   eventconverter-opl.cpp
 * @brief  EventHandler implementation that produces OPL data from Events.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <sstream>
#include <math.h>
#include <boost/pointer_cast.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/gamemusic/opl-util.hpp>

using namespace camoto::gamemusic;

EventConverter_OPL::EventConverter_OPL(OPLWriterCallback *cb,
	const PatchBankPtr inst, double fnumConversion, unsigned int flags)
	:	cb(cb),
		inst(inst),
		fnumConversion(fnumConversion),
		flags(flags),
		lastTick(0),
		cachedDelay(0),
		modeOPL3(false),
		modeRhythm(false)
{
	// Initialise all OPL registers to zero (this is assumed the initial state
	// upon playback)
	memset(this->oplSet, 0x00, sizeof(this->oplSet));
	memset(this->oplState, 0x00, sizeof(this->oplState));
	for (unsigned int c = 0; c < MAX_OPL_CHANNELS; c++) {
		this->oplChan[c].on = false;
		this->oplChan[c].lastTime = (unsigned long)-1;
		this->oplChan[c].patch = (unsigned int)-1;
	}
	for (unsigned int c = 0; c < MAX_CHANNELS; c++) {
		this->lastData[c].realOplChannel = -1;
	}
	assert(this->oplSet[0][0] == false);
}

EventConverter_OPL::~EventConverter_OPL()
{
}

void EventConverter_OPL::rewind()
{
	this->lastTick = 0;
	return;
}

void EventConverter_OPL::handleEvent(const TempoEvent *ev)
{
	assert(ev->usPerTick > 0);
	this->cb->writeTempoChange(ev->usPerTick);
	return;
}

void EventConverter_OPL::handleEvent(const NoteOnEvent *ev)
{
	assert(this->inst);
	assert(ev->channel != 0); // can't do note on on all channels

	// See if the instrument is already set
	if (ev->instrument >= this->inst->size()) {
		std::stringstream ss;
		ss << "Instrument bank too small - tried to play note with instrument #"
			<< ev->instrument + 1 << " but patch bank only has "
			<< this->inst->size() << " instruments.";
		throw bad_patch(ss.str());
	}
	OPLPatchPtr i = boost::dynamic_pointer_cast<OPLPatch>(this->inst->at(ev->instrument));

	// Don't play this note if there's no patch for it
	if (!i) return;

	// Toggle this now so we calculate the available channels correctly below,
	// because a note on for a rhythm instrument might be the first notification
	// we get about rhythm mode being used.
	if (i->rhythm) this->modeRhythm = true;

	unsigned int fnum, block;
	milliHertzToFnum(ev->milliHertz, &fnum, &block, this->fnumConversion);

	unsigned long delay = ev->absTime - this->lastTick;

	// Set the delay as a cached delay, so that it will be written out before the
	// next register write, whenever that happens to be.
	this->cachedDelay += delay;

	unsigned int availableChannels, chipSplit;
	if (this->modeOPL3) { // or dual OPL2
		if (this->modeRhythm) {
			availableChannels = 18-3; // -3 == 3 operators lost to gain 5 perc insts
			chipSplit = 6; // first channel on second chip
		} else {
			availableChannels = 18;
			chipSplit = 9;
		}
	} else { // single OPL2
		if (this->modeRhythm) availableChannels = 9-3; // -3 == 3 operators lost to gain 5 perc insts
		else availableChannels = 9;
		chipSplit = 0; // no split (only one chip)
	}

	if (this->flags & ReserveFirstChan) {
		availableChannels--;
		if (chipSplit) chipSplit--;
	}

	// Figure out which channel to use

	// A virtual OPL channel is from 0..availableChannels-1 inclusive.  A real OPL
	// channel is the virtual value split into a chipIndex and 0..8, or 0..5 if in
	// rhythm mode and chipIndex == 0.
	int virtualOplChannel = -1;
	int realOplChannel = -1;
	int chipIndex = 0;
	if (i->rhythm == 0) {
		for (unsigned int c = 0; c < availableChannels; c++) {
			// If this channel isn't currently playing but it's using the same
			// instrument we want to play now, select it.
			if ((!this->oplChan[c].on) && (this->oplChan[c].patch == ev->instrument)) {
				virtualOplChannel = c;
				break;
			}

			// Otherwise, if this channel is in use, or had a note played previously,
			// skip it.
			if (this->oplChan[c].lastTime != (unsigned long)-1) continue;

			// Lastly, this channel must be fresh (never used), select it.
			virtualOplChannel = c;
			break;
		}

		if (virtualOplChannel == -1) {
			// All OPL channels are in use (or have been used), figure out which one
			// to reuse.

			// Try to find the oldest note of various types
			unsigned long oldestOnTime = -1, oldestOnChan = -1;
			unsigned long oldestOffTime = -1, oldestOffChan = -1;
			unsigned long oldestSameTime = -1, oldestSameChan = -1;
			for (unsigned int c = 0; c < availableChannels; c++) {
				if (this->oplChan[c].on) {
					if (this->oplChan[c].patch == ev->instrument) {
						// This instrument is the one we want to use
						if (this->oplChan[c].lastTime < oldestSameTime) {
							oldestSameTime = this->oplChan[c].lastTime;
							oldestSameChan = c;
						}
					} else {
						if (this->oplChan[c].lastTime < oldestOnTime) {
							oldestOnTime = this->oplChan[c].lastTime;
							oldestOnChan = c;
						}
					}
				} else {
					if (this->oplChan[c].lastTime < oldestOffTime) {
						oldestOffTime = this->oplChan[c].lastTime;
						oldestOffChan = c;
					}
				}
			}

			// We have to have at least one note on or off!
			assert(
				(oldestOffTime != (unsigned long)-1)
				|| (oldestOnTime != (unsigned long)-1)
				|| (oldestSameTime != (unsigned long)-1)
			);

			if (oldestOffTime != (unsigned long)-1) {
				// Use the channel that had its last note the longest time ago
				virtualOplChannel = oldestOffChan;
			} else if (oldestSameTime != (unsigned long)-1) {
				// Cut the oldest playing note with the same instrument as the new note
				virtualOplChannel = oldestSameChan;
				std::cout << "OPL writer: All " << availableChannels
					<< " channels are in use at t=" << ev->absTime
					<< ", cutting off the oldest note with the same instrument.\n";
			} else { // oldestOnTime != (unsigned long)-1
				assert(oldestOnTime != (unsigned long)-1);
				// Cut the note that's been playing the longest
				virtualOplChannel = oldestOnChan;
				std::cout << "OPL writer: All " << availableChannels
					<< " channels are in use at t=" << ev->absTime
					<< ", cutting off the oldest note.\n";
			}

		}

		// Figure out real channel and split
		if (chipSplit && (virtualOplChannel >= (signed)chipSplit)) {
			realOplChannel = virtualOplChannel - chipSplit;
			chipIndex++;
		} else {
			realOplChannel = virtualOplChannel;
		}
		if (this->flags & ReserveFirstChan) realOplChannel++;

		if (this->oplChan[virtualOplChannel].on) {
			// This channel has a note on, switch it off
			this->processNextPair(0, chipIndex, 0xB0 | realOplChannel,
				(this->oplState[chipIndex][0xB0 | realOplChannel] & ~OPLBIT_KEYON)
			);
			// Don't bother setting oplChan.on = false, it'll be set to true below
		}
	}

	// We always have to set the patch in case the velocity has changed
	switch (i->rhythm) {
		case 1:
			virtualOplChannel = realOplChannel = 7;
			chipIndex = 0;
			this->writeOpSettings(chipIndex, realOplChannel, 0, i, ev->velocity);
			break; // mod, hihat
		case 2:
			virtualOplChannel = realOplChannel = 8;
			chipIndex = 0;
			this->writeOpSettings(chipIndex, realOplChannel, 1, i, ev->velocity);
			break; // car, topcym
		case 3:
			virtualOplChannel = realOplChannel = 8;
			chipIndex = 0;
			this->writeOpSettings(chipIndex, realOplChannel, 0, i, ev->velocity);
			break; // mod, tomtom
		case 4:
			virtualOplChannel = realOplChannel = 7;
			chipIndex = 0;
			this->writeOpSettings(chipIndex, realOplChannel, 1, i, ev->velocity);
			break; // car, snare
		case 5:
			virtualOplChannel = realOplChannel = 6;
			chipIndex = 0;
			// both, bassdrum
			// fall through
		default:
			// normal instrument (or bass drum), write both operators
			this->writeOpSettings(chipIndex, realOplChannel, 0, i, ev->velocity);
			this->writeOpSettings(chipIndex, realOplChannel, 1, i, ev->velocity);
			break;
	}

	assert(realOplChannel >= 0);
	assert(realOplChannel <= 8);

	// Write the rest of the (global) instrument settings.
	if ((i->rhythm != 2) && (i->rhythm != 4)) {
		this->processNextPair(0, chipIndex, BASE_FEED_CONN | realOplChannel,
			0x30 /* L+R OPL3 panning */
			| ((i->feedback & 7) << 1)
			| (i->connection & 1));
	}

	// Remember the patch we just set
	this->oplChan[virtualOplChannel].patch = ev->instrument;

	if (i->rhythm == 0) { // normal instrument
		// Write lower eight bits of note freq
		this->processNextPair(0, chipIndex, 0xA0 | realOplChannel, fnum & 0xFF);

		// Write 0xB0 w/ keyon bit
		this->processNextPair(0, chipIndex, 0xB0 | realOplChannel,
			OPLBIT_KEYON  // keyon enabled
			| (block << 2) // and the octave
			| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
		);

		this->oplChan[virtualOplChannel].on = true; // note on
		this->oplChan[virtualOplChannel].lastTime = ev->absTime;
	} else { // rhythm instrument
		//if ((i->rhythm != 2) && (i->rhythm != 4)) { // can't set hihat or snare freq
			// Write lower eight bits of note freq
			this->processNextPair(0, chipIndex, 0xA0 | realOplChannel, fnum & 0xFF);

			this->processNextPair(0, chipIndex, 0xB0 | realOplChannel,
				//OPLBIT_KEYON  // different keyon for rhythm instruments
				(block << 2) // and the octave
				| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
			);
		//}

		int keyBit = 1 << (i->rhythm-1); // instrument-specific keyon
		if (this->oplState[chipIndex][0xBD] & keyBit) {
			// Note is already playing, disable
			this->processNextPair(0, chipIndex, 0xBD,
				this->oplState[chipIndex][0xBD] ^ keyBit
			);
		}
		// Write keyon for correct instrument
		this->processNextPair(0, chipIndex, 0xBD,
			0x20 |  // enable percussion mode
			this->oplState[chipIndex][0xBD] | keyBit
		);
	}

	this->lastData[ev->channel].realOplChannel = realOplChannel;
	this->lastData[ev->channel].virtualOplChannel = virtualOplChannel;
	this->lastData[ev->channel].rhythm = i->rhythm;
	this->lastData[ev->channel].chipIndex = chipIndex;

	this->lastTick = ev->absTime;
	return;
}

void EventConverter_OPL::handleEvent(const NoteOffEvent *ev)
{
	LastData &d = this->lastData[ev->channel];
	int delay = ev->absTime - this->lastTick;

	if (d.realOplChannel == (unsigned int)-1) return; // note off without note on!

	if (d.rhythm) {
		// Rhythm-mode instrument
		assert(d.chipIndex == 0);
		this->processNextPair(delay, d.chipIndex, 0xBD,
			(this->oplState[d.chipIndex][0xBD] & ~(1 << (d.rhythm-1)))
		);
	} else {
		// Write 0xB0 w/ keyon bit disabled
		this->processNextPair(delay, d.chipIndex, 0xB0 | d.realOplChannel,
			(this->oplState[d.chipIndex][0xB0 | d.realOplChannel] & ~OPLBIT_KEYON)
		);
	}

	this->lastTick = ev->absTime;

	if (d.rhythm == 0) {
		this->oplChan[d.virtualOplChannel].on = false; // note off
		this->oplChan[d.virtualOplChannel].lastTime = ev->absTime;
	}
	d.realOplChannel = (unsigned int)-1;
	return;
}

void EventConverter_OPL::handleEvent(const PitchbendEvent *ev)
{
	LastData &d = this->lastData[ev->channel];
	int delay = ev->absTime - this->lastTick;

	if (d.realOplChannel == (unsigned int)-1) return; // pitchbend without note on!

	unsigned int fnum, block;
	milliHertzToFnum(ev->milliHertz, &fnum, &block, this->fnumConversion);

	// Write lower eight bits of note freq
	this->processNextPair(delay, d.chipIndex, 0xA0 | d.realOplChannel, fnum & 0xFF);

	// Write 0xB0 w/ keyon bit
	this->processNextPair(0, d.chipIndex, 0xB0 | d.realOplChannel,
		OPLBIT_KEYON  // keyon enabled (wouldn't have a pitchbend with no note playing)
		| (block << 2) // and the octave
		| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
	);

	this->lastTick = ev->absTime;
	return;
}

void EventConverter_OPL::handleEvent(const ConfigurationEvent *ev)
{
	int delay = ev->absTime - this->lastTick;
	switch (ev->configType) {
		case ConfigurationEvent::EnableOPL3:
			this->processNextPair(delay, 1, 0x05, ev->value ? 0x01 : 0x00);
			this->modeOPL3 = ev->value;
			break;
		case ConfigurationEvent::EnableDeepTremolo: {
			unsigned int chipIndex = (ev->value >> 1) & 1;
			uint8_t val;
			if (ev->value & 1) val = this->oplState[chipIndex][0xBD] | 0x80;
			else val = this->oplState[chipIndex][0xBD] & 0x7F;
			this->processNextPair(delay, chipIndex, 0xBD, val);
			break;
		}
		case ConfigurationEvent::EnableDeepVibrato: {
			unsigned int chipIndex = (ev->value >> 1) & 1;
			uint8_t val;
			if (ev->value & 1) val = this->oplState[chipIndex][0xBD] | 0x40;
			else val = this->oplState[chipIndex][0xBD] & 0xBF;
			this->processNextPair(delay, chipIndex, 0xBD, val);
			break;
		}
		case ConfigurationEvent::EnableRhythm:
			// Nothing required, rhythm mode is enabled the first time a rhythm
			// instrument is played.
			if (this->modeRhythm && (ev->value == 0)) {
				// Switching rhythm mode off (incl all rhythm instruments)
				this->processNextPair(delay, 0, 0xBD,
					this->oplState[0][0xBD] & ~0x3F
				);
				this->processNextPair(delay, 1, 0xBD,
					this->oplState[1][0xBD] & ~0x3F
				);
			}
			this->modeRhythm = ev->value;
			break;
		case ConfigurationEvent::EnableWaveSel:
			this->processNextPair(delay, 0, 0x01, ev->value ? 0x20 : 0x00);
			break;
	}
	this->lastTick = ev->absTime;
	return;
}

void EventConverter_OPL::processNextPair(uint32_t delay, uint8_t chipIndex,
	uint8_t reg, uint8_t val
)
{
	assert(chipIndex < 2);

	this->cachedDelay += delay;

	// Don't write anything if the value isn't changing
	if (this->oplSet[chipIndex][reg] && (this->oplState[chipIndex][reg] == val)) return;

	OPLEvent oplev;
	oplev.reg = reg;
	oplev.val = val;
	oplev.chipIndex = chipIndex;
	oplev.delay = this->cachedDelay;
	this->cachedDelay = 0;

	this->cb->writeNextPair(&oplev);

	this->oplState[chipIndex][reg] = val;
	this->oplSet[chipIndex][reg] = true;
	return;
}

void EventConverter_OPL::writeOpSettings(int chipIndex, int oplChannel,
	int opNum, OPLPatchPtr i, int velocity
)
{
	uint8_t op;
	OPLOperator *o;
	int outputLevel;
	if (opNum == 0) {
		op = OPLOFFSET_MOD(oplChannel);
		o = &(i->m);
		outputLevel = o->outputLevel;
	} else {
		op = OPLOFFSET_CAR(oplChannel);
		o = &(i->c);
		outputLevel = o->outputLevel;
		if (velocity != 0) {
			// Not using default velocity
			outputLevel = 0x3F - 0x3F * log((float)velocity) / log(256.0);
			// Note the CMF reader sets the velocity to zero to skip this
		}
	}

	this->processNextPair(0, chipIndex, BASE_CHAR_MULT | op,
		((o->enableTremolo & 1) << 7) |
		((o->enableVibrato & 1) << 6) |
		((o->enableSustain & 1) << 5) |
		((o->enableKSR     & 1) << 4) |
		 (o->freqMult      & 0x0F)
	);
	this->processNextPair(0, chipIndex, BASE_SCAL_LEVL | op, (o->scaleLevel << 6) | (outputLevel & 0x3F));
	this->processNextPair(0, chipIndex, BASE_ATCK_DCAY | op, (o->attackRate << 4) | (o->decayRate & 0x0F));
	this->processNextPair(0, chipIndex, BASE_SUST_RLSE | op, (o->sustainRate << 4) | (o->releaseRate & 0x0F));
	this->processNextPair(0, chipIndex, BASE_WAVE      | op,  o->waveSelect & 7);

	return;
}
