/**
 * @file  camoto/gamemusic/eventhandler.hpp
 * @brief Declaration of all the EventHandler base class.
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

#ifndef _CAMOTO_GAMEMUSIC_EVENTHANDLER_HPP_
#define _CAMOTO_GAMEMUSIC_EVENTHANDLER_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/events.hpp>

namespace camoto {
namespace gamemusic {

/// Callback interface
/**
 * Any class implementing this interface will be able to process different
 * events in typed functions, removing the need to dynamic_cast to figure
 * out the event type.
 */
class CAMOTO_GAMEMUSIC_API EventHandler
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
		 *
		 * @param trackIndex
		 *   Zero-based index of the track the event came from.  This is often used
		 *   to look up (via Music::trackInfo) what channel the track will be
		 *   played on.
		 *
		 * @param patternIndex
		 *   Index of the pattern the event is being played on.
		 *
		 * @param ev
		 *   The event to process.
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev) = 0;

		/// A note is being played.
		/**
		 * If the instrument is incorrect for the song (e.g. OPL instrument on a
		 * PCM channel), the behaviour is undefined.  Typically the instrument
		 * number will be set anyway but it will correspond to the wrong patch.
		 *
		 * @see handleEvent(unsigned long, unsigned int, unsigned int, const TempoEvent *)
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev) = 0;

		/// A note has finished playing.
		/**
		 * @see handleEvent(unsigned long, unsigned int, unsigned int, const TempoEvent *)
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev) = 0;

		/// An effect is being applied.
		/**
		 * @see handleEvent(unsigned long, unsigned int, unsigned int, const TempoEvent *)
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev) = 0;

		/// An jump is being performed.
		/**
		 * @see handleEvent(unsigned long, unsigned int, unsigned int, const TempoEvent *)
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const GotoEvent *ev) = 0;

		/// A global song parameter is being changed.
		/**
		 * @see handleEvent(unsigned long, unsigned int, unsigned int, const TempoEvent *)
		 */
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev) = 0;

		/// Process all the events in a given song.
		/**
		 * @see handleEvent(unsigned long, unsigned int, unsigned int, const TempoEvent *)
		 */
		void handleAllEvents(EventOrder eventOrder, const Music& music);

	private:
		/// Merge the given pattern into a single track and process that.
		/**
		 * @param music
		 *   Song being processed.
		 *
		 * @param pattern
		 *   Pattern to process this time.
		 *
		 * @param patternIndex
		 *   Zero-based index of \a pattern.
		 */
		void processPattern_mergeTracks(const Music& music, const Pattern& pattern,
			unsigned int patternIndex);

		/// Process the events in each track, track by track.
		/**
		 * @copydetails processPattern_mergeTracks
		 */
		void processPattern_separateTracks(const Music& music,
			const Pattern& pattern, unsigned int patternIndex);

	protected:
		std::shared_ptr<const Music> music;  ///< Song being converted
};

/// Callback used for passing tempo-change events outside the EventHandler.
class CAMOTO_GAMEMUSIC_API TempoCallback
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

#endif // _CAMOTO_GAMEMUSIC_EVENTHANDLER_HPP_
