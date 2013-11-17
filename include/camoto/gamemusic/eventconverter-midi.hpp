/**
 * @file   eventconverter-midi.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_EVENTCONVERTER_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_EVENTCONVERTER_MIDI_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/events.hpp>
#include <camoto/stream.hpp>

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

namespace camoto {
namespace gamemusic {

/// Number of MIDI channels.
const unsigned int MIDI_CHANNELS = 16;

/// Number of valid MIDI notes.
const unsigned int MIDI_NOTES = 128;

/// Number of valid MIDI patches.
const unsigned int MIDI_PATCHES = 128;

/// Default number of ticks per quarter note.
const unsigned int MIDI_DEF_TICKS_PER_QUARTER_NOTE = 192;

/// Default number of microseconds per quarter note.
const unsigned long MIDI_DEF_uS_PER_QUARTER_NOTE = 500000;

/// Default number of microseconds per MIDI tick.
const unsigned long MIDI_DEF_uS_PER_TICK =
	MIDI_DEF_uS_PER_QUARTER_NOTE / MIDI_DEF_TICKS_PER_QUARTER_NOTE;

/// Default value to use for MIDI note-off events.
const unsigned int MIDI_DEFAULT_RELEASE_VELOCITY = 127;

/// Default value to use for MIDI note-on events.
const unsigned int MIDI_DEFAULT_ATTACK_VELOCITY = 127;

/// Flags indicating variations in the type of MIDI data.
struct MIDIFlags {
	enum Type {
		/// Normal MIDI data.
		Default = 0,

		/// Note aftertouch events are one byte too short.  These will be handled
		/// as channel aftertouch events (i.e. they will affect all notes on a
		/// channel instead of just one note.)
		ShortAftertouch = 1,

		/// Channel 10 is just another channel (not percussion)
		Channel10NoPerc = 2,

		/// Use basic MIDI commands only (no sysex)
		BasicMIDIOnly = 4,

		/// Disable pitchbends.
		IntegerNotesOnly = 8,

		/// Only play notes when there is a MIDI patch available.
		/**
		 * The live player uses this to mute any notes that don't have MIDI patches
		 * (since they will probably have OPL or some other patch instead) but the
		 * CMF player can't use this because it needs MIDI notes when there are no
		 * MIDI patches (only OPL ones) available.
		 */
		MIDIPatchesOnly = 16,
	};
};

/// Callback used to do something with some MIDI events.
class MIDIEventCallback
{
	public:
		/// Stop sounding a note.
		/**
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.
		 *
		 * @param channel
		 *   MIDI channel (0-15 inclusive) to action this event on.
		 *
		 * @param note
		 *   MIDI number of note to silence, 0-127 inclusive.
		 *
		 * @param velocity
		 *   Velocity of release.  Use MIDI_DEFAULT_RELEASE_VELOCITY where possible
		 *   as this allows better optimisation of events.
		 */
		virtual void midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity) = 0;

		/// Start sounding a note.
		/**
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.
		 *
		 * @param channel
		 *   MIDI channel (0-15 inclusive) to action this event on.
		 *
		 * @param note
		 *   MIDI number of note to play, 0-127 inclusive.
		 *
		 * @param velocity
		 *   Velocity of note.  Use MIDI_DEFAULT_ATTACK_VELOCITY if the value is
		 *   unknown.
		 */
		virtual void midiNoteOn(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity) = 0;

		/// Change instrument on a channel.
		/**
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.
		 *
		 * @param channel
		 *   MIDI channel (0-15 inclusive) to action this event on.
		 *
		 * @param instrument
		 *   MIDI instrument to use, 0-127 inclusive.
		 */
		virtual void midiPatchChange(uint32_t delay, uint8_t channel,
			uint8_t instrument) = 0;

		/// Change a MIDI controller on a channel.
		/**
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.
		 *
		 * @param channel
		 *   MIDI channel (0-15 inclusive) to action this event on.
		 *
		 * @param controller
		 *   MIDI controller to use, 0-127 inclusive.
		 *
		 * @param value
		 *   Value to set controller to, 0-127 inclusive.
		 */
		virtual void midiController(uint32_t delay, uint8_t channel,
			uint8_t controller, uint8_t value) = 0;

		/// Alter pitch of all notes currently playing on a channel.
		/**
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.
		 *
		 * @param channel
		 *   MIDI channel (0-15 inclusive) to action this event on.
		 *
		 * @param bend
		 *   Amount of bending.  0 is as low as possible, +8192 is none/default
		 *   and +16383 is as high as possible.
		 */
		virtual void midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend) = 0;

		/// Change the song speed.
		/**
		 * @param delay
		 *   Delay since previous event, in MIDI ticks.  This is the delay at the
		 *   old tempo before the tempo change is actioned.
		 *
		 * @param usPerTick
		 *   New tempo, in number of microseconds per tick.
		 */
		virtual void midiSetTempo(uint32_t delay, tempo_t usPerTick) = 0;

};

