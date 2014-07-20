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

/// Collection of methods for managing playback speeds and time signatures
struct Tempo
{
	/// Number of beats in one bar.
	/**
	 * For example in 3/4 time, this value is 3.  Only used to assist with
	 * correctly arranging notes into bars.
	 */
	unsigned int beatsPerBar;

	/// Note length of each beat.
	/**
	 * In 3/4 time, this value is 4.  Only used to assist with correctly
	 * arranging notes into bars.
	 */
	unsigned int beatLength;

	/// Number of ticks in a single beat.
	/**
	 * This value is used to calculate speeds based around note lengths and beats
	 * like "ticks per quarter note" or beats per minute.
	 *
	 * If the song is in /4 time (i.e. each beat is a quarter note), then this
	 * value is of course the same as the number of ticks per quarter note.
	 */
	unsigned int ticksPerBeat;

	/// Number of microseconds per tick.
	/**
	 * Event::absTime values are in ticks.  Two events with an absTime
	 * difference of 1 would occur this many microseconds apart.
	 *
	 * This controls the actual playback speed.  None of the other values control
	 * the speed, they all assist with notation rendering and converting tempo
	 * values to and from other units.
	 */
	double usPerTick;

	/// Number of effect frames per tick.
	/**
	 * A tick is analogous to a row in a .mod file and is equal to the least
	 * amount of time possible between two non-simultaneous events.  Some effects
	 * like retrig cause multiple audible changes between two rows, i.e. at
	 * intervals of less than one tick.  This value is used to set how finely
	 * ticks can be subdivided for these effect frames.  If this value is set to
	 * 2 for example, there will be two retrigs between rows (for those rows where
	 * the retrig effect is used.)
	 */
	unsigned int framesPerTick;

	static const double US_PER_SEC = 1000000.0d;

	inline Tempo()
		:	beatsPerBar(4),
			beatLength(4),
			ticksPerBeat(2),
			usPerTick(250000), // 120 BPM
			framesPerTick(6)
	{
	}

	/// Set the tempo by beats per minute.
	/**
	 * @pre \ref ticksPerBeat is valid and correct for the song.
	 *   ticksPerQuarterNote() can be used first to set this value.
	 *
	 * @post \ref usPerTick is changed to achieve the desired BPM.
	 */
	inline void bpm(unsigned int bpm)
	{
		this->usPerTick = 60.0d * US_PER_SEC / (this->ticksPerBeat * bpm);
		return;
	}

	/// Get the tempo as a number of beats per minute.
	inline unsigned int bpm(void)
	{
		return round(60.0d * US_PER_SEC / ((double)this->ticksPerBeat * this->usPerTick));
	}

	/// Set the tempo by the number of ticks in a quarter note.
	/**
	 * @pre \ref beatLength is valid and correct for the song.
	 *
	 * @post \ref ticksPerBeat is changed to achieve the desired number of ticks
	 *   per quarter note.
	 */
	inline void ticksPerQuarterNote(unsigned int ticks)
	{
		this->ticksPerBeat = this->beatLength / 4 * ticks;
		return;
	}

	/// Get the tempo as the number of ticks in a quarter note.
	inline unsigned int ticksPerQuarterNote(void)
	{
		return this->beatLength / 4 * this->ticksPerBeat;
	}

	/// Set the tempo as a .mod speed and tempo value.
	/**
	 * @post \ref usPerTick is changed to achieve the desired number of ticks per
	 *   second.
	 */
	inline void module(unsigned int speed, unsigned int tempo)
	{
		//this->usPerTick = (double)tempo * US_PER_SEC / (double)speed;
		this->usPerTick = US_PER_SEC / (double)tempo * (double)speed;
		this->framesPerTick = speed;
		return;
	}

	/// Get the tempo as a mod "speed" value.
	inline unsigned int module_speed(void)
	{
		assert(this->framesPerTick != 0);
		return this->framesPerTick;
	}

	/// Get the tempo as a mod "tempo" value.
	inline unsigned int module_tempo(void)
	{
		assert(this->usPerTick != 0);
		//return round(this->usPerTick * (double)this->framesPerTick / US_PER_SEC);
		return round((US_PER_SEC / this->usPerTick) * (double)this->framesPerTick);
	}

	/// Set the tempo as ticks per second.
	/**
	 * @post \ref usPerTick is changed to achieve the desired number of ticks per
	 *   second.
	 */
	inline void hertz(unsigned long uhz)
	{
		assert(uhz != 0);
		this->usPerTick = US_PER_SEC / (double)uhz;
		assert(this->usPerTick != 0);
		return;
	}

	/// Get the tempo as the number of ticks per second.
	inline unsigned long hertz(void)
	{
		assert(this->usPerTick != 0);
		return round(US_PER_SEC / this->usPerTick);
	}

	/// Set the tempo as milliseconds per tick.
	/**
	 * @pre ms > 0
	 *
	 * @post \ref usPerTick is changed to achieve the desired number of ticks per
	 *   second.
	 */
	inline void msPerTick(unsigned long ms)
	{
		assert(ms != 0);
		this->usPerTick = ms * 1000;
		return;
	}

	/// Get the tempo as the number of ticks per millisecond.
	/**
	 * Divide the result by 1000 to get Hertz.
	 */
	inline unsigned long msPerTick(void)
	{
		assert(this->usPerTick != 0);
		return round(this->usPerTick / 1000.0d);
	}
};

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
