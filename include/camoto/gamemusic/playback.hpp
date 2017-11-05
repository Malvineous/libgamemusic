/**
 * @file  camoto/gamemusic/playback.hpp
 * @brief Helper class for managing song playback.
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

#ifndef _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_
#define _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/synth-opl.hpp>
#include <camoto/gamemusic/synth-pcm.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>

namespace camoto {
namespace gamemusic {

/// Helper class to assist with song playback.
class CAMOTO_GAMEMUSIC_API Playback: virtual public SynthPCMCallback
{
	public:
		struct CAMOTO_GAMEMUSIC_API Position
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
			unsigned long row;

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

		class CAMOTO_GAMEMUSIC_API OPLHandler : virtual public OPLWriterCallback
		{
			public:
				OPLHandler(Playback *playback, bool midi);

				// OPLWriterCallback
				virtual void writeNextPair(const OPLEvent *oplEvent);

			protected:
				Playback *playback;
				bool midi;
		};

		Playback(unsigned long sampleRate, unsigned int channels,
			unsigned int bits);
		~Playback();

		/// Set the MIDI patch bank to use for MIDI events.
		/**
		 * @param bankMIDI
		 *   OPL or PCM patch bank to use.  Obviously this can't be a MIDI patch
		 *   bank as the point is to translate the MIDI patches to something
		 *   audible.
		 */
		void setBankMIDI(std::shared_ptr<const PatchBank> bankMIDI);

		/// Set the song to play.
		/**
		 * This also resets playback to the start of the song.
		 *
		 * @param music
		 *   The song to play.
		 */
		void setSong(std::shared_ptr<const Music> music);

		/// Set the number of times the song should loop.
		/**
		 * @param count
		 *   The number of times the song should be played.  1 means play once, 2
		 *   means play twice (so loop once), and 0 means loop forever.
		 */
		void setLoopCount(unsigned int count);

		/// Get the length of the song, in milliseconds.
		/**
		 * @return The length of the song in milliseconds.
		 */
		unsigned long getLength();

		/// Jump to a specific point in the song, specified by order number.
		/**
		 * @param destOrder
		 *   The order to start playing from.  0 will jump back to the start of the
		 *   song.  An order number that is out of range will immediately end the
		 *   song, or jump back to the loop point if the song is set to loop.
		 */
		void seekByOrder(unsigned int destOrder);

		/// Jump to a specific point in the song, specified in milliseconds.
		/**
		 * @param ms
		 *   The new playback point, in milliseconds.  The actual playback point
		 *   will always be at the start of a row, so the playback time may not
		 *   be exactly at this number of milliseconds, but will be as close to
		 *   it as possible.
		 *
		 * @return Actual seek position, in milliseconds.  Will be similar to the
		 *   input parameter, just rounded to the start of the row.
		 */
		unsigned long seekByTime(unsigned long ms);

		/// Synthesize and mix audio into the given buffer.
		/**
		 * @param output
		 *   Input and output buffer.  Synthesized audio is mixed into this buffer
		 *   and combined with whatever audio is already in it.  Make sure you zero
		 *   the buffer with memset() before the first call!
		 *
		 * @param samples
		 *   Size of output, in samples.  One sample is one int16_t, and two
		 *   samples is needed for one frame of stereo audio (four bytes).
		 *
		 * @param pos
		 *   Pointer to a structure that on return, will receive the playback
		 *   position of the data just placed in the buffer.
		 */
		void mix(int16_t *output, unsigned long samples, Position *pos);

		/// Switch all playing notes off.  Notes will still linger as they fade out.
		void allNotesOff();

	protected:
		unsigned long outputSampleRate; ///< in Hertz, e.g. 44100
		unsigned int outputChannels;    ///< e.g. 2 for stereo
		unsigned int outputBits;        ///< e.g. 16 for 16-bit output

		std::shared_ptr<const Music> music;
		unsigned int loopCount; ///< 0=loop forever, 1=no loop, 2=loop once, etc.

		std::map<const void *, unsigned int> loopEvents;

		// TempoCallback
		virtual void tempoChange(const Tempo& tempo);

	private:
		// These are kept as indices rather than iterators so that the song can be
		// modified in between calls to generate() without causing problems.
		bool end;
		unsigned int loop;
		unsigned int order;
		unsigned int pattern;
		unsigned int row;
		unsigned int frame;
		unsigned int nextRow;
		unsigned int nextOrder;
		Tempo tempo;

		unsigned int samplesPerFrame;

		/// A single frame of audio, copied into the output buffer as needed
		std::vector<int16_t> frameBuffer;
		unsigned int frameBufferPos;

		/// Optional patch bank for MIDI notes
		std::shared_ptr<const PatchBank> bankMIDI;

		SynthPCM pcm;
		SynthPCM pcmMIDI;
		SynthOPL opl;
		SynthOPL oplMIDI;
		OPLHandler oplHandler;
		OPLHandler oplHandlerMIDI;
		std::shared_ptr<EventConverter_OPL> oplConverter;
		std::shared_ptr<EventConverter_OPL> oplConvMIDI;

		/// Populate frameBuffer with the next frame
		void nextFrame();
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PLAYBACK_HPP_
