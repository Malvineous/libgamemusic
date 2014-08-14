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
#include <camoto/gamemusic/synth-pcm.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>

namespace camoto {
namespace gamemusic {

/// Helper class to assist with song playback.
class Playback: virtual public OPLWriterCallback,
                virtual public SynthPCMCallback
{
	public:
		struct Position
		{
			Position();

			/// Number of times the song has looped.
			unsigned int loop;

			/// Order number (0 to largest order+1)
			/**
			 * The order number starts at 0 and after the last note is played (when
			 * \ref end is set to true) this value will equal the number of orders
			 * in the song.  If you are using this field as an index into the order
			 * list, be careful to check that it is in range first, as it will go out
			 * of range when the end of the song has been reached!
			 */
			unsigned int order;

			/// Row index within pattern.  Starts at 0.
			unsigned int row;

			/// True if the end of the song has been reached.
			bool end;

			/// Current tempo at this point in the song.  May be different to the
			/// song's initial tempo.
			Tempo tempo;

			bool operator== (const Playback::Position& b);

			inline bool operator!= (const Playback::Position& b)
			{
				return !(*this == b);
			}
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

		// TempoCallback
		virtual void tempoChange(const Tempo& tempo);

		// OPLWriterCallback
		virtual void writeNextPair(const OPLEvent *oplEvent);

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

		unsigned int samplesPerFrame;

		/// A single frame of audio, copied into the output buffer as needed
		std::vector<int16_t> frameBuffer;
		unsigned int frameBufferPos;

		SynthPCM pcm;
		SynthOPL opl;
		boost::shared_ptr<EventConverter_OPL> oplConverter;

		/// Populate frameBuffer with the next frame
		void nextFrame();
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_
