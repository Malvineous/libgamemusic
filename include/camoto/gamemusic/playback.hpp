/**
 * @file   playback.hpp
 * @brief  Helper class for managing song playback.
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

#ifndef _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_
#define _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/synth-opl.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>

namespace camoto {
namespace gamemusic {

/// Helper class to assist with song playback.
class Playback: public OPLWriterCallback
{
	public:
		struct Position {
			Position();

			bool changed;
			unsigned int loop;
			unsigned int order;
			unsigned int row;
			bool end;
			Tempo tempo;
		};

		Playback(unsigned long sampleRate, unsigned int channels,
			unsigned int bits);
		~Playback();

		void setSong(ConstMusicPtr music);
		void setLoopCount(unsigned int count);
		void seekByOrder(unsigned int destOrder);
		void seekByTime(unsigned long ms);

		/// Synthesize and mix audio into the given buffer.
		/**
		 * @param samples
		 *   Size of output, in samples.  One sample is one int16_t, and two
		 *   samples is needed for one frame of stereo audio (four bytes).
		 */
		void generate(int16_t *output, unsigned long samples, Position *pos);

		// OPLWriterCallback
		virtual void writeNextPair(const OPLEvent *oplEvent);
		virtual void writeTempoChange(const Tempo& tempo);

	protected:
		unsigned long outputSampleRate; ///< in Hertz, e.g. 44100
		unsigned int outputChannels;    ///< e.g. 2 for stereo
		unsigned int outputBits;        ///< e.g. 16 for 16-bit output

		ConstMusicPtr music;
		unsigned int loopCount; ///< 0=loop forever, 1=no loop, 2=loop once, etc.

	private:
		// These are kept as indices rather than iterators so that the song can be
		// modified in between calls to generate() without causing problems.
		bool end;
		unsigned int loop;
		unsigned int order;
		unsigned int pattern;
		unsigned int row;
		unsigned int frame;
		Tempo tempo;

		unsigned int framesPerRow; ///< Current speed
		unsigned int samplesPerFrame;

		/// A single frame of audio, copied into the output buffer as needed
		std::vector<int16_t> frameBuffer;
		unsigned int frameBufferPos;

		SynthOPL opl;
		boost::shared_ptr<EventConverter_OPL> oplConverter;

		/// Populate frameBuffer with the next frame
		void nextFrame();
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_
