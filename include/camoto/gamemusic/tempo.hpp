/**
 * @file   tempo.hpp
 * @brief  Declaration of Tempo object and conversion functions.
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

#ifndef _CAMOTO_GAMEMUSIC_TEMPO_HPP_
#define _CAMOTO_GAMEMUSIC_TEMPO_HPP_

#include <math.h>

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
	inline unsigned int bpm(void) const
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
	inline unsigned int ticksPerQuarterNote(void) const
	{
		return this->beatLength / 4 * this->ticksPerBeat;
	}

	/// Set the tempo by the number of ticks in a quarter note.
	/**
	 * @pre \ref beatLength is valid and correct for the song.
	 *
	 * @post \ref ticksPerBeat is changed to achieve the desired number of ticks
	 *   per quarter note.
	 */
	inline void usPerQuarterNote(unsigned int us)
	{
		this->usPerTick = (double)us / this->ticksPerQuarterNote();
		return;
	}

	/// Get the tempo as the number of ticks in a quarter note.
	inline unsigned int usPerQuarterNote(void) const
	{
		return this->usPerTick * this->ticksPerQuarterNote();
	}

	/// Set the tempo as a .mod speed and tempo value.
	/**
	 * @post \ref usPerTick is changed to achieve the desired number of ticks per
	 *   second.
	 */
	inline void module(unsigned int speed, unsigned int tempo)
	{
		double modTicksPerSec = tempo * 2 / 5;
		this->usPerTick = US_PER_SEC / modTicksPerSec * (double)speed;
		this->framesPerTick = speed;
		return;
	}

	/// Get the tempo as a mod "speed" value.
	inline unsigned int module_speed(void) const
	{
		assert(this->framesPerTick != 0);
		return this->framesPerTick;
	}

	/// Get the tempo as a mod "tempo" value.
	inline unsigned int module_tempo(void) const
	{
		assert(this->usPerTick != 0);
		double modTicksPerSec = (US_PER_SEC / this->usPerTick) * (double)this->framesPerTick;
		return round(modTicksPerSec * 5 / 2);
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
	inline unsigned long hertz(void) const
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
	inline unsigned long msPerTick(void) const
	{
		assert(this->usPerTick != 0);
		return round(this->usPerTick / 1000.0d);
	}
};

inline bool operator== (const Tempo& a, const Tempo& b)
{
	return
		(a.beatsPerBar == b.beatsPerBar)
		&& (a.beatLength == b.beatLength)
		&& (a.ticksPerBeat == b.ticksPerBeat)
		&& (a.usPerTick == b.usPerTick)
		&& (a.framesPerTick == b.framesPerTick)
	;
}

inline bool operator!= (const Tempo& a, const Tempo& b)
{
	return !(a == b);
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_TEMPO_HPP_
