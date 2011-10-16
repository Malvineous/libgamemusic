/**
 * @file   mus-generic-midi.hpp
 * @brief  MusicReader and MusicWriter classes to process standard MIDI data.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_GENERIC_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_GENERIC_MIDI_HPP_

#include <deque>
#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/patchbank-midi.hpp>
#include <camoto/stream.hpp>
#include <stdint.h>

namespace camoto {
namespace gamemusic {

/// Number of MIDI channels.
const int MIDI_CHANNELS = 16;

/// Number of valid MIDI notes.
const int MIDI_NOTES = 128;

/// Number of valid MIDI patches.
const int MIDI_PATCHES = 128;

/// Flags indicating variations in the type of MIDI data.
struct MIDIFlags {
	enum Type {
		/// Normal MIDI data.
		Default = 0,

		/// Note aftertouch events are one byte too short.  These will be handled
		/// as channel aftertouch events (i.e. they will affect all notes on a
		/// channel instead of just one note.)
		ShortAftertouch = 1,

		/// Is channel 10 used for percussion?
		Channel10Perc = 2,

		/// Use basic MIDI commands only? (no sysex)
		BasicMIDIOnly = 4,
	};
};

/// Convert MIDI note number into milliHertz.
/**
 * @param midi
 *   MIDI note number between 0 and 127 inclusive.  Fractional numbers (i.e.
 *   as a result of a pitchbend) are permitted.
 *
 * @return Frequency in milliHertz (440000 == 440Hz == A4)
 */
unsigned long midiToFreq(double midi);

/// Convert milliHertz into MIDI note number and pitchbend value.
/**
 * @param milliHertz
 *   Frequency value to convert (440000 == 440Hz == A4)
 *
 * @param note
 *   MIDI note number (between 0 and 127 inclusive) is written here
 *
 * @param bend
 *   MIDI pitchbend value (between -8192 and 8191 inclusive) is written here
 *
 * @param activeNote
 *   Currently active note on the channel, or 0xFF for no note.  If not 0xFF,
 *   then *note is set to this value and the pitchbend is calculated so as to
 *   keep the note unchanged.
 */
void freqToMIDI(unsigned long milliHertz, uint8_t *note, int16_t *bend,
	uint8_t activeNote);

/// MusicReader class that understands MIDI data.
class MusicReader_GenericMIDI: virtual public MusicReader {

	protected:
		unsigned long tick;               ///< Time of next event
		uint8_t lastEvent;                ///< Last event (for MIDI running status)
		std::deque<EventPtr> eventBuffer; ///< List of events to return in readNextEvent()
		stream::input_sptr midi;          ///< MIDI data to read
		MIDIPatchBankPtr patches;         ///< Cached patches populated by getPatchBank()
		bool setTempo;                    ///< Have we sent the initial tempo event?
		MIDIFlags::Type midiFlags;        ///< Flags supplied in constructor
		unsigned long ticksPerQuarterNote;///< Current song granularity

		/// For each of the 128 MIDI patches, which instrument are we using?
		uint8_t patchMap[MIDI_PATCHES];

		/// For each of the percussion notes, which instrument are we using?
		uint8_t percMap[MIDI_NOTES];

		/// Length of a quarter note, in microseconds.
		unsigned long usPerQuarterNote;

		/// Current instrument (not MIDI patch) on each MIDI channel
		uint8_t currentInstrument[MIDI_CHANNELS];

		/// Current pitchbend level on each MIDI channel (-8192 to 8191)
		int16_t currentPitchbend[MIDI_CHANNELS];

		/// List of notes currently being played on each channel
		bool activeNotes[MIDI_CHANNELS][MIDI_NOTES];

	public:

		/// Constructor
		/**
		 * @param flags
		 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
		 *   data is unusual in some way.
		 */
		MusicReader_GenericMIDI(MIDIFlags::Type midiFlags)
			throw ();

		virtual ~MusicReader_GenericMIDI()
			throw ();

		virtual PatchBankPtr getPatchBank()
			throw ();

		virtual EventPtr readNextEvent()
			throw (stream::error);

