/**
 * @file   music.hpp
 * @brief  Declaration of top-level Music class, for accessing files
 *         storing music data.
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

#ifndef _CAMOTO_GAMEMUSIC_MUSIC_HPP_
#define _CAMOTO_GAMEMUSIC_MUSIC_HPP_

#include <boost/shared_ptr.hpp>
#include <camoto/metadata.hpp>
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

	/// List of events in the song (note on, note off, etc.)
	EventVectorPtr events;

	/// List of metadata elements set.  Remove from map to unset.
	Metadata::TypeMap metadata;

	/// Number of ticks in a quarter note (one beat in 4/4 time), for arranging
	/// the song into measures and bars.
	unsigned int ticksPerQuarterNote;
};

/// Shared pointer to a Music instance.
typedef boost::shared_ptr<Music> MusicPtr;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUSIC_HPP_
