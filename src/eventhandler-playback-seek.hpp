/**
 * @file  eventhandler-playback-seek.hpp
 * @brief EventHandler implementation that can be used to seek by time.
 *
 * Copyright (C) 2010-2016 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_EVENTHANDLER_PLAYBACK_SEEK_HPP_
#define _CAMOTO_GAMEMUSIC_EVENTHANDLER_PLAYBACK_SEEK_HPP_

#include <camoto/enum-ops.hpp>
#include <camoto/gamemusic/eventhandler.hpp>

#ifndef CAMOTO_GAMEMUSIC_API
#define CAMOTO_GAMEMUSIC_API
#endif

namespace camoto {
namespace gamemusic {

class CAMOTO_GAMEMUSIC_API EventHandler_Playback_Seek: virtual public EventHandler
{
	public:
		/// Prepare for event examination.
		/**
		 * @param music
		 *   Song to be examined.
		 *
		 * @param loopCount
		 *   Number of times the song will play.  1 == play once, 2 == play twice
		 *   (loop once).  Since 0 means loop forever, this value will be treated
		 *   the same as looping once.
		 */
		EventHandler_Playback_Seek(std::shared_ptr<const Music> music,
			unsigned long loopCount);

		virtual ~EventHandler_Playback_Seek();

		/// Prepare to start from the first event.
		/**
		 * This function must be called before re-sending an old event, otherwise
		 * the resulting negative delay will cause an extremely long pause.
		 */
		void rewind();

		// EventHandler functions
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const GotoEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

		/// Calculate the total length of the song, in milliseconds.
		/**
		 * @return Length of the song in milliseconds.
		 */
		unsigned long getTotalLength();

		/// Calculate the position of a specific moment in time.
		/**
		 * @param msTarget
		 *   The moment in time to reach.
		 *
		 * @param destTempo
		 *   The tempo at the target seek point.
		 *
		 * @return The actual position reached.  The values are available in
		 *   microseconds, order, row, etc.  This will be close to the desired time
		 *   supplied in msTarget, except that seeking is only done to the row
		 *   level, so it will differ from the target time by a few milliseconds.
		 */
		Position seekTo(unsigned long msTarget, Tempo *destTempo);

	protected:
		std::shared_ptr<const Music> music; ///< Song being examined
		unsigned long loopCount;            ///< Number of times to play song

		double usTarget;   ///< Target seek time, in microseconds, -1 for no target
		double usTotal;    ///< Current song length in microseconds
		double usPerTick;  ///< Current tempo, microseconds per tick
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_EVENTHANDLER_PLAYBACK_SEEK_HPP_
