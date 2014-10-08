/**
 * @file   synth-pcm.cpp
 * @brief  Implementation of a synthesiser that uses PCM samples.
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

#include <algorithm>
#include <assert.h>
#include <camoto/util.hpp>
#include <camoto/gamemusic/synth-pcm.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/util-pcm.hpp>

using namespace camoto;
using namespace camoto::gamemusic;

/// How much to dampen the maximum possible volume by
#define VOL_DAMPEN 4

/// Middle-C frequency in milliHertz
#define FREQ_MIDDLE_C 261625

SynthPCM::SynthPCM(unsigned long sampleRate, SynthPCMCallback *cb)
	:	outputSampleRate(sampleRate),
		cb(cb)
{
}

SynthPCM::~SynthPCM()
{
}

void SynthPCM::setBankMIDI(PatchBankPtr bankMIDI)
{
	this->bankMIDI = bankMIDI;
	return;
}

void SynthPCM::reset(const TrackInfoVector *trackInfo, PatchBankPtr patches)
{
	this->trackInfo = trackInfo;
	this->patches = patches;
	return;
}

void SynthPCM::mix(int16_t *output, unsigned long len)
{
	len /= 2; // stereo
	// TODO: Lock mutex
	for (SampleVector::iterator
		i = this->activeSamples.begin(); i != this->activeSamples.end(); /* i++ */
	) {
		Sample& sample = *i;
		int16_t *output_cur = output;
		bool complete = false;
		uint8_t *data = sample.patch->data.get();
		unsigned long lenInput = sample.patch->loopEnd ? sample.patch->loopEnd : sample.patch->lenData;
		unsigned long numOutputSamples = lenInput * this->outputSampleRate / sample.sampleRate;
		for (unsigned long j = 0; j < len; j++) {

			// Check if we have reached the end of the sample
			if (sample.pos >= numOutputSamples) {
				// We have, so either loop if the sample supports this or silence it
				if (sample.patch->loopEnd) {
					sample.pos = sample.patch->loopStart;
					if (sample.patch->bitDepth == 16) sample.pos *= 2;
					sample.pos = sample.pos * this->outputSampleRate / sample.sampleRate;
					if (sample.pos >= numOutputSamples) {
						std::cout << "synth-pcm: Silencing instrument with loop start "
							"beyond end of sample\n";
						complete = true;
						break;
					}
				} else {
					complete = true;
					break;
				}
			}
			unsigned long posInput = lenInput * sample.pos / numOutputSamples;
			assert(posInput < lenInput);
			uint8_t *in = data + posInput;

			int16_t s;
			if (sample.patch->bitDepth == 8) {
				// Convert from 8-bit unsigned to 16-bit signed
				s = pcm_u8_to_s16(*in);
				sample.pos++;
			} else if (sample.patch->bitDepth == 16) {
				s = *((int16_t *)in);
				sample.pos += 2;
			} else {
				std::cerr << "synth-pcm: Unsupported playback bit depth: "
					<< sample.patch->bitDepth << "\n";
				complete = true;
				break;
			}

			// Volume adjustment
			assert(sample.vol < 256);
			s = (s * (int32_t)sample.vol / 255) / VOL_DAMPEN;

			*output_cur = pcm_mix_s16(*output_cur, s);
			output_cur++;
			*output_cur = pcm_mix_s16(*output_cur, s);
			output_cur++;
		}
		if (complete) {
			i = this->activeSamples.erase(i);
		} else {
			i++;
		}
		// TODO: Release mutex
	}
	return;
}

void SynthPCM::endOfTrack(unsigned long delay)
{
	return;
}

void SynthPCM::endOfPattern(unsigned long delay)
{
	return;
}

void SynthPCM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->cb->tempoChange(ev->tempo);
	return;
}

void SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const NoteOnEvent *ev)
{
	assert(this->patches);
	assert(trackIndex < this->trackInfo->size());
	assert(ev->velocity < 256);

	if (ev->instrument >= this->patches->size()) {
		throw bad_patch(createString("Instrument bank too small - tried to play "
			"note with instrument #" << ev->instrument + 1
			<< " but patch bank only has " << this->patches->size()
			<< " instruments."));
	}
	PatchPtr patch = this->patches->at(ev->instrument);

	const TrackInfo& ti = this->trackInfo->at(trackIndex);
	if (this->bankMIDI) {
		// We are handling MIDI events
		if (
			(ti.channelType != TrackInfo::MIDIChannel)
			&& (ti.channelType != TrackInfo::AnyChannel)
		) {
			// Not a MIDI track
			return;
		}
		MIDIPatchPtr instMIDI = boost::dynamic_pointer_cast<MIDIPatch>(patch);
		if (!instMIDI) return; // non-MIDI instrument on a MIDI channel, ignore
		unsigned long target = instMIDI->midiPatch;
		if (instMIDI->percussion) {
			target += MIDI_PATCHES;
		}
		if (target < this->bankMIDI->size()) {
			patch = this->bankMIDI->at(target);
		} else {
			// No patch, bank too small
			return;
		}
	} else {
		// We are handling PCM events
		if (
			(ti.channelType != TrackInfo::PCMChannel)
			&& (ti.channelType != TrackInfo::AnyChannel)
		) {
			// Not a PCM track
			return;
		}
	}
	PCMPatchPtr inst = boost::dynamic_pointer_cast<PCMPatch>(patch);

	// Don't play this note if there's no patch for it
	if (!inst) return;

	this->noteOff(trackIndex);

	Sample n;
	n.track = trackIndex;
	n.sampleRate = inst->sampleRate * ev->milliHertz / FREQ_MIDDLE_C;
	n.patch = inst;
	n.pos = 0;
	if (ev->velocity < 0) {
		// Use default velocity
		n.vol = inst->defaultVolume;
	} else {
		n.vol = ev->velocity;
	}

	this->activeSamples.push_back(n);
	return;
}

void SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->noteOff(trackIndex);
	return;
}

void SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const EffectEvent *ev)
{
	// TODO: Lock mutex
	Sample *activeSample = NULL;
	for (SampleVector::iterator
		i = this->activeSamples.begin(); i != this->activeSamples.end(); i++
	) {
		Sample& sample = *i;
		if (sample.track == trackIndex) {
			activeSample = &sample;
			break;
		}
	}
	// TODO: Release mutex
	if (!activeSample) return; // no note to affect

	switch (ev->type) {
		case EffectEvent::PitchbendNote:
			activeSample->sampleRate =
				activeSample->patch->sampleRate * ev->data / FREQ_MIDDLE_C;
			break;
		case EffectEvent::Volume:
			activeSample->vol = ev->data;
			break;
	}
	return;
}

void SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const ConfigurationEvent *ev)
{
	return;
}

void SynthPCM::noteOff(unsigned int trackIndex)
{
	for (SampleVector::iterator
		i = this->activeSamples.begin(); i != this->activeSamples.end();
	) {
		Sample& sample = *i;
		if (sample.track == trackIndex) {
			i = this->activeSamples.erase(i);
		} else {
			i++;
		}
	}
	return;
}
