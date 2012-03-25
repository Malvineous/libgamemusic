/**
 * @file   eventconverter-opl.cpp
 * @brief  EventHandler implementation that produces OPL data from Events.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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
#include <boost/pointer_cast.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/gamemusic/opl-util.hpp>
#include <camoto/gamemusic/patchbank-opl.hpp>

using namespace camoto::gamemusic;

EventConverter_OPL::EventConverter_OPL(OPLWriterCallback *cb,
	double fnumConversion, unsigned int flags)
	throw ()
	: cb(cb),
	  fnumConversion(fnumConversion),
	  flags(flags),
	  lastTick(0),
	  cachedDelay(0),
	  postponedDelay(0)
{
	// Initialise all OPL registers to zero (this is assumed the initial state
	// upon playback)
	memset(this->oplState, 0, sizeof(this->oplState));
}

EventConverter_OPL::~EventConverter_OPL()
	throw ()
{
}

void EventConverter_OPL::setPatchBank(const PatchBankPtr& instruments)
	throw (EBadPatchType)
{
	this->inst = boost::dynamic_pointer_cast<OPLPatchBank>(instruments);
	if (!this->inst) {
		// Patch bank isn't an OPL one, see if it can be converted into an
		// OPLPatchBank.  May throw EBadPatchType.
		this->inst = OPLPatchBankPtr(new OPLPatchBank(*instruments));
	}
	return;
}

void EventConverter_OPL::handleEvent(const TempoEvent *ev)
	throw (stream::error)
{
	assert(ev->usPerTick > 0);
	this->cb->writeTempoChange(ev->usPerTick);
	return;
}

void EventConverter_OPL::handleEvent(const NoteOnEvent *ev)
	throw (stream::error, EChannelMismatch, EBadPatchType)
{
	assert(this->inst);
	assert(ev->channel != 0); // can't do note on on all channels

	unsigned int fnum, block;
	milliHertzToFnum(ev->milliHertz, &fnum, &block, this->fnumConversion);

	int oplChannel = (ev->channel - 1) % 14; // TODO: channel map for >9 chans
	int rhythm = 0;
	int chipIndex = 0; // TODO: calculate from channel map

	int delay = ev->absTime - this->lastTick;

	// Set the delay as a cached delay, so that it will be written out before the
	// next register write, whenever that happens to be.
	this->cachedDelay += delay;

	// See if the instrument is already set
	if (ev->instrument >= this->inst->getPatchCount()) {
		std::stringstream ss;
		ss << "Instrument bank too small - tried to play note with instrument #"
			<< ev->instrument + 1 << " but patch bank only has "
			<< this->inst->getPatchCount() << " instruments.";
		throw EBadPatchType(ss.str());
	}
	OPLPatchPtr i = this->inst->getTypedPatch(ev->instrument);

	if (oplChannel > 8) {
		// Rhythm-mode instrument
		rhythm = oplChannel - 8;

		// Make sure the correct rhythm channel is in use
		if (i->rhythm != rhythm) {
			std::stringstream ss;
			ss << "Instrument type mismatch - patch was for "
				<< rhythmToText(i->rhythm) << " but channel can only be used for "
				<< rhythmToText(rhythm);
			//throw EChannelMismatch(ev->instrument, oplChannel, ss.str());
			std::cerr << "Warning: " << ss.str() << std::endl;
		}

		switch (rhythm) {
			case 1:
				oplChannel = 7;
				this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
				break; // mod, hihat
			case 2:
				oplChannel = 8;
				this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
				break; // car, topcym
			case 3:
				oplChannel = 8;
				this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
				break; // mod, tomtom
			case 4:
				oplChannel = 7;
				this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
				break; // car, snare
			case 5:
				oplChannel = 6;
				this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
				this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
				break; // both, bassdrum
		}

	} else { // normal instrument, write both operators
		if (i->rhythm != 0) {
			std::stringstream ss;
			ss << "Instrument type mismatch - patch was for rhythm instrument ("
				<< rhythmToText(i->rhythm) << ") but channel can only be used for "
					"normal (non-rhythm) instruments";
			throw EChannelMismatch(ev->instrument, oplChannel, ss.str());
		}
		this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
		this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
	}

	// Write the rest of the (global) instrument settings.
	/// @todo Figure out which rhythm instruments should be excluded
	if ((rhythm != 1) && (rhythm != 2)) {
		this->processNextPair(0, chipIndex, BASE_FEED_CONN | oplChannel,  ((i->feedback & 7) << 1) | (i->connection & 1));
	}
	// These next two affect the entire synth (all instruments/channels)
	//TODO curPatch->deepTremolo     =  this->oplState[BASE_RHYTHM] >> 7;
	//TODO curPatch->deepVibrato     = (this->oplState[BASE_RHYTHM] >> 6) & 1;

	if (rhythm == 0) { // normal instrument
		// Write lower eight bits of note freq
		this->processNextPair(0, chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

		// Write 0xB0 w/ keyon bit
		this->processNextPair(0, chipIndex, 0xB0 | oplChannel,
			OPLBIT_KEYON  // keyon enabled
			| (block << 2) // and the octave
			| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
		);
	} else { // rhythm instrument
		if ((rhythm != 2) && (rhythm != 1)) { // can't set hihat or cymbal freq
			// Write lower eight bits of note freq
			this->processNextPair(0, chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

			this->processNextPair(0, chipIndex, 0xB0 | oplChannel,
				//OPLBIT_KEYON  // different keyon for rhythm instruments
				(block << 2) // and the octave
				| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
			);
		}
		// Write keyon for correct instrument
		this->processNextPair(0, chipIndex, 0xBD,
			0x20 |  // enable percussion mode
			this->oplState[chipIndex][0xBD] |
			(1 << (rhythm-1))  // instrument-specific keyon
		);
	}

	this->lastTick = ev->absTime;
	return;
}

void EventConverter_OPL::handleEvent(const NoteOffEvent *ev)
	throw (stream::error)
{
	// TODO: These are just converting back the mappings we used when reading
	// files.  We'll need to implement some kind of proper channel map (probably
	// specified in the UI with the instrument map) so that arbitrary incoming
	// channels can be mapped to correct OPL instrument/rhythm channels.
	int oplChannel = (ev->channel - 1) % 14; // TODO: channel map for >9 chans
	int chipIndex = (ev->channel - 1) / 14; // TODO: calculate from channel map


	int delay = ev->absTime - this->lastTick;

	if (oplChannel > 8) {
		// Rhythm-mode instrument
		int rhythm = oplChannel - 8;
		this->processNextPair(delay, chipIndex, 0xBD,
			(this->oplState[chipIndex][0xBD] & ~(1 << (rhythm-1)))
		);
	} else {
		// Write 0xB0 w/ keyon bit disabled
		this->processNextPair(delay, chipIndex, 0xB0 | oplChannel,
			(this->oplState[chipIndex][0xB0 | oplChannel] & ~OPLBIT_KEYON)
		);
	}

	this->lastTick = ev->absTime;
	return;
}

void EventConverter_OPL::handleEvent(const PitchbendEvent *ev)
	throw (stream::error)
{
	unsigned int fnum, block;
	milliHertzToFnum(ev->milliHertz, &fnum, &block, this->fnumConversion);
	int oplChannel = (ev->channel - 1) % 9; // TODO: channel map for >9 chans
	int chipIndex = 0; // TODO: calculate from channel map

	int delay = ev->absTime - this->lastTick;

	// Write lower eight bits of note freq
	this->processNextPair(delay, chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

	// Write 0xB0 w/ keyon bit
	this->processNextPair(0, chipIndex, 0xB0 | oplChannel,
		OPLBIT_KEYON  // keyon enabled (wouldn't have a pitchbend with no note playing)
		| (block << 2) // and the octave
		| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
	);

	this->lastTick = ev->absTime;
	return;
}

void EventConverter_OPL::handleEvent(const ConfigurationEvent *ev)
	throw (stream::error)
{
	int delay = ev->absTime - this->lastTick;
	switch (ev->configType) {
		case ConfigurationEvent::EnableOPL3:
			this->processNextPair(delay, 0, 0x05, ev->value ? 0x01 : 0x00);
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			if (ev->value & 1) this->oplState[ev->value >> 1][0xBD] |= 0x80;
			else this->oplState[ev->value >> 1][0xBD] &= 0x7F;
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			if (ev->value & 1) this->oplState[ev->value >> 1][0xBD] |= 0x40;
			else this->oplState[ev->value >> 1][0xBD] &= 0xBF;
			break;
		case ConfigurationEvent::EnableRhythm:
			// Nothing required, rhythm mode is enabled the first type a rhythm
			// instrument is played.
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
	throw (stream::error)
{
	assert(chipIndex < 2);

	OPLEvent oplev;
	oplev.reg = reg;
	oplev.val = val;
	oplev.chipIndex = chipIndex;
	oplev.delay = delay + this->cachedDelay;
	this->cachedDelay = 0;

	this->cb->writeNextPair(&oplev);

	this->oplState[chipIndex][reg] = val;
	return;
}

void EventConverter_OPL::writeOpSettings(int chipIndex, int oplChannel,
	int opNum, OPLPatchPtr i, int velocity
)
	throw (stream::error)
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
			outputLevel = 0x3F - ((0x3F - outputLevel) * velocity / 255);
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
