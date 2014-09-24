/**
 * @file   events.hpp
 * @brief  Declaration of all the Event types.
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

#ifndef _CAMOTO_GAMEARCHIVE_EVENTS_HPP_
#define _CAMOTO_GAMEARCHIVE_EVENTS_HPP_

#include <boost/shared_ptr.hpp>
#include <exception>
#include <vector>

#include <camoto/error.hpp>
#include <camoto/stream.hpp>
#include <stdint.h>
#include <camoto/gamemusic/patchbank.hpp> // bad_patch
#include <camoto/gamemusic/tempo.hpp>

namespace camoto {
namespace gamemusic {

class EventHandler;

/// All channel numbers in the %Event struct must be less than this value.
const unsigned int MAX_CHANNELS = 256;

/// Type to use for all microsecond-per-tick values.
typedef double tempo_t;

/// Base class to represent events in the file.
/**
 * Will be extended by descendent classes to hold format-specific data.
 * The entries here will be valid for all music types.
 */
struct DLL_EXPORT Event
{
	/// Helper function (for debugging) to return all the data as a string
	virtual std::string getContent() const;

	/// Call the handleEvent() function in an EventHandler class, passing this
	/// event as the parameter.
	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) = 0;
};

/// Shared pointer to an event
typedef boost::shared_ptr<Event> EventPtr;

struct DLL_EXPORT TrackEvent
{
	unsigned long delay;
	EventPtr event;
};

/// Vector of shared event pointers
typedef std::vector<TrackEvent> Track;

/// Shared pointer to a list of events
typedef boost::shared_ptr<Track> TrackPtr;

/// Vector of tracks (a pattern)
typedef std::vector<TrackPtr> Pattern;

/// Shared pointer to a pattern
typedef boost::shared_ptr<Pattern> PatternPtr;


/// Changing the tempo changes the rate at which the ticks tick.
/**
 * Remember that since the tick length can vary, you cannot calculate the
 * absolute time of a note by multiplying the tick length by Event.absTime,
 * as this does not take into account any tempo changes in the middle of the
 * song.
 */
struct DLL_EXPORT TempoEvent: virtual public Event
{
	/// New tempo.
	Tempo tempo;

	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);
};


/// A single note is now playing on this channel.
/**
 * @note Only one note can be playing on each channel at a time.  For formats
 *       which allow multiple notes on each channel (e.g. MIDI) some "virtual"
 *       channels will have to be created.
 */
struct DLL_EXPORT NoteOnEvent: virtual public Event
{
	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);

	/// Instrument to play this note with.  This is an index into the vector
	/// returned by Music::getInstruments().
	unsigned int instrument;

	/// Note frequency (440000 == 440Hz)
	unsigned int milliHertz;

	/// Velocity: 0=silent, 255=loud, DefaultVelocity(-1)=default/unknown
	int velocity;
};
static const int DefaultVelocity = -1;


/// All notes on this channel are now silenced.
struct DLL_EXPORT NoteOffEvent: virtual public Event
{
	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);
};


/// Alter the way the note is currently being played on a channel.
struct DLL_EXPORT EffectEvent: virtual public Event
{
	enum EffectType {
		PitchbendNote, ///< Change note freq: data=new freq in milliHertz
		Volume,        ///< Change note volume: 0=silent, 255=loud
	};

	/// Type of effect.
	EffectType type;

	/// Effect data.  See EffectType.
	unsigned int data;

	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);
};


/// Configure the synthesiser's global parameters.
/**
 * This event can occur at any time.
 */
struct DLL_EXPORT ConfigurationEvent: virtual public Event
{
	/// What can be configured.
	/**
	 * For boolean values (Enable*) a value of 0 is false/disabled and a value
	 * of nonzero (usually 1) is true/enabled.
	 */
	enum ConfigurationType
	{
		/// No operation
		/**
		 * Dummy event that doesn't do anything.  Can be placed last in a file if
		 * there is a trailing delay.
		 */
		None,

		/// Enable OPL3 mode (or limit to OPL2)
		/**
		 * value: 1 for OPL3 mode, 0 for OPL2 mode
		 */
		EnableOPL3,

		/// Extend range of OPL tremolo
		/**
		 * value: bit0 = 1 to enable, 0 to disable
		 *        bit1 = 0-1 as chip index
		 */
		EnableDeepTremolo,

		/// Extend range of OPL vibrato
		/**
		 * value: bit0 = 1 to enable, 0 to disable
		 *        bit1 = 0-1 as chip index
		 */
		EnableDeepVibrato,

		/// Enable OPL rhythm mode
		/**
		 * value: 1 to enable, 0 to disable
		 *
		 * This is used by the CMF handler and MIDI controller event 0x67.
		 */
		EnableRhythm,

		/// Enable use of wave selection registers
		/**
		 * value: 1 to enable, 0 to disable
		 */
		EnableWaveSel,
	};

