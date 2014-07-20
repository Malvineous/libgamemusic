/**
 * @file   music.hpp
 * @brief  Declaration of top-level Music class, for in-memory representation
 *         of all music formats.
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

#ifndef _CAMOTO_GAMEMUSIC_MUSIC_HPP_
#define _CAMOTO_GAMEMUSIC_MUSIC_HPP_

#include <math.h>
#include <boost/shared_ptr.hpp>
#include <camoto/metadata.hpp>
#include <camoto/gamemusic/tempo.hpp>

namespace camoto {
namespace gamemusic {

struct Music;

/// Shared pointer to a Music instance.
typedef boost::shared_ptr<Music> MusicPtr;

} // namespace gamemusic
} // namespace camoto

#include <camoto/gamemusic/patchbank.hpp>
#include <camoto/gamemusic/events.hpp>

namespace camoto {
namespace gamemusic {

/// In-memory representation of a single song.
/**
 * This class represents a single song in an arbitrary file format.  The
 * instruments (patches) can be read and modified, as can the events making up
 * the melody and percussion.  Care must be taken when writing a Music object
 * back to a file, as most file formats only support differing subsets of the
 * capabilities presented here.
 */
struct Music {

	/// PatchBank holding all instruments in the song.
	PatchBankPtr patches;

	/// List of all tracks in the song and their channel allocations.
	/**
	 * A pattern always has the same number of tracks as there are entries in
	 * this vector, and the allocation specified in this vector holds true for
	 * all tracks in all patterns.
	 */
	TrackInfoVector trackInfo;

	/// List of events in the song (note on, note off, etc.)
	/**
	 * Each entry in the vector is one pattern, referred to by index into the
	 * vector.  Patterns are played one after the other, in the order given by
	 * \ref patternOrder.
	 */
	std::vector<PatternPtr> patterns;

	/// Which order the above patterns play in.
	/**
	 * A value of 1 refers to the second entry in \ref patterns.
	 */
	std::vector<unsigned int> patternOrder;

	/// Loop destination.
	/**
	 * -1 means no loop, otherwise this value is the index into
	 * \ref patternOrder where playback will jump to once the last entry in
	 * \ref patternOrder has been played.
	 *
	 * Note that an effect can also cause a loop independently of this value.
	 */
	int loopDest;

	/// List of events in the song (note on, note off, etc.)
//	EventVectorPtr events;

	/// List of metadata elements set.  Remove from map to unset.
	Metadata::TypeMap metadata;

	/// Initial song tempo, time signature, etc.
	/**
	 * This is the value the song starts with.  The actual tempo can be changed
	 * during playback, but this value always contains the song's starting tempo.
	 */
	Tempo initialTempo;
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUSIC_HPP_
