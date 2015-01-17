/**
 * @file   music.hpp
 * @brief  Declaration of top-level Music class, for in-memory representation
 *         of all music formats.
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

/// Shared pointer to an unmodifiable Music instance.
typedef boost::shared_ptr<const Music> ConstMusicPtr;

} // namespace gamemusic
} // namespace camoto

#include <camoto/gamemusic/patchbank.hpp>
#include <camoto/gamemusic/events.hpp>

namespace camoto {
namespace gamemusic {

/// Information about a track, shared across all patterns.
/**
 * This controls which channel a track's events are played on.
 */
struct DLL_EXPORT TrackInfo
{
	/// Channel type
	enum ChannelType {
		UnusedChannel,
		AnyChannel,
		OPLChannel,
		OPLPercChannel,
		MIDIChannel,
		PCMChannel,
	};

	/// What type of channel this track will be played through.
	ChannelType channelType;

	/// Channel index.
	/**
	 * When type is:
	 *
	 * - AnyChannel: this value is 0-255.  The value
	 *   is not so important, but only one note can be played on each channel
	 *   at a time, across all tracks.  It is not valid to write any tracks
	 *   with this channel type set.  All AnyChannel tracks must be mapped to
	 *   other values before a song is passed to a format writer.  This is to
	 *   prevent every format writer from having to map channels itself.
	 *   @todo: Make a utility function to do this.
	 *
	 * - OPLChannel: this value is 0 to 8 for normal OPL channels on
	 *   chip 1, and 9 to 17 for chip 2.  Some events are global and will affect
	 *   the whole chip regardless of what track they are played on.
	 *
	 * - OPLPercChannel: this value is 4 for bass drum, 3 for snare, 2 for tomtom,
	 *   1 for top cymbal, 0 for hi-hat.  Other values are invalid.
	 *
	 * - MIDIChannel: this value is 0 to 15, with 9 being percussion.
	 *
	 * - PCMChannel: this value is the channel index starting at 0.  For some
	 *   formats like .mod, this affects the panning of the channel.
	 *
	 * Note that OPL percussion mode uses channels 6, 7 and 8, so it is not
	 * valid for a song to have OPLChannel events on these channels while
	 * OPLPercChannel events are present.  This may happen temporarily during a
	 * format conversion, but this state must be resolved by the time the
	 * data is written out to a file.
	 */
	unsigned int channelIndex;
};

/// Vector of trackinfo (a channel map)
typedef std::vector<TrackInfo> TrackInfoVector;

/// In-memory representation of a single song.
/**
 * This class represents a single song in an arbitrary file format.  The
 * instruments (patches) can be read and modified, as can the events making up
 * the melody and percussion.  Care must be taken when writing a Music object
 * back to a file, as most file formats only support differing subsets of the
 * capabilities presented here.
 */
struct Music
{
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

	/// Total number of ticks in each track.
	/**
	 * This is the same for all tracks in all patterns in the song.
	 */
	unsigned int ticksPerTrack;

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
