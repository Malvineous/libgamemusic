/**
 * @file  encode-midi.cpp
 * @brief Function to convert a Music instance into raw MIDI data.
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

#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/util-midi.hpp>
#include "encode-midi.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Class taking MIDI events and producing SMF MIDI data.
class MIDIEncoder: virtual private MIDIEventCallback
{
	public:
		/// Set encoding parameters.
		/**
		 * @param output
		 *   Data stream to write the MIDI data to.  Must remain valid until the
		 *   class has been destroyed.
		 *
		 * @param music
		 *   The instance to convert to MIDI data.  Must remain valid until the
		 *   class has been destroyed.
		 *
		 * @param midiFlags
		 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
		 *   data is unusual in some way.
		 *
		 * @param cbEndOfTrack
		 *   Callback notified at the end of each track.  May be null.
		 */
		MIDIEncoder(stream::output& output, const Music& music,
			MIDIFlags midiFlags, std::function<void()> cbEndOfTrack);

		/// Destructor.
		~MIDIEncoder();

		/// Process the events, and write out MIDI data.
		/**
		 * This function will write SMF (standard MIDI format) data to the output
		 * stream, until all the events in the song have been written out.
		 *
		 * @param eventOrder
		 *   The order the events should be processed (e.g. chronologically or
		 *   track by track).
		 *
		 * @param channelsUsed
		 *   Pointer to an array of MIDI_CHANNEL_COUNT entries of bool.  On return,
		 *   this each entry is set to true where that MIDI channel was used.  Set
		 *   to nullptr if this information is not required.
		 *
		 * @throw stream:error
		 *   If the output data could not be written for some reason.
		 *
		 * @throw format_limitation
		 *   If the song could not be converted to MIDI for some reason (e.g. it has
		 *   sampled instruments.)
		 */
		void encode(EventHandler::EventOrder eventOrder, bool *channelsUsed);

		// MIDIEventCallback
		virtual void midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity);
		virtual void midiNoteOn(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity);
		virtual void midiPatchChange(uint32_t delay, uint8_t channel,
			uint8_t instrument);
		virtual void midiController(uint32_t delay, uint8_t channel,
			uint8_t controller, uint8_t value);
		virtual void midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend);
		virtual void midiSetTempo(uint32_t delay, const Tempo& tempo);
		virtual void endOfTrack();
		virtual void endOfPattern();
		virtual void endOfSong(uint32_t delay);

	protected:
		stream::output& output;            ///< Target stream for SMF MIDI data
		const Music& music;                ///< Song to convert
		MIDIFlags midiFlags;               ///< One or more MIDIFlags
		std::function<void()> cbEndOfTrack;///< Callback used at end of each track
		uint8_t lastCommand;               ///< Last MIDI command written
		bool channelsUsed[MIDI_CHANNEL_COUNT];  ///< Which MIDI channels had events on them

		/// Write a MIDI command, using running notation if possible.
		/**
		 * This implements the running-status logic, whereby an event is only
		 * written if it is different to the previous event.
		 *
		 * @pre cmd < 0xF0 as sysex/meta events are not considered in the
		 *   running-status.
		 *
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.
		 *
		 * @param cmd
		 *   MIDI command and channel.
		 */
		void writeCommand(uint32_t delay, uint8_t cmd);
};

void camoto::gamemusic::midiEncode(stream::output& output, const Music& music,
	MIDIFlags midiFlags, bool *channelsUsed,
	EventHandler::EventOrder eventOrder, std::function<void()> cbEndOfTrack)
{
	MIDIEncoder encoder(output, music, midiFlags, cbEndOfTrack);
	encoder.encode(eventOrder, channelsUsed);
	return;
}

MIDIEncoder::MIDIEncoder(stream::output& output, const Music& music,
	MIDIFlags midiFlags, std::function<void()> cbEndOfTrack)
	:	output(output),
		music(music),
		midiFlags(midiFlags),
		cbEndOfTrack(cbEndOfTrack),
		lastCommand(0xFF)
{
	for (unsigned int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		this->channelsUsed[i] = false;
	}
}

MIDIEncoder::~MIDIEncoder()
{
}

