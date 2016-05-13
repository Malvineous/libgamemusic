/**
 * @file  synth-pcm.cpp
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

void SynthPCM::setBankMIDI(std::shared_ptr<const PatchBank> bankMIDI)
{
	this->bankMIDI = bankMIDI;
	return;
}

void SynthPCM::reset(const std::vector<TrackInfo>& trackInfo,
	std::shared_ptr<const PatchBank> patches)
{
	this->trackInfo = trackInfo;
	this->patches = patches;
	return;
}

void SynthPCM::mix(int16_t *output, unsigned long len)
{
	len /= 2; // stereo
	// TODO: Lock mutex
	for (auto
		i = this->activeSamples.begin(); i != this->activeSamples.end(); /* i++ */
	) {
		auto& sample = *i;
		auto *output_cur = output;
		bool complete = false;
		auto& data = sample.patch->data;
		if (data.size() == 0) { i++; continue; }
		unsigned long lenInput = sample.patch->loopEnd ? sample.patch->loopEnd : data.size();
		unsigned long numOutputSamples = lenInput * ((double)this->outputSampleRate / (double)sample.sampleRate);
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
			uint8_t *in = &data[posInput];

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

bool SynthPCM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->cb->tempoChange(ev->tempo);
	return true;
}

bool SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const NoteOnEvent *ev)
{
	assert(this->patches);
	assert(trackIndex < this->trackInfo.size());
	assert(ev->velocity < 256);

	if (ev->instrument >= this->patches->size()) {
		throw bad_patch(createString("Instrument bank too small - tried to play "
			"note with instrument #" << ev->instrument + 1
			<< " but patch bank only has " << this->patches->size()
			<< " instruments."));
	}
	auto patch = this->patches->at(ev->instrument);

	auto& ti = this->trackInfo.at(trackIndex);
	if (this->bankMIDI) {
		// We are handling MIDI events
		if (
			(ti.channelType != TrackInfo::ChannelType::MIDI)
			&& (ti.channelType != TrackInfo::ChannelType::Any)
		) {
			// Not a MIDI track
			return true;
		}
		auto instMIDI = dynamic_cast<MIDIPatch*>(patch.get());
		if (!instMIDI) return true; // non-MIDI instrument on a MIDI channel, ignore
		unsigned long target = instMIDI->midiPatch;
		if (instMIDI->percussion) {
			target += MIDI_PATCHES;
		}
		if (target < this->bankMIDI->size()) {
			patch = this->bankMIDI->at(target);
		} else {
			// No patch, bank too small
			return true;
		}
	} else {
		// We are handling PCM events
		if (
			(ti.channelType != TrackInfo::ChannelType::PCM)
			&& (ti.channelType != TrackInfo::ChannelType::Any)
		) {
			// Not a PCM track
			return true;
		}
	}
	auto inst = std::dynamic_pointer_cast<PCMPatch>(patch);

	// Don't play this note if there's no patch for it
	if (!inst) return true;

	this->noteOff(trackIndex);

	Sample n;
	n.track = trackIndex;
	n.sampleRate = inst->sampleRate * ((double)ev->milliHertz / (double)FREQ_MIDDLE_C);
	n.patch = inst;
	n.pos = 0;
	if (ev->velocity < 0) {
		// Use default velocity
		n.vol = inst->defaultVolume;
	} else {
		n.vol = ev->velocity;
	}

	this->activeSamples.push_back(n);
	return true;
}

bool SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->noteOff(trackIndex);
	return true;
}

bool SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const EffectEvent *ev)
{
	// TODO: Lock mutex
	Sample *activeSample = NULL;
	for (auto& sample : this->activeSamples) {
		if (sample.track == trackIndex) {
			activeSample = &sample;
			break;
		}
	}
	// TODO: Release mutex
	if (!activeSample) return true; // no note to affect

	switch (ev->type) {
		case EffectEvent::Type::PitchbendNote:
			activeSample->sampleRate =
				activeSample->patch->sampleRate * ev->data / FREQ_MIDDLE_C;
			break;
		case EffectEvent::Type::Volume:
			activeSample->vol = ev->data;
			break;
	}
	return true;
}

bool SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const GotoEvent *ev)
{
	return true;
}

bool SynthPCM::handleEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, const ConfigurationEvent *ev)
{
	return true;
}

void SynthPCM::noteOff(unsigned int trackIndex)
{
	for (auto i = this->activeSamples.begin(); i != this->activeSamples.end(); /* i++ */
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