/// Convert MIDI note number into milliHertz.
/**
 * @param midi
 *   MIDI note number between 0 and 127 inclusive.  Fractional numbers (i.e.
 *   as a result of a pitchbend) are permitted.
 *
 * @return Frequency in milliHertz (440000 == 440Hz == A4)
 */
unsigned long DLL_EXPORT midiToFreq(double midi);

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
void DLL_EXPORT freqToMIDI(unsigned long milliHertz, uint8_t *note, int16_t *bend,
	uint8_t activeNote);

/// EventHandler class that can produce MIDI events.
/**
 * This class handles libgamemusic events and calls its own virtual functions
 * corresponding to the MIDI event needing to be handled.
 *
 * @todo Are callbacks used to figure out channel mappings or what?  Given that
 *   we have to write events like 'end of track/song' and different formats store
 *   tracks differently, this can only really handle a single MIDI track.
 */
class DLL_EXPORT EventConverter_MIDI: virtual public EventHandler
{
	protected:
		MIDIEventCallback *cb;             ///< Callback to handle MIDI events
		const PatchBankPtr patches;        ///< List of instruments
		unsigned int midiFlags;            ///< Flags supplied in constructor
		unsigned long lastTick;            ///< Time of last event
		unsigned long ticksPerQuarterNote; ///< Required for tempo calculation
		tempo_t usPerTick;                 ///< Current song tempo

		bool deepTremolo;   ///< MIDI controller 0x63 bit 0
		bool deepVibrato;   ///< MIDI controller 0x63 bit 1
		bool updateDeep;    ///< True if deepTremolo or deepVibrato have changed

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
		unsigned long first_usPerTick;           ///< Initial song tempo

		/// Prepare for event conversion.
		/**
		 * @param cb
		 *   Callback which will handle all the MIDI events generated.
		 *
		 * @param patches
		 *   Optional MIDI patchback if the output data will be processed by
		 *   a General MIDI device.  Set to a null pointer if GM is not
		 *   required (e.g. CMF file with OPL instruments.)  This is needed to
		 *   ensure MIDI percussion instruments end up on channel 10.
		 *
		 * @param midiFlags
		 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
		 *   data is unusual in some way.
		 *
		 * @param ticksPerQuarterNote
		 *   Number of MIDI ticks per quarter-note.  This controls how many notes appear
		 *   in each notation bar, among other things.  It has no effect on playback
		 *   speed.
		 */
		EventConverter_MIDI(MIDIEventCallback *cb, const PatchBankPtr patches,
			unsigned int midiFlags, unsigned long ticksPerQuarterNote);

		virtual ~EventConverter_MIDI();

		/// Prepare to start from the first event.
		/**
		 * This function must be called before re-sending an old event, otherwise
		 * the resulting negative delay will cause an extremely long pause.
		 */
		void rewind();

		// EventHandler functions

		virtual void handleEvent(const TempoEvent *ev);

		virtual void handleEvent(const NoteOnEvent *ev);

		virtual void handleEvent(const NoteOffEvent *ev);

		virtual void handleEvent(const PitchbendEvent *ev);

		virtual void handleEvent(const ConfigurationEvent *ev);

	protected:

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
		uint8_t getMIDIchannel(unsigned int channel, unsigned int numMIDIchans);

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_EVENTCONVERTER_MIDI_HPP_