	/// What we are configuring
	ConfigurationType configType;

	/// What value we are setting (meaning dependent on config type)
	int value;

	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);
};

} // namespace gamemusic
} // namespace camoto

#include <camoto/gamemusic/music.hpp>

namespace camoto {
namespace gamemusic {

/// Callback interface
/**
 * Any class implementing this interface will be able to process different
 * events in typed functions, removing the need to dynamic_cast to figure
 * out the event type.
 */
class DLL_EXPORT EventHandler
{
	public:
		/// How to process events
		enum EventOrder {
			/// Handle all events in order, no matter what track they are in
			/**
			 * This processes all events at t=0, then all events at t=1, and so on.
			 * When the last row is reached, an "end of pattern" callback is issued.
			 * This effectively behaves as if all the tracks are merged into one
			 * stream, but of course each event's track can still be identified so
			 * channel information can be examined.
			 *
			 * This is best suited to formats that have a single track, like IMF
			 * or type-0 MIDI.  It is also useful for tracked formats which store
			 * data for each channel one full row at a time.
			 *
			 * If you use this layout, you won't have to deal with independent
			 * tracks, just a single big list of events (with corresponding channel
			 * numbers.)
			 */
			Pattern_Row_Track,

			/// Handle one track at a time in full before moving on to the next
			/**
			 * This processes all events in the first track, then all events in
			 * the second track, and so on.
			 *
			 * This is best suited to type-1 MIDI data, where the output file has a
			 * number of separate self-contained tracks, which are played back at the
			 * same time.
			 *
			 * When the last row in a track is reached, an "end of track" callback is
			 * issued and processing continues with the next track.  Once the last
			 * track has been processed, an "end of pattern" callback is issued.
			 */
			Pattern_Track_Row,

			/// Same as Pattern_Row_Track except respecting the order list.
			/**
			 * This means some patterns will be processed multiple times, so it should
			 * only be used when converting to formats that don't have a concept of
			 * reusable patterns.
			 */
			Order_Row_Track,

			/// Same as Pattern_Track_Row except respecting the order list.
			/**
			 * This means some patterns will be processed multiple times, so it should
			 * only be used when converting to formats that don't have a concept of
			 * reusable patterns.
			 */
			Order_Track_Row,
		};

		/// Callback when handleAllEvents() has reached the end of the track.
		/**
		 * Not called when EventOrder::Pattern_Row_Track is in use.
		 *
		 * @param delay
		 *   Number of ticks worth of silence until the end of the track.
		 */
		virtual void endOfTrack(unsigned long delay) = 0;

		/// Callback when handleAllEvents() has reached the end of a pattern.
		/**
		 * This callback is used when the last pattern is processed, but there
		 * is no 'end of song' callback.  But the end of the song is when
		 * handleAllEvents() returns, so any code that should run once the last
		 * pattern has been processed can just be put after handleAllEvents().
		 *
		 * @param delay
		 *   Number of ticks worth of silence until the end of the pattern.
		 */
		virtual void endOfPattern(unsigned long delay) = 0;

		/// The tempo is being changed.
		/**
		 * As delays are measured in ticks, and ticks are independent of the tempo,
		 * any events with a delay that crosses the tempo change will have the
		 * first of their ticks timed at the original tempo, and the last of their
		 * ticks timed at the new tempo.
		 *
		 * Be wary of this when a tempo change occurs in one track and there are
		 * events being processed in other tracks as well.
		 *
		 * @param delay
		 *   The number of ticks to delay (at the original tempo) before the
		 *   new tempo takes effect.
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev) = 0;

		/// A note is being played.
		/**
		 * If the instrument is incorrect for the song (e.g. OPL instrument on a
		 * PCM channel), the behaviour is undefined.  Typically the instrument
		 * number will be set anyway but it will correspond to the wrong patch.
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev) = 0;

		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev) = 0;

		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev) = 0;

		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev) = 0;

		void handleAllEvents(EventOrder eventOrder, ConstMusicPtr music);

	private:
		/// Merge the given pattern into a single track and process that.
		void processPattern_mergeTracks(const ConstMusicPtr& music,
			const PatternPtr& pattern, unsigned int patternIndex);

		/// Process the events in each track, track by track.
		void processPattern_separateTracks(const ConstMusicPtr& music,
			const PatternPtr& pattern, unsigned int patternIndex);

	protected:
		ConstMusicPtr music;  ///< Song being converted
};

/// Callback used for passing tempo-change events outside the EventHandler.
class DLL_EXPORT TempoCallback
{
	public:
		/// Change the length of the delay values for subsequent events.
		/**
		 * @param tempo
		 *   New tempo to use.
		 */
		virtual void tempoChange(const Tempo& tempo) = 0;
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEARCHIVE_EVENTS_HPP_