		// MusicReader_GenericMIDI functions

		/// Change the tempo of the song.
		/**
		 * This function will normally be called from within nextPair() when a
		 * format-specific command to change the song speed is encountered.
		 */
//		virtual void changeSpeed(uint32_t usPerTick)
//			throw ();

		/// Seek back to the start of the song data.
		/**
		 * @postcondition The next call to nextPair() will return the first
		 *   pair in the file.  This is the same point as when the object is first
		 *   instantiated.
		 */
		virtual void rewind()
			throw () = 0;

		/// Return the number of tracks in this MIDI file.
		/**
		 * All MIDI files have at least one track, but some have additional tracks
		 * with events to be played concurrently.  Each channel within each track
		 * is assigned to a unique libgamemusic channel, i.e. events for MIDI
		 * channel 5 on track 1 might be returned as channel 0, and events for
		 * the same channel 5 on track 2 could be returned as channel 1.
		 *
		 * @todo How is MIDI channel 10 data flagged as percussive?
		 */
//		virtual int getTrackCount()
//			throw () = 0;

	protected:

		/// Get the stream to decode the MIDI data from.
		/**
		 * This function must be implemented by a format-specific handler
		 * extending this class.  The returned stream will be positioned at the
		 * start of the underlying format's MIDI data stream (which does not have
		 * to be at the beginning of the stream.)
		 *
//
		 * @param track
		 *   Zero-based index of track to retrieve.  Must be less than the value
		 *   returned by getTrackCount().  Each track is independent - the delay
		 *   value is relative to the previous event on that track, but of course
		 *   events across all tracks are heard together.  Each track starts at
		 *   tick 0.
		 *
//
		 * @return istream shared pointer to track data.  Try to avoid seeking
		 *   in the stream, as the data begins at the stream's current seek
		 *   position, which may not be the beginning of the stream.
		 */
		virtual void setMIDIStream(stream::input_sptr data)
			throw (stream::error);

		/// Set the initial tempo to be returned in the first event.
		/**
		 * @pre Must be called before the first readNextEvent() as it sets the
		 *   value in the initial tempo event.  It can be called again after
		 *   rewind() though.
		 *
		 * @param ticks
		 *   Number of ticks in a quarter-note.  Defaults to 192.
		 */
		virtual void setTicksPerQuarterNote(unsigned long ticks)
			throw ();

		/// Set the initial tempo to be returned in the first event.
		/**
		 * @pre Must be called before the first readNextEvent() as it sets the
		 *   value in the initial tempo event.  It can be called again after
		 *   rewind() though.
		 *
		 * @param ticks
		 *   Number of microseconds in a quarter-note.  Defaults to 500,000.
		 */
		virtual void setusPerQuarterNote(unsigned long us)
			throw ();

		/// Read a variable-length integer from MIDI data.
		/**
		 * Reads up to four bytes from the MIDI stream, returning a 28-bit
		 * integer.
		 */
		uint32_t readMIDINumber()
			throw (stream::error);

};

/// MusicWriter class that can produce MIDI data.
class MusicWriter_GenericMIDI: virtual public MusicWriter {

	protected:
		unsigned long lastTick;            ///< Time of last event
		stream::output_sptr midi;          ///< Where to write MIDI data
		MIDIPatchBankPtr patches;          ///< List of instruments
		MIDIFlags::Type midiFlags;         ///< Flags supplied in constructor
		uint8_t lastCommand;               ///< Last event written (for running-status)
		unsigned long ticksPerQuarterNote; ///< Current song granularity

		bool deepTremolo;   ///< MIDI controller 0x63 bit 0
		bool deepVibrato;   ///< MIDI controller 0x63 bit 1
		bool updateDeep;    ///< True if deepTremolo or deepVibrato have changed

		/// Length of a quarter note, in microseconds.
		unsigned long usPerQuarterNote;

		/// Current patch on each MIDI channel
		uint8_t currentPatch[MIDI_CHANNELS];

		/// Current patch on each internal channel
		uint8_t currentInternalPatch[MAX_CHANNELS];

