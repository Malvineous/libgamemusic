/**
 * @file  track-split.hpp
 * @brief Split tracks with multiple notes into separate tracks.
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

#ifndef _CAMOTO_GAMEMUSIC_TRACK_SPLIT_HPP_
#define _CAMOTO_GAMEMUSIC_TRACK_SPLIT_HPP_

#include <camoto/gamemusic/events.hpp>

namespace camoto {
namespace gamemusic {

/// Just one note on this channel is silenced.
struct CAMOTO_GAMEMUSIC_API SpecificNoteOffEvent: virtual public NoteOffEvent
{
	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);

	/// Note frequency (440000 == 440Hz)
	unsigned int milliHertz;
};

/// Just one note on this channel is having an effect applied.
struct CAMOTO_GAMEMUSIC_API SpecificNoteEffectEvent: virtual public EffectEvent
{
	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);

	/// Note frequency (440000 == 440Hz)
	unsigned int milliHertz;
};

/// Similar to effect event but data stored differently to apply to all notes
/// in the track.
struct CAMOTO_GAMEMUSIC_API PolyphonicEffectEvent: virtual public EffectEvent
{
	enum class Type {
		PitchbendChannel, ///< Change note freq of all current notes: data=0..8192..16384 for -2..0..+2 semitones
		VolumeChannel, ///< Change volume (velocity aftertouch) of all current notes: data 0-255
	};

	virtual std::string getContent() const;

	virtual void processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler);
};

/// Split tracks as needed to ensure only one note at a time is on each track.
/**
 * Run through all tracks in the song and move any polyphonic notes onto
 * separate tracks so that only monophonic tracks exist upon return.
 */
void CAMOTO_GAMEMUSIC_API splitPolyphonicTracks(Music& music);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_TRACK_SPLIT_HPP_
