/**
 * @file   eventconverter-midi.cpp
 * @brief  EventConverter implementation that produces MIDI events from Events.
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

#include <math.h> // pow
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/patch-opl.hpp>

#define log2(x) (log(x) / 0.30102999566398119521373889472449)
#define round(x) (int)((x) + 0.5)

using namespace camoto;
using namespace camoto::gamemusic;

unsigned long camoto::gamemusic::midiToFreq(double midi)
{
	return 440000 * pow(2, (midi - 69.0) / 12.0);
}

void camoto::gamemusic::freqToMIDI(unsigned long milliHertz, uint8_t *note,
	int16_t *bend, uint8_t curNote)
{
	// Lower bound is clamped to MIDI note #0.  Could probably get lower with a
	// pitchbend but the difference is unlikely to be audible (8Hz is pretty much
	// below human hearing anyway.)
	if (milliHertz <= 8175) {
		*note = 0;
		*bend = 0;
		return;
	}
	double val = 12.0 * log2((double)milliHertz / 440000.0) + 69.0;
	// round to three decimal places
	val = round(val * 1000.0) / 1000.0;
	if (curNote == 0xFF) *note = round(val);
	else *note = curNote;
	*bend = (val - *note) * 4096;

	// If the pitchbend is out of range, just clamp it
	if (*bend < -8192) *bend = -8192;
	if (*bend > 8191) *bend = 8191;

	if (*note > 0x7F) {
		std::cout << "Error: Frequency " << (double)milliHertz / 1000 << " is too "
			"high (requires MIDI note " << (int)*note << ")" << std::endl;
		*note = 0x7F;
	}
	/// @todo Take into account current range of pitchbend and allow user to
	/// extend it to prevent clamping.

	/// @todo Figure out if this is a currently playing note, and try to maintain
	/// the bend to avoid playing a new note.  Maybe only necessary in the
	/// pitchbend callback rather than the noteon one.
	return;
}


EventConverter_MIDI::EventConverter_MIDI(MIDIEventCallback *cb,
	const PatchBankPtr patches, unsigned int midiFlags,
	unsigned long ticksPerQuarterNote)
	:	cb(cb),
		patches(patches),
		midiFlags(midiFlags),
		lastTick(0),
		ticksPerQuarterNote(ticksPerQuarterNote),
		usPerTick(0),
		deepTremolo(false),
		deepVibrato(false),
		updateDeep(false),
		first_usPerTick(MIDI_DEF_uS_PER_TICK)
{
	memset(this->currentPatch, 0xFF, sizeof(this->currentPatch));
	memset(this->currentInternalPatch, 0xFF, sizeof(this->currentInternalPatch));
	memset(this->currentPitchbend, 0, sizeof(this->currentPitchbend));
	memset(this->activeNote, 0xFF, sizeof(this->activeNote));
	memset(this->lastEvent, 0xFF, sizeof(this->lastEvent));
	memset(this->channelMap, 0xFF, sizeof(this->channelMap));
}

EventConverter_MIDI::~EventConverter_MIDI()
{
}

void EventConverter_MIDI::rewind()
{
	this->lastTick = 0;
	return;
}

void EventConverter_MIDI::handleEvent(const TempoEvent *ev)
{
	assert(ev->usPerTick > 0);
	if (this->usPerTick != ev->usPerTick) {
		if (!(this->midiFlags & MIDIFlags::BasicMIDIOnly)) {
			this->cb->midiSetTempo(ev->absTime - this->lastTick, ev->usPerTick);
		} else {
			std::cerr << "TODO: Adjust tempo by changing tick multiplier" << std::endl;
		}
		this->usPerTick = ev->usPerTick;
	}
	if (ev->absTime == 0) this->first_usPerTick = ev->usPerTick;
	return;
}

void EventConverter_MIDI::handleEvent(const NoteOnEvent *ev)
{
	// Can't do a note-on for all channels
	assert(ev->channel != 0);

	/// @todo If instrument is rhythm use channel 10, with override func so CMF
	/// can pick other channels based on value
	uint32_t delay = ev->absTime - this->lastTick;
	//this->writeMIDINumber(delay);

	if (this->updateDeep) {
		// Need to set CMF controller 0x63
		uint8_t val = (this->deepTremolo ? 1 : 0) | (this->deepVibrato ? 1 : 0);
		this->cb->midiController(delay, 0, 0x63, val);
		delay = 0;
		this->updateDeep = false;
	}

	// If we've got MIDI patches, make sure any percussion instruments end up on
	// channel 10.
	uint8_t midiChannel = 0xFF, note;

	// No notes if there are no patches.
	if (!this->patches) return;

	assert(ev->instrument < this->patches->size());
	MIDIPatchPtr patch = boost::dynamic_pointer_cast<MIDIPatch>(this->patches->at(ev->instrument));
	OPLPatchPtr oplPatch;
	if (!patch) {
		oplPatch = boost::dynamic_pointer_cast<OPLPatch>(this->patches->at(ev->instrument));
	}

	// If we're only supposed to play MIDI notes then don't play this note if
	// there's no MIDI patch.
	if (!patch && (this->midiFlags & MIDIFlags::MIDIPatchesOnly)) return;

	// Got a MIDI patch
	if (patch && patch->percussion) {
		midiChannel = 9;
		note = patch->midiPatch;
		this->channelMap[ev->channel] = 9;
	} else {
		if (oplPatch && oplPatch->rhythm) {
			// Special case for CMF files
			midiChannel = 16 - oplPatch->rhythm;
			this->channelMap[ev->channel] = midiChannel;
		} else {
			if ((this->channelMap[ev->channel] == 9) && ((this->midiFlags & MIDIFlags::Channel10NoPerc) == 0)) {
				// This channel is allocated to percussion, but this note is not a
				// percussive note, so clear the mapping
				this->channelMap[ev->channel] = 0xFF;
			}
			midiChannel = this->getMIDIchannel(ev->channel, MIDI_CHANNELS);
		}
		int16_t bend;
		freqToMIDI(ev->milliHertz, &note, &bend, 0xFF);
		if (!(this->midiFlags & MIDIFlags::IntegerNotesOnly)) {
			if (bend != this->currentPitchbend[midiChannel]) {
				bend += 8192;
				this->cb->midiPitchbend(delay, midiChannel, bend);
				delay = 0;
				this->currentPitchbend[midiChannel] = bend;
			}
		}
		unsigned int newPatch;
		if (patch) {
			// Got a patch bank, figure out which MIDI patch we need
			newPatch = patch->midiPatch;
		} else {
			// Don't have a patch bank, use the incoming instrument number
			newPatch = ev->instrument;
		}
		if (newPatch != this->currentPatch[midiChannel]) {
			assert(newPatch < MIDI_PATCHES);
			// Instrument has changed
			this->cb->midiPatchChange(delay, midiChannel, newPatch);
			delay = 0;
			this->currentPatch[midiChannel] = newPatch;
		}
	}
	assert(midiChannel != 0xFF);

	// Use a default velocity if none was specified, otherwise squish it into
	// a 7-bit value.
	uint8_t velocity =
		(ev->velocity == 0) ? MIDI_DEFAULT_ATTACK_VELOCITY : (ev->velocity >> 1);

	this->cb->midiNoteOn(delay, midiChannel, note, velocity);

	this->activeNote[ev->channel] = note;

	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void EventConverter_MIDI::handleEvent(const NoteOffEvent *ev)
{
	// Can't (yet?) handle a note-off on all channels
	assert(ev->channel != 0);

	// No need to use the mapping logic here as a channel must always be mapped,
	// otherwise we will be processing a note-off with no associated note-on.
	uint8_t midiChannel = this->channelMap[ev->channel];
	if (midiChannel == 0xFF) return; // unmapped channel

	if (this->activeNote[ev->channel] != 0xFF) {
		uint32_t delay = ev->absTime - this->lastTick;
		this->cb->midiNoteOff(delay, midiChannel, this->activeNote[ev->channel],
			MIDI_DEFAULT_RELEASE_VELOCITY);
		this->activeNote[ev->channel] = 0xFF;
	} else {
		std::cerr << "Warning: Got note-off event for channel #" << ev->channel
			<< " but there was no note playing!" << std::endl;
	}

	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void EventConverter_MIDI::handleEvent(const PitchbendEvent *ev)
{
	if (this->midiFlags & MIDIFlags::IntegerNotesOnly) return;

	uint8_t midiChannel = this->getMIDIchannel(ev->channel, MIDI_CHANNELS);
	if (midiChannel == 0xFF) return; // unmapped channel

	uint8_t note;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &note, &bend, this->activeNote[ev->channel]);
	/// @todo Handle pitch bending further than one note or if 'note' comes out
	/// differently to the currently playing note
	if (bend != this->currentPitchbend[midiChannel]) {
		uint32_t delay = ev->absTime - this->lastTick;
		bend += 8192;
		this->cb->midiPitchbend(delay, midiChannel, bend);
		this->currentPitchbend[midiChannel] = bend;
	}
	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void EventConverter_MIDI::handleEvent(const ConfigurationEvent *ev)
{
	uint32_t delay = ev->absTime - this->lastTick;

	switch (ev->configType) {
		case ConfigurationEvent::EnableRhythm:
			// Controller 0x67 (CMF rhythm)
			this->cb->midiController(delay, 0, 0x67, ev->value);
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			this->deepTremolo = ev->value;
			this->updateDeep = true;
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			this->deepVibrato = ev->value;
			this->updateDeep = true;
			break;
		default:
			break;
	}
	this->lastTick = ev->absTime;
	return;
}


uint8_t EventConverter_MIDI::getMIDIchannel(unsigned int channel, unsigned int numMIDIchans)
{
	assert(channel < MAX_CHANNELS);
	assert(numMIDIchans <= MIDI_CHANNELS);

	if (this->channelMap[channel] == 0xFF) {
		// This is the first event on this channel, try to find a spare space
		bool available[MIDI_CHANNELS];
		for (unsigned int m = 0; m < numMIDIchans; m++) available[m] = true;
		for (unsigned int o = 0; o < MAX_CHANNELS; o++) {
			if (this->channelMap[o] != 0xFF) {
				available[this->channelMap[o]] = false;
			}
		}
		bool mapped = false;
		unsigned long earliestTime = this->lastEvent[0];
		uint8_t earliestTimeChannel = 0;
		for (unsigned int m = 0; m < numMIDIchans; m++) {
			if ((m == 9) && ((this->midiFlags & MIDIFlags::Channel10NoPerc) == 0)) {
				m++; // skip percussion channel
			}
			if (available[m]) {
				this->channelMap[channel] = m;
				mapped = true;
				break;
			}
			// While we're here, find the channel with the longest time since the
			// last event, in case all channels are in use.
			if (this->lastEvent[m] < earliestTime) {
				earliestTime = this->lastEvent[m];
				earliestTimeChannel = m;
			}
		}
		if (!mapped) {
			// All MIDI channels are in use!  Use the one that has gone the longest
			// with no instruments playing.
			this->channelMap[channel] = earliestTimeChannel;
		}
	}
	return this->channelMap[channel];
}
