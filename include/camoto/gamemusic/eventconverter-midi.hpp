/**
 * @file  camoto/gamemusic/eventconverter-midi.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_EVENTCONVERTER_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_EVENTCONVERTER_MIDI_HPP_

#include <camoto/enum-ops.hpp>
#include <camoto/gamemusic/eventhandler.hpp>

#ifndef CAMOTO_GAMEMUSIC_API
#define CAMOTO_GAMEMUSIC_API
#endif

namespace camoto {
namespace gamemusic {

/// Number of MIDI channels.
const unsigned int MIDI_CHANNEL_COUNT = 16;

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
enum class MIDIFlags {
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

	/// Use the instrument index instead of the MIDI program number.
	/**
	 * Normally an instrument will be MIDI, OPL or PCM, and the MIDI instrument
	 * will include which MIDI Program Number to use.  For example for a song
	 * with only one instrument that is a violin will have a single instrument,
	 * its index will be 0, and it will be of type MIDI with a program number of
	 * 40 (since 40 is a MIDI violin).
	 *
	 * Normally the program number is used, so when instrument #0 is used, a
	 * MIDI patch change event will be issued setting the channel to patch #40.
	 * However if UsePatchIndex is specified, then the instrument number will be
	 * used instead, so instrument #0 will cause a patch change to patch #0,
	 * regardless of whether the patch is OPL, MIDI or PCM.
	 *
	 * This flag is currently not implemented when reading MIDI data.
	 *
	 * This is useful for formats like CMF which use MIDI patch change events
	 * but which don't use MIDI instruments.
	 */
	UsePatchIndex = 16,

	/// Set to embed the tempo as a meta (0xFF) event in the MIDI stream
	EmbedTempo = 32,

	/// Use extensions for .cmf file format (cmf-creativelabs)
	/**
	 * This also preserves note order on MIDI channels 12-15 as they are used
	 * for OPL percussion.
	 */
	CMFExtensions = 64,

	// Use AdLib .mus format timing bytes.
	/**
	 * These use 0xF8 as an overflow byte (of value 240) rather than the high
	 * bit to signify variable-length timing values.
	 */
	AdLibMUS = 128,
};

IMPLEMENT_ENUM_OPERATORS(MIDIFlags);

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
		 * @param tempo
		 *   New tempo.
		 */
		virtual void midiSetTempo(uint32_t delay, const Tempo& tempo) = 0;

		/// End of a track has been reached.
		virtual void endOfTrack() = 0;

		/// End of a pattern has been reached.
		virtual void endOfPattern() = 0;

		/// End of song, with optional final delay.
		virtual void endOfSong(uint32_t delay) = 0;
};

/// Middle-C note number for MIDI functions.
static const int midiMiddleC = 60;

/// Convert MIDI note number into milliHertz.
/**
 * @param midi
 *   MIDI note number between 0 and 127 inclusive.  Fractional numbers (i.e.
 *   as a result of a pitchbend) are permitted.  Middle-C is 60.
 *
 * @return Frequency in milliHertz (440000 == 440Hz == A4)
 */
unsigned long CAMOTO_GAMEMUSIC_API midiToFreq(double midi);

/// Convert milliHertz into a fractional MIDI note number.
/**
 * @param milliHertz
 *   Frequency value to convert (440000 == 440Hz == A4)
 *
 * @return Fractional MIDI note number, e.g. 60.5
 */
double CAMOTO_GAMEMUSIC_API freqToMIDI(unsigned long milliHertz);

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
void CAMOTO_GAMEMUSIC_API freqToMIDI(unsigned long milliHertz, uint8_t *note, int16_t *bend,
	uint8_t activeNote);

/// Convert a MIDI pitchbend (0..16383) into semitones (-2..+1.9999)
inline double midiPitchbendToSemitones(unsigned int bend)
{
	return ((signed int)bend - 8192) / 4096.0;
}

/// Convert a fractional semitone (-2..+2) into a MIDI pitchbend (0..16383)
inline double midiSemitonesToPitchbend(double semitones)
{
	return std::max(0, std::min(16383, (int)(8192 + (semitones * 4096.0))));
}

/// EventHandler class that can produce MIDI events.
/**
 * This class handles libgamemusic events and calls its own virtual functions
 * corresponding to the MIDI event needing to be handled.
 *
 * @todo Are callbacks used to figure out channel mappings or what?  Given that
 *   we have to write events like 'end of track/song' and different formats store
 *   tracks differently, this can only really handle a single MIDI track.
 */
class CAMOTO_GAMEMUSIC_API EventConverter_MIDI: virtual public EventHandler
{
	public:
		/// Prepare for event conversion.
		/**
		 * @param cb
		 *   Callback which will handle all the MIDI events generated.
		 *
		 * @param music
		 *   Song to be converted.
		 *
		 * @param midiFlags
		 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
		 *   data is unusual in some way.
		 */
		EventConverter_MIDI(MIDIEventCallback *cb,
			std::shared_ptr<const Music> music, MIDIFlags midiFlags);

		virtual ~EventConverter_MIDI();

		/// Prepare to start from the first event.
		/**
		 * This function must be called before re-sending an old event, otherwise
		 * the resulting negative delay will cause an extremely long pause.
		 */
		void rewind();

		// EventHandler functions
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual void handleAllEvents(EventHandler::EventOrder eventOrder);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const GotoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

	protected:
		MIDIEventCallback *cb;              ///< Callback to handle MIDI events
		std::shared_ptr<const Music> music; ///< Song being converted
		MIDIFlags midiFlags;                ///< Flags supplied in constructor
		double usPerTick;                   ///< Current song tempo
		unsigned long cachedDelay;          ///< Number of ticks before next event

		bool deepTremolo;   ///< MIDI controller 0x63 bit 0
		bool deepVibrato;   ///< MIDI controller 0x63 bit 1
		bool updateDeep;    ///< True if deepTremolo or deepVibrato have changed

		/// Current patch on each MIDI channel
		uint8_t currentPatch[MIDI_CHANNEL_COUNT];

		/// Current patch on each internal channel
		uint8_t currentInternalPatch[MAX_CHANNELS];

		/// Current pitchbend level on each MIDI channel
		int16_t currentPitchbend[MIDI_CHANNEL_COUNT];

		/// List of notes currently being played on each channel
		uint8_t activeNote[MAX_CHANNELS];
		static const uint8_t ACTIVE_NOTE_NONE = 0xFF;

		/// List of notes currently being played on each channel
		uint8_t channelMap[MAX_CHANNELS];

		/// Time the last event was played on this channel
		unsigned long lastEvent[MIDI_CHANNEL_COUNT];
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_EVENTCONVERTER_MIDI_HPP_
