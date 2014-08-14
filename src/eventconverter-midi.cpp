/**
 * @file   eventconverter-midi.cpp
 * @brief  EventConverter implementation that produces MIDI events from Events.
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

#include <math.h> // pow
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/patch-opl.hpp>

#define LOG_2 0.69314718055994530941
#define log2(x) (log(x) / LOG_2)
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
	ConstMusicPtr music, unsigned int midiFlags)
	:	cb(cb),
		music(music),
		midiFlags(midiFlags),
		usPerTick(0),
		cachedDelay(0),
		deepTremolo(false),
		deepVibrato(false),
		updateDeep(false)
{
	memset(this->currentPatch, 0xFF, sizeof(this->currentPatch));
	memset(this->currentInternalPatch, 0xFF, sizeof(this->currentInternalPatch));
	memset(this->currentPitchbend, 0, sizeof(this->currentPitchbend));
	memset(this->activeNote, ACTIVE_NOTE_NONE, sizeof(this->activeNote));
	memset(this->lastEvent, 0xFF, sizeof(this->lastEvent));
	memset(this->channelMap, 0xFF, sizeof(this->channelMap));
}

EventConverter_MIDI::~EventConverter_MIDI()
{
}

void EventConverter_MIDI::endOfTrack(unsigned long delay)
{
	return;
}

void EventConverter_MIDI::endOfPattern(unsigned long delay)
{
	this->cachedDelay += delay;
	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->cachedDelay += delay;
	if (this->usPerTick != ev->tempo.usPerTick) {
		if (!(this->midiFlags & MIDIFlags::BasicMIDIOnly)) {
			this->cb->midiSetTempo(this->cachedDelay, ev->tempo);
			this->cachedDelay = 0;
		} else {
			std::cerr << "TODO: Adjust tempo by changing tick multiplier" << std::endl;
		}
		this->usPerTick = ev->tempo.usPerTick;
	}
	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	const TrackInfo& trackInfo = this->music->trackInfo[trackIndex];
	if (
		!(this->midiFlags & MIDIFlags::UsePatchIndex) &&
		(trackInfo.channelType != TrackInfo::MIDIChannel)
	) return;
	const unsigned int& midiChannel = trackInfo.channelIndex;
	this->cachedDelay += delay;

	// Figure out which MIDI instrument number to use
	unsigned int targetPatch;
	assert(ev->instrument < this->music->patches->size());
	if (this->midiFlags & MIDIFlags::UsePatchIndex) {
		targetPatch = ev->instrument;
	} else {
		MIDIPatchPtr patch =
			boost::dynamic_pointer_cast<MIDIPatch>(this->music->patches->at(ev->instrument));
		if (!patch) return;
		targetPatch = patch->midiPatch;
	}

	/// @todo If instrument is rhythm use channel 10, with override func so CMF
	/// can pick other channels based on value

	// Batch these two changes together so they can happen in one event
	if (this->updateDeep) {
		// Need to set CMF controller 0x63
		uint8_t val = (this->deepTremolo ? 1 : 0) | (this->deepVibrato ? 1 : 0);
		this->cb->midiController(this->cachedDelay, 0, 0x63, val);
		this->cachedDelay = 0;
		this->updateDeep = false;
	}

	// If we've got MIDI patches, make sure any percussion instruments end up on
	// channel 10.

	if (targetPatch != this->currentPatch[midiChannel]) {
		assert(targetPatch < MIDI_PATCHES);
		// Instrument has changed
		this->cb->midiPatchChange(this->cachedDelay, midiChannel, targetPatch);
		this->cachedDelay = 0;
		this->currentPatch[midiChannel] = targetPatch;
	}

	uint8_t note;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &note, &bend, 0xFF);
	if (!(this->midiFlags & MIDIFlags::IntegerNotesOnly)) {
		if (bend != this->currentPitchbend[midiChannel]) {
			bend += 8192;
			this->cb->midiPitchbend(this->cachedDelay, midiChannel, bend);
			this->cachedDelay = 0;
			this->currentPitchbend[midiChannel] = bend;
		}
	}

	// Use a default velocity if none was specified, otherwise squish it into
	// a 7-bit value.
	uint8_t velocity =
		(ev->velocity == 0) ? MIDI_DEFAULT_ATTACK_VELOCITY : (ev->velocity >> 1);

	// If there's a note already playing, switch it off first
	if (this->activeNote[trackIndex] != ACTIVE_NOTE_NONE) {
		this->cb->midiNoteOff(this->cachedDelay, midiChannel, this->activeNote[trackIndex],
			MIDI_DEFAULT_RELEASE_VELOCITY);
		this->cachedDelay = 0;
		// No need to do this as it will just be changed below
		// this->activeNote[trackIndex] = ACTIVE_NOTE_NONE;
	}

	this->cb->midiNoteOn(this->cachedDelay, midiChannel, note, velocity);

	this->activeNote[trackIndex] = note;
	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	const TrackInfo& trackInfo = this->music->trackInfo[trackIndex];
	if (
		!(this->midiFlags & MIDIFlags::UsePatchIndex) &&
		(trackInfo.channelType != TrackInfo::MIDIChannel)
	) return;
	const unsigned int& midiChannel = trackInfo.channelIndex;
	this->cachedDelay += delay;

	this->cb->midiNoteOff(this->cachedDelay, midiChannel, this->activeNote[trackIndex],
		MIDI_DEFAULT_RELEASE_VELOCITY);
	this->activeNote[trackIndex] = ACTIVE_NOTE_NONE;

	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	const TrackInfo& trackInfo = this->music->trackInfo[trackIndex];
	if (
		!(this->midiFlags & MIDIFlags::UsePatchIndex) &&
		(trackInfo.channelType != TrackInfo::MIDIChannel)
	) return;
	const unsigned int& midiChannel = trackInfo.channelIndex;
	this->cachedDelay += delay;

	switch (ev->type) {
		case EffectEvent::Pitchbend: {
			if (this->midiFlags & MIDIFlags::IntegerNotesOnly) return;

			uint8_t note;
			int16_t bend;
			freqToMIDI(ev->data, &note, &bend, this->activeNote[trackIndex]);
			/// @todo Handle pitch bending further than one note or if 'note' comes out
			/// differently to the currently playing note
			if (bend != this->currentPitchbend[midiChannel]) {
				bend += 8192;
				this->cb->midiPitchbend(this->cachedDelay, midiChannel, bend);
				this->currentPitchbend[midiChannel] = bend;
			}
			break;
		}
		//case EffectEvent::Volume:
		//	break;
	}
	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->cachedDelay += delay;
	switch (ev->configType) {
		case ConfigurationEvent::EnableRhythm:
			// Controller 0x67 (CMF rhythm)
			this->cb->midiController(this->cachedDelay, 0, 0x67, ev->value);
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
	return;
}

void EventConverter_MIDI::handleAllEvents(EventHandler::EventOrder eventOrder)
{
	this->EventHandler::handleAllEvents(eventOrder, this->music);
}
