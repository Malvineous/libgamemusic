/**
 * @file  camoto/gamemusic/synth-pcm.hpp
 * @brief Implementation of a synthesiser that uses PCM samples.
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

#ifndef _CAMOTO_GAMEMUSIC_SYNTH_PCM_HPP_
#define _CAMOTO_GAMEMUSIC_SYNTH_PCM_HPP_

#include <stdint.h>
#include <boost/shared_array.hpp>
#include <camoto/gamemusic/events.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>

namespace camoto {
namespace gamemusic {

class DLL_EXPORT SynthPCMCallback: virtual public TempoCallback
{
};

typedef boost::shared_ptr<SynthPCMCallback> SynthPCMCallbackPtr;

/// Interface to an PCM/FM/Adlib synthesizer.
class DLL_EXPORT SynthPCM: virtual public EventHandler
{
	public:
		/// Constructor
		/**
		 * @param sampleRate
		 *   Sample rate to generate output audio, in Hertz.
		 *
		 * @param cb
		 *   Callback to process events requiring external changes, like tempo
		 *   changes.  The caller must keep the object alive while the SynthPCM
		 *   object is alive.  This is done so that a class can pass 'this' as
		 *   the callback parameter.
		 *
		 * @post Object is in initial state, no need to call reset().
		 */
		SynthPCM(unsigned long sampleRate, SynthPCMCallback *cb);
		~SynthPCM();

		/// Reset the synthesiser to initial state.
		/**
		 * @post Object is in same state as it is just following the constructor.
		 */
		void reset(const TrackInfoVector *trackInfo, PatchBankPtr patches);

		/// Synthesize and mix one frame of audio into the given buffer.
		/**
		 * @post Any active effects that change on each frame are updated to then
		 *   next frame.
		 */
		void mix(int16_t *output, unsigned long len);

		/// Set a MIDI patch bank to use.
		/**
		 * If a patch bank is set that contains PCM instruments, then events on
		 * MIDI channels will be played using the bank given here.  MIDI events
		 * can be ignored again by setting an empty patch bank here.
		 *
		 * @param bankMIDI
		 *   Patch bank to use.  An empty patch bank will mute any MIDI events.
		 *   A supplied patch bank will mute any PCM events.
		 *   The patch bank can contain different instrument types - only PCM
		 *   instruments will be played.
		 *   Entries 0 to 127 inclusive are for GM instruments, entries 128 to 255
		 *   are for percussion (128=note 0, 129=note 1, etc.)
		 */
		void setBankMIDI(PatchBankPtr bankMIDI);

		// EventHandler overrides
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

	protected:
		unsigned long outputSampleRate;   ///< in Hertz, e.g. 44100
		SynthPCMCallback *cb;             ///< Callback for tempo change events
		const TrackInfoVector *trackInfo; ///< Track to channel assignments
		PatchBankPtr patches;             ///< Patch bank
		PatchBankPtr bankMIDI;            ///< Optional patch bank for MIDI notes

		struct Sample {
			unsigned long track;      ///< Source track (for finding note again)
			unsigned long sampleRate; ///< Playback sample rate for this note
			PCMPatchPtr patch;
			unsigned long pos; ///< Number of samples played at outputSampleRate
			unsigned int vol; // 0..255
		};
		typedef std::vector<Sample> SampleVector;
		SampleVector activeSamples;

		/// Switch all notes off on the given track.
		void noteOff(unsigned int trackIndex);
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_SYNTH_PCM_HPP_
