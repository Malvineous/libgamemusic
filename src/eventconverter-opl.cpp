/**
 * @file   eventconverter-opl.cpp
 * @brief  EventHandler implementation that produces OPL data from Events.
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
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
#include <camoto/error.hpp>
#include <camoto/util.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/gamemusic/opl-util.hpp>

using namespace camoto;
using namespace camoto::gamemusic;

/// Figure out which OPL channel and operator(s) to use for a track.
void getOPLChannel(const TrackInfo& ti, unsigned int *oplChannel,
	unsigned int *chipIndex, bool *mod, bool *car)
{
	*oplChannel = 99;
	if (ti.channelType == TrackInfo::OPLPercChannel) {
		*chipIndex = 0;
		switch (ti.channelIndex) {
			case 4: *oplChannel = 6; *mod = true;  *car = true;  break; // bass drum (mod+car)
			case 3: *oplChannel = 7; *mod = false; *car = true;  break; // snare (car)
			case 2: *oplChannel = 8; *mod = true;  *car = false; break; // tomtom (mod)
			case 1: *oplChannel = 8; *mod = false; *car = true;  break; // top cymbal (car)
			case 0: *oplChannel = 7; *mod = true;  *car = false; break; // hi-hat (mod)
		}
	} else { // OPLChannel
		if (ti.channelIndex < 9) {
			*oplChannel = ti.channelIndex;
			*chipIndex = 0;
		} else if (ti.channelIndex < 18) {
			*oplChannel = ti.channelIndex - 9;
			*chipIndex = 1;
		} else {
			throw error(createString("OPL channel " << ti.channelIndex
				<< " is out of range!"));
		}
		*mod = true;
		*car = true;
	}
	assert(*oplChannel != 99);
	return;
}

EventConverter_OPL::EventConverter_OPL(OPLWriterCallback *cb,
	const TrackInfoVector *trackInfo, const PatchBankPtr& patches,
	double fnumConversion, unsigned int flags)
	:	cb(cb),
		trackInfo(trackInfo),
		patches(patches),
		fnumConversion(fnumConversion),
		flags(flags),
		cachedDelay(0),
		modeOPL3(false),
		modeRhythm(false)
{
	// Initialise all OPL registers to zero (this is assumed the initial state
	// upon playback)
	memset(this->oplSet, 0x00, sizeof(this->oplSet));
	memset(this->oplState, 0x00, sizeof(this->oplState));
	assert(this->oplSet[0][0] == false);
}

EventConverter_OPL::~EventConverter_OPL()
{
}

void EventConverter_OPL::rewind()
{
	return;
}

void EventConverter_OPL::endOfPattern(unsigned long delay)
{
	this->cachedDelay += delay;
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	assert(ev->tempo.usPerTick > 0);
	this->cb->tempoChange(ev->tempo);
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	assert(this->patches);
	assert(trackIndex < this->trackInfo->size());
	const TrackInfo& ti = this->trackInfo->at(trackIndex);
	if (
		(ti.channelType != TrackInfo::OPLChannel)
		&& (ti.channelType != TrackInfo::OPLPercChannel)
	) {
		// This isn't an OPL track, so ignore it
		return;
	}
	if (
		(ti.channelType == TrackInfo::OPLPercChannel)
		&& (!this->modeRhythm)
	) {
		std::cerr << "OPL: Ignoring rhythm channel in non-rhythm mode" << std::endl;
		return;
	}
	if (
		(ti.channelType == TrackInfo::OPLChannel)
		&& (!this->modeOPL3)
		&& (ti.channelIndex >= 9)
	) {
		std::cerr << "OPL: Ignoring OPL3 channels in OPL2 mode" << std::endl;
		return;
	}

	// Set the delay as a cached delay, so that it will be written out before the
	// next register write, whenever that happens to be.
	this->cachedDelay += delay;

	// See if the instrument is already set
	if (ev->instrument >= this->patches->size()) {
		throw bad_patch(createString("Instrument bank too small - tried to play "
			"note with instrument #" << ev->instrument + 1
			<< " but patch bank only has " << this->patches->size()
			<< " instruments."));
	}
	OPLPatchPtr inst = boost::dynamic_pointer_cast<OPLPatch>(this->patches->at(ev->instrument));

	// Don't play this note if there's no patch for it
	if (!inst) return;

	unsigned int oplChannel, chipIndex;
	bool mod, car;
	getOPLChannel(ti, &oplChannel, &chipIndex, &mod, &car);

	// We always have to set the patch in case the velocity has changed.
	// Duplicate register writes will be dropped later.

	// Write modulator settings
	if (mod) {
		this->writeOpSettings(chipIndex, oplChannel, 0, inst, ev->velocity);
	}
	// Write carrier settings
	if (car) {
		this->writeOpSettings(chipIndex, oplChannel, 1, inst, ev->velocity);
	}

	unsigned int fnum, block;
	milliHertzToFnum(ev->milliHertz, &fnum, &block, this->fnumConversion);

	// Write the rest of the (global) instrument settings.
	if (
		(
			(ti.channelType == TrackInfo::OPLPercChannel)
			&& (ti.channelIndex != 2)
			&& (ti.channelIndex != 4)
		) || (ti.channelType == TrackInfo::OPLChannel)
	) {
		this->processNextPair(chipIndex, BASE_FEED_CONN | oplChannel,
			0x30 /* L+R OPL3 panning */
			| ((inst->feedback & 7) << 1)
			| (inst->connection & 1));
	}

	if (ti.channelType == TrackInfo::OPLPercChannel) {
		// Only set the freq if it's not a hihat or snare, as these instruments
		// apparently can't have their frequency set.
		//if ((ti.channelIndex != 2) && (ti.channelIndex != 4)) {
			// Write lower eight bits of note freq
			this->processNextPair(chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

			this->processNextPair(chipIndex, 0xB0 | oplChannel,
				//OPLBIT_KEYON  // different keyon for rhythm instruments
				(block << 2) // and the octave
				| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
			);
		//}

		int keyBit = 1 << ti.channelIndex; // instrument-specific keyon
		if (this->oplState[chipIndex][0xBD] & keyBit) {
			// Note is already playing, disable
			this->processNextPair(chipIndex, 0xBD,
				this->oplState[chipIndex][0xBD] ^ keyBit
			);
		}
		// Write keyon for correct instrument
		this->processNextPair(chipIndex, 0xBD,
			0x20 |  // enable percussion mode
			this->oplState[chipIndex][0xBD] | keyBit
		);
	} else { // normal instrument
		if (this->oplState[chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON) {
			// Note already playing, switch it off first
			this->processNextPair(chipIndex, 0xB0 | oplChannel,
				(this->oplState[chipIndex][0xB0 | oplChannel] & ~OPLBIT_KEYON)
			);
		}
		// Write lower eight bits of note freq
		this->processNextPair(chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

		// Write 0xB0 w/ keyon bit
		this->processNextPair(chipIndex, 0xB0 | oplChannel,
			OPLBIT_KEYON  // keyon enabled
			| (block << 2) // and the octave
			| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
		);
	}
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	assert(trackIndex < this->trackInfo->size());
	const TrackInfo& ti = this->trackInfo->at(trackIndex);
	if (
		(ti.channelType != TrackInfo::OPLChannel)
		&& (ti.channelType != TrackInfo::OPLPercChannel)
	) {
		// This isn't an OPL track, so ignore it
		return;
	}
	this->cachedDelay += delay;

	if (ti.channelType == TrackInfo::OPLPercChannel) {
		int keyBit = 1 << ti.channelIndex; // instrument-specific keyon
		this->processNextPair(0, 0xBD,
			(this->oplState[0][0xBD] & ~keyBit)
		);
	} else {
		unsigned int oplChannel, chipIndex;
		if (ti.channelIndex < 9) {
			oplChannel = ti.channelIndex;
			chipIndex = 0;
		} else if (ti.channelIndex < 18) {
			oplChannel = ti.channelIndex - 9;
			chipIndex = 1;
		} else {
			std::cerr << "OPL channel " << ti.channelIndex << " is out of range!"
				<< std::endl;
			return;
		}

		// Write 0xB0 w/ keyon bit disabled
		this->processNextPair(chipIndex, 0xB0 | oplChannel,
			(this->oplState[chipIndex][0xB0 | oplChannel] & ~OPLBIT_KEYON)
		);
	}
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const EffectEvent *ev)
{
	assert(trackIndex < this->trackInfo->size());
	const TrackInfo& ti = this->trackInfo->at(trackIndex);
	if (
		(ti.channelType != TrackInfo::OPLChannel)
		&& (ti.channelType != TrackInfo::OPLPercChannel)
	) {
		// This isn't an OPL track, so ignore it
		return;
	}

	unsigned int oplChannel, chipIndex;
	bool mod, car;
	getOPLChannel(ti, &oplChannel, &chipIndex, &mod, &car);

	this->cachedDelay += delay;

	switch (ev->type) {
		case EffectEvent::Pitchbend: {
			const unsigned int& milliHertz = ev->data;
			unsigned int fnum, block;
			milliHertzToFnum(milliHertz, &fnum, &block, this->fnumConversion);

			// Write lower eight bits of note freq
			this->processNextPair(chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

			// Write 0xB0 w/ keyon bit
			this->processNextPair(chipIndex, 0xB0 | oplChannel,
				OPLBIT_KEYON  // keyon enabled (wouldn't have a pitchbend with no note playing)
				| (block << 2) // and the octave
				| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
			);
			break;
		}
		case EffectEvent::Volume:
			// ev->data is new velocity/volume value
			// Write modulator settings
			if (mod) {
				//this->writeOpSettings(chipIndex, oplChannel, 0, inst, ev->data);
//std::cerr << "TODO: Set volume for mod instrument\n";
			}
			// Write carrier settings
			if (car) {
				const unsigned int& volume = ev->data;
				//this->writeOpSettings(chipIndex, oplChannel, 1, inst, ev->data);
				unsigned int op = OPLOFFSET_CAR(oplChannel);
				unsigned int outputLevel = 0x3F - 0x3F * log((float)volume) / log(256.0);

				uint8_t reg = BASE_SCAL_LEVL | op;
				uint8_t val = this->oplState[chipIndex][reg] & ~0x3F;
				this->processNextPair(chipIndex, reg, val | (outputLevel & 0x3F));
			}
			break;
	}
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->cachedDelay += delay;
	switch (ev->configType) {
		case ConfigurationEvent::None:
			break;
		case ConfigurationEvent::EnableOPL3:
			this->processNextPair(1, 0x05, ev->value ? 0x01 : 0x00);
			this->modeOPL3 = ev->value;
			break;
		case ConfigurationEvent::EnableDeepTremolo: {
			unsigned int chipIndex = (ev->value >> 1) & 1;
			uint8_t val;
			if (ev->value & 1) val = this->oplState[chipIndex][0xBD] | 0x80;
			else val = this->oplState[chipIndex][0xBD] & 0x7F;
			this->processNextPair(chipIndex, 0xBD, val);
			break;
		}
		case ConfigurationEvent::EnableDeepVibrato: {
			unsigned int chipIndex = (ev->value >> 1) & 1;
			uint8_t val;
			if (ev->value & 1) val = this->oplState[chipIndex][0xBD] | 0x40;
			else val = this->oplState[chipIndex][0xBD] & 0xBF;
			this->processNextPair(chipIndex, 0xBD, val);
			break;
		}
		case ConfigurationEvent::EnableRhythm:
			// Nothing required, rhythm mode is enabled the first time a rhythm
			// instrument is played.
			if (this->modeRhythm && (ev->value == 0)) {
				// Switching rhythm mode off (incl all rhythm instruments)
				this->processNextPair(0, 0xBD,
					this->oplState[0][0xBD] & ~0x3F
				);
				this->processNextPair(1, 0xBD,
					this->oplState[1][0xBD] & ~0x3F
				);
			}
			this->modeRhythm = ev->value;
			break;
		case ConfigurationEvent::EnableWaveSel:
			this->processNextPair(0, 0x01, ev->value ? 0x20 : 0x00);
			break;
	}
	return;
}

void EventConverter_OPL::processNextPair(uint8_t chipIndex, uint8_t reg,
	uint8_t val)
{
	assert(chipIndex < 2);

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
	int opNum, OPLPatchPtr i, int velocity)
{
	uint8_t op;
	OPLOperator *o;
	int outputLevel;
	if (opNum == 0) {
		op = OPLOFFSET_MOD(oplChannel);
		o = &(i->m);
		outputLevel = o->outputLevel;
		// @todo: If this is a mod only perc inst, should we set velocity here?
	} else {
		op = OPLOFFSET_CAR(oplChannel);
		o = &(i->c);
		outputLevel = o->outputLevel;
		if (velocity != -1) {
			// Not using default velocity
			outputLevel = 0x3F - 0x3F * log((float)velocity) / log(256.0);
			// Note the CMF reader sets the velocity to -1 to skip this
			// @todo: Use a flag: inst output val is max note vel, or note vel overrides inst output val
		}
	}

	this->processNextPair(chipIndex, BASE_CHAR_MULT | op,
		((o->enableTremolo & 1) << 7) |
		((o->enableVibrato & 1) << 6) |
		((o->enableSustain & 1) << 5) |
		((o->enableKSR     & 1) << 4) |
		 (o->freqMult      & 0x0F)
	);
	this->processNextPair(chipIndex, BASE_SCAL_LEVL | op, (o->scaleLevel << 6) | (outputLevel & 0x3F));
	this->processNextPair(chipIndex, BASE_ATCK_DCAY | op, (o->attackRate << 4) | (o->decayRate & 0x0F));
	this->processNextPair(chipIndex, BASE_SUST_RLSE | op, (o->sustainRate << 4) | (o->releaseRate & 0x0F));
	this->processNextPair(chipIndex, BASE_WAVE      | op,  o->waveSelect & 7);

	return;
}