void MIDIEncoder::encode(EventHandler::EventOrder eventOrder, bool *channelsUsed)
{
	// Create a dummy shared_ptr to pass to EventConverter_MIDI, which takes the
	// reference to the Music instance and converts it into a fake smart_ptr
	// (which will never try to delete the reference.)
	// This is safe because the EventConverter_MIDI instance holding this fake
	// smart_ptr does not outlive the current function.
	std::shared_ptr<const Music> music_ptr(&this->music, [](const Music *p){ return; });

	EventConverter_MIDI conv(this, music_ptr, this->midiFlags);

	if (this->midiFlags & MIDIFlags::EmbedTempo) {
		TempoEvent tempoEvent;
		tempoEvent.tempo = this->music.initialTempo;
		conv.handleEvent(0, 0, 0, &tempoEvent);
	}

	conv.handleAllEvents(eventOrder);

	if (channelsUsed) {
		memcpy(channelsUsed, this->channelsUsed, sizeof(this->channelsUsed));
	}
	return;
}

void MIDIEncoder::writeCommand(uint32_t delay, uint8_t cmd)
{
	this->output << u28midi(delay);
	assert(cmd < 0xF0); // these commands are not part of the running status
	if (this->lastCommand == cmd) return;
	this->output << u8(cmd);
	this->lastCommand = cmd;
	return;
}

void MIDIEncoder::midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
	uint8_t velocity)
{
	assert(note < 128);
	assert(velocity < 128);
	this->channelsUsed[channel] = true;
	if ((this->lastCommand == (0x90 | channel)) && (velocity == MIDI_DEFAULT_RELEASE_VELOCITY)) {
		// Since the last event was a note-on, it will be more efficient to
		// write the note-off as a zero-velocity note-on, as we won't have to
		// write the actual 'note on' byte thanks to MIDI running-status.
		this->writeCommand(delay, this->lastCommand);

		// The velocity *must* be zero now, otherwise we'll get another note on!
		velocity = 0;
	} else {
		// Last event wasn't a note-off, or we have to specify a velocity value
		this->writeCommand(delay, 0x80 | channel);
	}
	this->output
		<< u8(note)
		<< u8(velocity)
	;
	return;
}

void MIDIEncoder::midiNoteOn(uint32_t delay, uint8_t channel, uint8_t note,
	uint8_t velocity)
{
	assert(note < 128);
	assert(velocity < 128);
	this->channelsUsed[channel] = true;
	this->writeCommand(delay, 0x90 | channel);
	this->output
		<< u8(note)
		<< u8(velocity)
	;
	return;
}

void MIDIEncoder::midiPatchChange(uint32_t delay, uint8_t channel,
	uint8_t instrument)
{
	this->channelsUsed[channel] = true;
	this->writeCommand(delay, 0xC0 | channel);
	this->output << u8(instrument);
	return;
}

void MIDIEncoder::midiController(uint32_t delay, uint8_t channel,
	uint8_t controller, uint8_t value)
{
	this->channelsUsed[channel] = true;
	this->writeCommand(delay, 0xB0 | channel);
	this->output
		<< u8(controller)
		<< u8(value)
	;
	return;
}

void MIDIEncoder::midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend)
{
	assert(bend < 16384);
	this->channelsUsed[channel] = true;
	uint8_t msb = (bend >> 7) & 0x7F;
	uint8_t lsb = bend & 0x7F;
	this->writeCommand(delay, 0xE0 | channel);
	this->output
		<< u8(lsb)
		<< u8(msb)
	;
	return;
}

void MIDIEncoder::midiSetTempo(uint32_t delay, const Tempo& tempo)
{
	unsigned long usPerQuarterNote = tempo.usPerQuarterNote();
	this->output << u28midi(delay);
	this->output.write("\xFF\x51\x03", 3);
	this->output
		<< u8(usPerQuarterNote >> 16)
		<< u8(usPerQuarterNote >> 8)
		<< u8(usPerQuarterNote & 0xFF)
	;
	return;
}

void MIDIEncoder::endOfTrack()
{
	if (this->cbEndOfTrack) this->cbEndOfTrack();
	return;
}

void MIDIEncoder::endOfPattern()
{
	//if (this->cbEndOfPattern) this->cbEndOfPattern();
	return;
}

void MIDIEncoder::endOfSong(uint32_t delay)
{
	this->output << u28midi(delay);
	// Write an end-of-song event
	this->output.write("\xFF\x2F\x00", 3);
	return;
}