		/// Current pitchbend level on each MIDI channel
		int16_t currentPitchbend[MIDI_CHANNELS];

		/// List of notes currently being played on each channel
		uint8_t activeNote[MAX_CHANNELS];

		/// List of notes currently being played on each channel
		uint8_t channelMap[MAX_CHANNELS];

		/// Time the last event was played on this channel
		unsigned long lastEvent[MIDI_CHANNELS];

	public:
		MusicWriter_GenericMIDI(MIDIFlags::Type midiFlags)
			throw ();

		virtual ~MusicWriter_GenericMIDI()
			throw ();

		virtual void setPatchBank(const PatchBankPtr& instruments)
			throw (EBadPatchType);

		/// Finish writing to the output stream.
		/**
		 * If overriding this function, before to call this version of it first!
		 */
		virtual void finish()
			throw (stream::error);

		virtual void handleEvent(TempoEvent *ev)
			throw (stream::error);

		virtual void handleEvent(NoteOnEvent *ev)
			throw (stream::error, EChannelMismatch);

		virtual void handleEvent(NoteOffEvent *ev)
			throw (stream::error);

		virtual void handleEvent(PitchbendEvent *ev)
			throw (stream::error);

		virtual void handleEvent(ConfigurationEvent *ev)
			throw (stream::error);

		// MusicReader_GenericMIDI functions

		/// Change the tempo
		/**
		 * This function must be implemented by a format-specific handler
		 * extending this class.
		 */
//		virtual void changeSpeed(uint32_t usPerTick)
//			throw () = 0;
		/// Set the stream where the output MIDI data will go
		/**
		 * @param data
		 *   Shared pointer to an std::ostream where raw MIDI data will be written.
		 */
		virtual void setMIDIStream(stream::output_sptr data)
			throw (stream::error);

	protected:

		/// Set the initial tempo to be returned in the first event.
		/**
		 * @pre Must be called before the first readNextEvent() as it sets the
		 *   value in the initial tempo event.  It can be called again after
		 *   rewind() though.
		 *
		 * @param ticks
		 *   Number of ticks in a quarter-note.  Defaults to 192.
		 */
		virtual unsigned long getTicksPerQuarterNote()
			throw ();

		/// Set the initial tempo to be returned in the first event.
		/**
		 * @pre Must be called before the first readNextEvent() as it sets the
		 *   value in the initial tempo event.  It can be called again after
		 *   rewind() though.
		 *
		 * @param ticks
		 *   Number of microseconds in a quarter-note.  Defaults to 500,000.
		 */
		virtual unsigned long getusPerQuarterNote()
			throw ();

		/// Write an integer in variable-length MIDI notation.
		/**
		 * Writes up to four bytes into the MIDI stream.
		 *
		 * @param value
		 *   Value to write.  Although it is a 32-bit integer, the MIDI format can
		 *   only accept up to 28 bits.  An assertion failure will be triggered
		 *   if the value is larger than this.
		 */
		virtual void writeMIDINumber(uint32_t value)
			throw (stream::error);

		/// Write the MIDI command only if it's different to the last one
		/**
		 * This implements the running-status logic, whereby an event is only
		 * written if it is different to the previous event.
		 *
		 * @pre cmd < 0xF0 as sysex/meta events are not considered in the
		 *   running-status.
		 *
		 * @post cmd may or may not be written to %midi
		 */
		virtual void writeCommand(uint8_t cmd)
			throw (stream::error);

		/// Get the current mapping of the input channel to a MIDI channel.
		/**
		 * @param channel
		 *   Input channel number.
		 *
		 * @param numMIDIchans
		 *   Number of MIDI channels to use.  Normally will be MIDI_CHANNELS (i.e.
		 *   16) but the CMF handler sets it to 11 so notes will never be mapped
		 *   onto the percussive channels by this function.
		 *
		 * @return MIDI channel number (between 0 and 15 inclusive.)
		 */
		uint8_t getMIDIchannel(int channel, int numMIDIchans)
			throw ();

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_GENERIC_MIDI_HPP_
