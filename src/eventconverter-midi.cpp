/**
 * @file  eventconverter-midi.cpp
 * @brief EventConverter implementation that produces MIDI events from Events.
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

#include <math.h> // pow
#include <iostream>
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

double camoto::gamemusic::freqToMIDI(unsigned long milliHertz)
{
	return 12.0 * log2((double)milliHertz / 440000.0) + 69.0;
}

void camoto::gamemusic::freqToMIDI(unsigned long milliHertz, uint8_t *note,
	int16_t *bend, uint8_t curNote)
{
	// Lower bound is clamped to MIDI note #0.  Could probably get lower with a
	// pitchbend but the difference is unlikely to be audible (8Hz is pretty much
	// below human hearing anyway.)
	if (milliHertz <= 8175) {
		*note = 0;
		*bend = 8192;
		return;
	}
	double val = freqToMIDI(milliHertz);
	// round to three decimal places
	val = round(val * 1000.0) / 1000.0;
	if (curNote == 0xFF) *note = round(val);
	else *note = curNote;
	*bend = 8192 + (val - *note) * 4096;

	// If the pitchbend is out of range, just clamp it
	if (*bend < 0) *bend = 0;
	if (*bend > 16383) *bend = 16383;

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
	std::shared_ptr<const Music> music, MIDIFlags midiFlags)
	:	cb(cb),
		music(music),
		midiFlags(midiFlags),
		usPerTick(0),
		cachedDelay(0),
		updateDeep(false)
{
	memset(this->currentPatch, 0xFF, sizeof(this->currentPatch));
	memset(this->currentInternalPatch, 0xFF, sizeof(this->currentInternalPatch));
	memset(this->activeNote, ACTIVE_NOTE_NONE, sizeof(this->activeNote));
	memset(this->lastEvent, 0xFF, sizeof(this->lastEvent));
	memset(this->channelMap, 0xFF, sizeof(this->channelMap));
	for (unsigned int c = 0; c < MIDI_CHANNEL_COUNT; c++) {
		this->currentPitchbend[c] = 8192;
	}
	this->deepTremolo = (this->midiFlags & MIDIFlags::CMFExtensions) ? true : false;
	this->deepVibrato = (this->midiFlags & MIDIFlags::CMFExtensions) ? true : false;
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
	assert(ev->velocity < 256);

	auto& trackInfo = this->music->trackInfo[trackIndex];
	if (
		!(this->midiFlags & MIDIFlags::UsePatchIndex) &&
		(trackInfo.channelType != TrackInfo::ChannelType::MIDI)
	) return;
	auto& midiChannel = trackInfo.channelIndex;
	this->cachedDelay += delay;

	// Figure out which MIDI instrument number to use
	unsigned int targetPatch;
	assert(ev->instrument < this->music->patches->size());
	if (this->midiFlags & MIDIFlags::UsePatchIndex) {
		targetPatch = ev->instrument;
	} else {
		auto patch =
			dynamic_cast<MIDIPatch*>(this->music->patches->at(ev->instrument).get());
		if (!patch) return;
		targetPatch = patch->midiPatch;
	}

	if (this->midiFlags & MIDIFlags::CMFExtensions) {
		// Batch these two changes together so they can happen in one event
		if (this->updateDeep) {
			// Need to set CMF controller 0x63
			uint8_t val = (this->deepTremolo ? 2 : 0) | (this->deepVibrato ? 1 : 0);
			this->cb->midiController(this->cachedDelay, 0, 0x63, val);
			this->cachedDelay = 0;
			this->updateDeep = false;
		}
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

	// Use a default velocity if none was specified, otherwise squish it into
	// a 7-bit value.
	uint8_t velocity =
		(ev->velocity == DefaultVelocity) ? MIDI_DEFAULT_ATTACK_VELOCITY : (ev->velocity >> 1);

	// If there's a note already playing, switch it off first
	if (this->activeNote[trackIndex] != ACTIVE_NOTE_NONE) {
		this->cb->midiNoteOff(this->cachedDelay, midiChannel, this->activeNote[trackIndex],
			MIDI_DEFAULT_RELEASE_VELOCITY);
		this->cachedDelay = 0;
		// No need to do this as it will just be changed below
		// this->activeNote[trackIndex] = ACTIVE_NOTE_NONE;
	}

	// If the pitch isn't on a note boundary, issue a bend first (or reset the
	// bend if there's one active on the channel)
	if (!(this->midiFlags & MIDIFlags::IntegerNotesOnly)) {
		if (bend != this->currentPitchbend[midiChannel]) {
			this->cb->midiPitchbend(this->cachedDelay, midiChannel, bend);
			this->cachedDelay = 0;
			this->currentPitchbend[midiChannel] = bend;
		}
	}

	this->cb->midiNoteOn(this->cachedDelay, midiChannel, note, velocity);
	this->cachedDelay = 0;

	this->activeNote[trackIndex] = note;
	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	auto& trackInfo = this->music->trackInfo[trackIndex];
	if (
		!(this->midiFlags & MIDIFlags::UsePatchIndex) &&
		(trackInfo.channelType != TrackInfo::ChannelType::MIDI)
	) return;
	const unsigned int& midiChannel = trackInfo.channelIndex;
	this->cachedDelay += delay;

	if (this->activeNote[trackIndex] == ACTIVE_NOTE_NONE) {
		std::cerr << "eventconverter-midi: Tried to switch off note on track "
			<< trackIndex << " in pattern " << patternIndex
			<< " but there was no note playing!\n";
		return;
	}
	this->cb->midiNoteOff(this->cachedDelay, midiChannel,
		this->activeNote[trackIndex], MIDI_DEFAULT_RELEASE_VELOCITY);
	this->cachedDelay = 0;
	this->activeNote[trackIndex] = ACTIVE_NOTE_NONE;

	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	auto& trackInfo = this->music->trackInfo[trackIndex];
	if (
		!(this->midiFlags & MIDIFlags::UsePatchIndex) &&
		(trackInfo.channelType != TrackInfo::ChannelType::MIDI)
	) return;
	const unsigned int& midiChannel = trackInfo.channelIndex;
	assert(midiChannel < MIDI_CHANNEL_COUNT);
	this->cachedDelay += delay;

	switch (ev->type) {
		case EffectEvent::Type::PitchbendNote: {
			if (this->midiFlags & MIDIFlags::IntegerNotesOnly) return;
			// If there's no note playing, don't do a pitchbend because it'll get done
			// when the next note-on happens.
			if (this->activeNote[trackIndex] == ACTIVE_NOTE_NONE) return;

			// We have to bend the whole channel because of MIDI limitations.  We
			// remember the bend though, so that it will be reset on the next note.
			uint8_t note;
			int16_t bend;
			freqToMIDI(ev->data, &note, &bend, this->activeNote[trackIndex]);
			/// @todo Handle pitch bending further than one note or if 'note' comes out
			/// differently to the currently playing note
			if (bend != this->currentPitchbend[midiChannel]) {
				this->cb->midiPitchbend(this->cachedDelay, midiChannel, bend);
				this->cachedDelay = 0;
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
	unsigned int trackIndex, unsigned int patternIndex, const GotoEvent *ev)
{
	// MIDI doesn't really have a concept of jumps, EMIDI notwithstanding
	this->cachedDelay += delay;
	return;
}

void EventConverter_MIDI::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->cachedDelay += delay;
	switch (ev->configType) {
		case ConfigurationEvent::Type::EmptyEvent:
			// cached delay is updated
			break;
		case ConfigurationEvent::Type::EnableRhythm:
			// Controller 0x67 (CMF rhythm)
			if (this->midiFlags & MIDIFlags::CMFExtensions) {
				this->cb->midiController(this->cachedDelay, 0, 0x67, ev->value);
			}
			this->cachedDelay = 0;
			break;
		case ConfigurationEvent::Type::EnableDeepTremolo: {
			if (this->deepTremolo != (ev->value != 0)) {
				this->deepTremolo = ev->value;
				this->updateDeep = true;
			}
			break;
		}
		case ConfigurationEvent::Type::EnableDeepVibrato: {
			if (this->deepVibrato != (ev->value != 0)) {
				this->deepVibrato = ev->value;
				this->updateDeep = true;
			}
			break;
		}
		default:
			break;
	}
	return;
}

void EventConverter_MIDI::handleAllEvents(EventHandler::EventOrder eventOrder)
{
	this->EventHandler::handleAllEvents(eventOrder, *this->music);
	this->cb->endOfSong(this->cachedDelay);
	this->cachedDelay = 0;
}
