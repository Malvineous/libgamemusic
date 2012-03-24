/**
 * @file   encode-midi.cpp
 * @brief  Function to convert a Music instance into raw MIDI data.
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

#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include "encode-midi.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Class taking MIDI events and producing SMF MIDI data.
class MIDIEncoder: virtual private MIDIEventCallback
{
	public:
		/// Set encoding parameters.
		/**
		 * @param input
		 *   Data stream to write the MIDI data to.
		 *
		 * @param midiFlags
		 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
		 *   data is unusual in some way.
		 *
		 * @param ticksPerQuarterNote
		 *   Number of MIDI ticks per quarter-note.  This controls how many notes appear
		 *   in each notation bar, among other things.  It has no effect on playback
		 *   speed.
		 *
		 * @param usPerQuarterNote
		 *   On return, the initial number of microseconds per quarter-note.  This could
		 *   have been overridden during the song, but this is the initial value.
		 */
		MIDIEncoder(stream::output_sptr& output, unsigned int midiFlags,
			unsigned long ticksPerQuarterNote, unsigned long *usPerQuarterNote)
			throw ();

		/// Destructor.
		~MIDIEncoder()
			throw ();

		/// Process the events, and write out MIDI data.
		/**
		 * This function will write SMF (standard MIDI format) data to the output
		 * stream, until all the events in the song have been written out.
		 *
		 * @param music
		 *   The instance to convert to MIDI data.
		 *
		 * @throw stream:error
		 *   If the output data could not be written for some reason.
		 *
		 * @throw format_limitation
		 *   If the song could not be converted to MIDI for some reason (e.g. it has
		 *   sampled instruments.)
		 */
		void encode(const MusicPtr music)
			throw (stream::error, format_limitation);

		// MIDIEventCallback

		virtual void midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity)
			throw (stream::error);

		virtual void midiNoteOn(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity)
			throw (stream::error);

		virtual void midiPatchChange(uint32_t delay, uint8_t channel,
			uint8_t instrument)
			throw (stream::error);

		virtual void midiController(uint32_t delay, uint8_t channel,
			uint8_t controller, uint8_t value)
			throw (stream::error);

		virtual void midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend)
			throw (stream::error);

		virtual void midiSetTempo(uint32_t delay, uint32_t usPerQuarterNote)
			throw (stream::error);

	protected:
		stream::output_sptr output;        ///< Target stream for SMF MIDI data
		unsigned int midiFlags;            ///< One or more MIDIFlags
		uint8_t lastCommand;               ///< Last MIDI command written
		unsigned long ticksPerQuarterNote; ///< Current song granularity
		unsigned long *usPerQuarterNote;   ///< Where to store initial tempo

		/// Write an integer in variable-length MIDI notation.
		/**
		 * This function will write up to four bytes into the MIDI stream.
		 *
		 * @param value
		 *   Value to write.  Although it is a 32-bit integer, the MIDI format can
		 *   only accept up to 28 bits.  An assertion failure will be triggered
		 *   if the value is larger than this.
		 */
		void writeMIDINumber(uint32_t value)
			throw (stream::error);

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
		void writeCommand(uint32_t delay, uint8_t cmd)
			throw (stream::error);
};


void camoto::gamemusic::midiEncode(stream::output_sptr& output,
	unsigned int midiFlags, MusicPtr music, unsigned long ticksPerQuarterNote,
	unsigned long *usPerQuarterNote)
	throw (stream::error, format_limitation)
{
	MIDIEncoder encoder(output, midiFlags, ticksPerQuarterNote, usPerQuarterNote);
	encoder.encode(music);
	return;
}

MIDIEncoder::MIDIEncoder(stream::output_sptr& output, unsigned int midiFlags,
	unsigned long ticksPerQuarterNote, unsigned long *usPerQuarterNote)
	throw ()
	: output(output),
	  midiFlags(midiFlags),
	  ticksPerQuarterNote(ticksPerQuarterNote),
	  usPerQuarterNote(usPerQuarterNote)
{
}

MIDIEncoder::~MIDIEncoder()
	throw ()
{
}

void MIDIEncoder::encode(const MusicPtr music)
	throw (stream::error, format_limitation)
{
	MIDIPatchBankPtr midiPatches =
		boost::dynamic_pointer_cast<MIDIPatchBank>(music->patches);
	// midiPatches may be NULL
	EventConverter_MIDI conv(this, midiPatches, this->midiFlags,
		ticksPerQuarterNote);
	conv.handleAllEvents(music->events);

	*this->usPerQuarterNote = conv.getusPerQuarterNote();

	// Write an end-of-song event
	this->output->write("\x00\xFF\x2F\x00", 4);
	return;
}

void MIDIEncoder::writeMIDINumber(uint32_t value)
	throw (stream::error)
{
	// Make sure the value will fit in MIDI's 28-bit limit.
	assert((value >> 28) == 0);

	// Write the three most-significant bytes as 7-bit bytes, with the most
	// significant bit set.
	uint8_t next;
	if (value & (0x7F << 28)) { next = 0x80 | (value >> 28); this->output << u8(next); }
	if (value & (0x7F << 14)) { next = 0x80 | (value >> 14); this->output << u8(next); }
	if (value & (0x7F <<  7)) { next = 0x80 | (value >>  7); this->output << u8(next); }

	// Write the least significant 7-bits last, with the high bit unset to
	// indicate the end of the variable-length stream.
	next = (value & 0x7F);
	this->output << u8(next);

	return;
}

void MIDIEncoder::writeCommand(uint32_t delay, uint8_t cmd)
	throw (stream::error)
{
	this->writeMIDINumber(delay);
	assert(cmd < 0xF0); // these commands are not part of the running status
	if (this->lastCommand == cmd) return;
	this->output << u8(cmd);
	this->lastCommand = cmd;
	return;
}

void MIDIEncoder::midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
	uint8_t velocity)
	throw (stream::error)
{
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
	throw (stream::error)
{
	this->writeCommand(delay, 0x90 | channel);
	this->output
		<< u8(note)
		<< u8(velocity)
	;
	return;
}

void MIDIEncoder::midiPatchChange(uint32_t delay, uint8_t channel,
	uint8_t instrument)
	throw (stream::error)
{
	this->writeCommand(delay, 0xC0 | channel);
	this->output << u8(instrument);
	return;
}

void MIDIEncoder::midiController(uint32_t delay, uint8_t channel,
	uint8_t controller, uint8_t value)
	throw (stream::error)
{
	this->writeCommand(delay, 0xB0 | channel);
	this->output
		<< u8(controller)
		<< u8(value)
	;
	return;
}

void MIDIEncoder::midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend)
	throw (stream::error)
{
	uint8_t msb = (bend >> 7) & 0x7F;
	uint8_t lsb = bend & 0x7F;
	this->writeCommand(delay, 0xE0 | channel);
	this->output
		<< u8(lsb)
		<< u8(msb)
	;
	return;
}

void MIDIEncoder::midiSetTempo(uint32_t delay, uint32_t usPerQuarterNote)
	throw (stream::error)
{
	this->writeMIDINumber(delay);
	this->output->write("\xFF\x51\x03", 3);
	this->output
		<< u8(usPerQuarterNote >> 16)
		<< u8(usPerQuarterNote >> 8)
		<< u8(usPerQuarterNote & 0xFF)
	;
	return;
}
