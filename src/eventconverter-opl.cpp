/**
 * @file   eventconverter-opl.cpp
 * @brief  EventHandler implementation that produces OPL data from Events.
 *
 * Copyright (C) 2010-2015 Adam Nielsen <malvineous@shikadi.net>
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
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/util-opl.hpp>
#include <camoto/gamemusic/patch-midi.hpp>

using namespace camoto;
using namespace camoto::gamemusic;

EventConverter_OPL::EventConverter_OPL(OPLWriterCallback *cb,
	ConstMusicPtr music, double fnumConversion, OPLWriteFlags::type flags)
	:	cb(cb),
		music(music),
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

void EventConverter_OPL::setBankMIDI(PatchBankPtr bankMIDI)
{
	this->bankMIDI = bankMIDI;
	return;
}

void EventConverter_OPL::handleAllEvents(EventHandler::EventOrder eventOrder)
{
	this->EventHandler::handleAllEvents(eventOrder, this->music);

	// Write out any trailing delay
	OPLEvent oplev;
	oplev.valid = OPLEvent::Delay;
	oplev.delay = this->cachedDelay;
	this->cachedDelay = 0;
	this->cb->writeNextPair(&oplev);
}

void EventConverter_OPL::endOfTrack(unsigned long delay)
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
	this->cachedDelay += delay;
	OPLEvent oplev;
	oplev.valid = 0;
	if (this->cachedDelay) {
		oplev.valid |= OPLEvent::Delay;
		oplev.delay = this->cachedDelay;
		this->cachedDelay = 0;
	}
	oplev.valid |= OPLEvent::Tempo;
	oplev.tempo = ev->tempo;
	this->cb->writeNextPair(&oplev);
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	assert(this->music->patches);
	assert(trackIndex < this->music->trackInfo.size());

	// Set the delay as a cached delay, so that it will be written out before the
	// next register write, whenever that happens to be.
	this->cachedDelay += delay;

	if (ev->instrument >= this->music->patches->size()) {
		throw bad_patch(createString("Instrument bank too small - tried to play "
			"note with instrument #" << ev->instrument + 1
			<< " but patch bank only has " << this->music->patches->size()
			<< " instruments."));
	}
	PatchPtr patch = this->music->patches->at(ev->instrument);
	const TrackInfo& ti = this->music->trackInfo[trackIndex];
	if (this->bankMIDI) {
		// We are handling MIDI events
		if (
			(ti.channelType != TrackInfo::MIDIChannel)
			&& (ti.channelType != TrackInfo::AnyChannel)
		) {
			// Not a MIDI track
			return;
		}
		MIDIPatchPtr instMIDI = boost::dynamic_pointer_cast<MIDIPatch>(patch);
		if (!instMIDI) return; // non-MIDI instrument on a MIDI channel, ignore
		unsigned long target = instMIDI->midiPatch;
		if (instMIDI->percussion) {
			target += MIDI_PATCHES;
		}
		if (target < this->bankMIDI->size()) {
			patch = this->bankMIDI->at(target);
		} else {
			// No patch, bank too small
			std::cout << "Dropping MIDI note, no entry in MIDI bank for "
				<< (instMIDI->percussion ? "percussion" : "")
				<< " patch #" << (int)instMIDI->midiPatch << "\n";
			return;
		}
	} else {
		// We are handling OPL events
		if (
			(ti.channelType != TrackInfo::OPLChannel)
			&& (ti.channelType != TrackInfo::OPLPercChannel)
			&& (ti.channelType != TrackInfo::AnyChannel)
		) {
			// Not an OPL track
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
	}
	OPLPatchPtr inst = boost::dynamic_pointer_cast<OPLPatch>(patch);

	// Don't play this note if there's no patch for it
	if (!inst) return;

	unsigned int oplChannel, chipIndex;
	bool mod, car;
	this->getOPLChannel(ti, trackIndex, &oplChannel, &chipIndex, &mod, &car);

	// No free channels
	if (chipIndex == OPL_INVALID_CHIP) return;

	// If the note is already playing, turn it off first before we update the
	// instrument parameters.  This makes a clean note and prevents it from
	// audibly morphing into the new patch.
	if (ti.channelType == TrackInfo::OPLPercChannel) {
		int keyBit = 1 << ti.channelIndex;
		if (this->oplState[chipIndex][0xBD] & keyBit) {
			// Note is already playing, disable
			this->processNextPair(chipIndex, 0xBD,
				this->oplState[chipIndex][0xBD] ^ keyBit
			);
		}
	} else { // normal instrument
		if (this->oplState[chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON) {
			// Note already playing, switch it off first
			this->processNextPair(chipIndex, 0xB0 | oplChannel,
				(this->oplState[chipIndex][0xB0 | oplChannel] & ~OPLBIT_KEYON)
			);
		}
	}

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
	assert(block <= 7);

	// Write the rest of the (global) instrument settings.
	if (ti.channelType != TrackInfo::OPLPercChannel) {
		// This register is only used on melodic channels.  It is ignored on
		// percussive channels, so don't bother setting it.
		this->processNextPair(chipIndex, BASE_FEED_CONN | oplChannel,
			(this->modeOPL3 ? 0x30 /* L+R OPL3 panning */ : 0 /* regs aren't on OPL2 */)
			| ((inst->feedback & 7) << 1)
			| (inst->connection ? 1 : 0));
	}

	// Write lower eight bits of note freq
	this->processNextPair(chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

	// Write 0xB0 w/ keyon bit
	this->processNextPair(chipIndex, 0xB0 | oplChannel,
		(ti.channelType != TrackInfo::OPLPercChannel ? OPLBIT_KEYON : 0) // keyon enabled
		| (block << 2) // and the octave
		| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
	);

	if (ti.channelType == TrackInfo::OPLPercChannel) {
		// Write keyon for correct instrument
		int keyBit = 1 << ti.channelIndex;
		this->processNextPair(chipIndex, 0xBD,
			0x20 |  // enable percussion mode
			this->oplState[chipIndex][0xBD] | keyBit
		);
	}
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->cachedDelay += delay;

	assert(trackIndex < this->music->trackInfo.size());
	const TrackInfo& ti = this->music->trackInfo[trackIndex];
	if (
		(ti.channelType != TrackInfo::OPLChannel)
		&& (ti.channelType != TrackInfo::OPLPercChannel)
		&& (ti.channelType != TrackInfo::AnyChannel)
		&& (
			!this->bankMIDI
			|| (ti.channelType != TrackInfo::MIDIChannel)
		)
	) {
		// This isn't an OPL track, so ignore it
		return;
	}

	if (ti.channelType == TrackInfo::OPLPercChannel) {
		int keyBit = 1 << ti.channelIndex; // instrument-specific keyon
		this->processNextPair(0, 0xBD,
			(this->oplState[0][0xBD] & ~keyBit)
		);
	} else {
		unsigned int oplChannel, chipIndex;
		bool mod, car;
		this->getOPLChannel(ti, trackIndex, &oplChannel, &chipIndex, &mod, &car);

		// No free channels
		if (chipIndex == OPL_INVALID_CHIP) return;

		// Write 0xB0 w/ keyon bit disabled
		this->processNextPair(chipIndex, 0xB0 | oplChannel,
			(this->oplState[chipIndex][0xB0 | oplChannel] & ~OPLBIT_KEYON)
		);

		this->clearOPLChannel(trackIndex);
	}
	return;
}

void EventConverter_OPL::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const EffectEvent *ev)
{
	this->cachedDelay += delay;

	assert(trackIndex < this->music->trackInfo.size());
	const TrackInfo& ti = this->music->trackInfo[trackIndex];
	if (
		(ti.channelType != TrackInfo::OPLChannel)
		&& (ti.channelType != TrackInfo::OPLPercChannel)
		&& (ti.channelType != TrackInfo::AnyChannel)
		&& (
			!this->bankMIDI
			|| (ti.channelType != TrackInfo::MIDIChannel)
		)
	) {
		// This isn't an OPL track, so ignore it
		return;
	}

	unsigned int oplChannel, chipIndex;
	bool mod, car;
	this->getOPLChannel(ti, trackIndex, &oplChannel, &chipIndex, &mod, &car);

	// No free channels
	if (chipIndex == OPL_INVALID_CHIP) return;

	switch (ev->type) {
		case EffectEvent::PitchbendNote: {
			// Just bend the whole channel because there will only be one note
			// playing anyway, and the bend will be reset on the next note.
			const unsigned int& milliHertz = ev->data;
			unsigned int fnum, block;
			milliHertzToFnum(milliHertz, &fnum, &block, this->fnumConversion);

			// Write lower eight bits of note freq
			this->processNextPair(chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

			// Write 0xB0 w/ keyon bit
			this->processNextPair(chipIndex, 0xB0 | oplChannel,
				// Use whatever keyon bit was set before - this will prevent rhythm
				// instruments from having the keyon bit set erroneously
				(this->oplState[chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON)
				| (block << 2) // and the octave
				| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
			);
			break;
		}
		case EffectEvent::Volume:
			// ev->data is new velocity/volume value
			// Write carrier settings (modulator output level does not control volume)
			if (car) {
				const unsigned int& volume = ev->data;
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

	assert(trackIndex < this->music->trackInfo.size());
	const TrackInfo& ti = this->music->trackInfo[trackIndex];
	if (
		(ti.channelType != TrackInfo::OPLChannel)
		&& (ti.channelType != TrackInfo::OPLPercChannel)
		&& (ti.channelType != TrackInfo::AnyChannel)
		&& (
			!this->bankMIDI
			|| (ti.channelType != TrackInfo::MIDIChannel)
		)
	) {
		// This isn't an OPL track, so ignore it
		return;
	}

	switch (ev->configType) {
		case ConfigurationEvent::EmptyEvent:
			break;
		case ConfigurationEvent::EnableOPL3:
			if (
				(this->modeOPL3 && (ev->value == 0)) // on, switching off
				|| (!this->modeOPL3 && (ev->value == 1)) // off, switching on
			) {
				this->processNextPair(1, 0x05, ev->value ? 0x01 : 0x00);
				this->modeOPL3 = ev->value;
			}
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
	oplev.valid = OPLEvent::Delay | OPLEvent::Regs;
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
		// Note that modulator-only percussive instruments cannot have their volume
		// set, so we don't need to handle that here
	} else {
		op = OPLOFFSET_CAR(oplChannel);
		o = &(i->c);
		outputLevel = o->outputLevel;
		if (velocity != DefaultVelocity) {
			// Not using default velocity
			outputLevel = 63 - lin_velocity_to_log_volume(velocity, 63);
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

void EventConverter_OPL::getOPLChannel(const TrackInfo& ti,
	unsigned int trackIndex, unsigned int *oplChannel, unsigned int *chipIndex,
	bool *mod, bool *car)
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
			default:
				throw error(createString("OPL percussion channel out of range: "
					<< (int)ti.channelIndex << " is not in 0 <= x <= 4"));
		}
	} else if (ti.channelType == TrackInfo::OPLChannel) {
		if ((ti.channelIndex == 0) && (this->flags & OPLWriteFlags::ReserveFirstChan)) {
			throw format_limitation("OPL channel 0 cannot be used in this format.  "
				"Please select a different channel.");
		}
		if (ti.channelIndex < 9) {
			*oplChannel = ti.channelIndex;
			*chipIndex = 0;
		} else if ((ti.channelIndex < 18) && !(this->flags & OPLWriteFlags::OPL2Only)) {
			*oplChannel = ti.channelIndex - 9;
			*chipIndex = 1;
		} else {
			throw error(createString("OPL channel " << ti.channelIndex
				<< " is out of range!  This format only supports up to and including "
				"channel " << ((this->flags & OPLWriteFlags::OPL2Only) ? 8 : 17)));
		}
		*mod = true;
		*car = true;
	} else if (ti.channelType == TrackInfo::MIDIChannel) {
		*mod = true;
		*car = true;
		int rawChannel = -1;
		MIDIChannelMap::iterator i = this->midiChannelMap.find(trackIndex);
		if (i != this->midiChannelMap.end()) {
			// Reuse a previous mapping
			rawChannel = i->second;
		} else {
			// Create a new mapping
			bool channelInUse[OPL_MAX_CHANNELS];
			for (unsigned int j = 0; j < OPL_MAX_CHANNELS; j++) channelInUse[j] = false;
			for (MIDIChannelMap::const_iterator
				j = this->midiChannelMap.begin(); j != this->midiChannelMap.end(); j++
			) {
				assert(j->second < (signed int)OPL_MAX_CHANNELS);
				if (j->second >= 0) channelInUse[j->second] = true;
			}
			for (unsigned int j = 0; j < OPL_MAX_CHANNELS; j++) {
				if (!channelInUse[j]) {
					// This is the channel to use
					rawChannel = j;
					this->midiChannelMap[trackIndex] = rawChannel;
					break;
				}
			}
			// This might set the map to -1 if there aren't enough free channels
			if (rawChannel == -1) std::cout << "OPL: All 18 channels are in use for MIDI, dropping a note.\n";
		}
		if (rawChannel < 0) {
			*chipIndex = OPL_INVALID_CHIP;
			*oplChannel = 0;
		} else if (rawChannel >= 9) {
			*chipIndex = 1;
			*oplChannel = rawChannel - 9;
		} else {
			*chipIndex = 0;
			*oplChannel = rawChannel;
		}
	}
	// Should never get this far
	assert(*oplChannel != 99);
	return;
}

void EventConverter_OPL::clearOPLChannel(unsigned int trackIndex)
{
	MIDIChannelMap::iterator i = this->midiChannelMap.find(trackIndex);
	if (i != this->midiChannelMap.end()) {
		this->midiChannelMap.erase(i);
	}
	return;
}
