/**
 * @file   synth-opl.cpp
 * @brief  Interface to an OPL/FM/Adlib synthesizer.
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
#include <camoto/gamemusic/synth-opl.hpp>
#include <camoto/gamemusic/util-pcm.hpp>
#include "dbopl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define OPL_FRAME_SIZE 512

/// Boost the volume by this amount
#define VOL_BOOST 0

/// Mixer for processing DOSBox OPL data.
class OPLMixer: public MixerChannel {
	public:
		int16_t *buf;

		OPLMixer(int16_t *buf)
			:	buf(buf)
		{
		}

		virtual void AddSamples_m32(Bitu samples, Bit32s *buffer)
		{
			while (samples) {
				*this->buf = pcm_mix_s16(*this->buf, pcm_clip_s16(*buffer << VOL_BOOST));
				this->buf++;
				*this->buf = pcm_mix_s16(*this->buf, pcm_clip_s16(*buffer << VOL_BOOST));
				this->buf++;
				buffer++;
				samples--;
			}
			return;
		}

		virtual void AddSamples_s32(Bitu frames, Bit32s *buffer)
		{
			unsigned long samples = frames * 2;
			while (samples) {
				*this->buf = pcm_mix_s16(*this->buf, pcm_clip_s16(*buffer << VOL_BOOST));
				this->buf++;
				buffer++;
				samples--;
			}
			return;
		}
};

struct SynthOPLInternal {
	DBOPL::Handler opl;
};

SynthOPL::SynthOPL(unsigned long sampleRate)
	:	outputSampleRate(sampleRate)
{
	this->internal = new SynthOPLInternal;
}

SynthOPL::~SynthOPL()
{
	SynthOPLInternal *priv = (SynthOPLInternal *)this->internal;
	delete priv;
}

void SynthOPL::reset()
{
	SynthOPLInternal *priv = (SynthOPLInternal *)this->internal;
	priv->opl.Init(this->outputSampleRate);
	return;
}

void SynthOPL::write(unsigned int chip, unsigned int reg, unsigned int val)
{
	SynthOPLInternal *priv = (SynthOPLInternal *)this->internal;
	priv->opl.WriteReg((chip << 8) | reg, val);
	return;
}

void SynthOPL::mix(int16_t *output, unsigned long len)
{
	SynthOPLInternal *priv = (SynthOPLInternal *)this->internal;

	len /= 2; // stereo
	OPLMixer mix(output);
	while (len > 0) {
		unsigned long sampleCount = std::min((unsigned long)OPL_FRAME_SIZE, len);
		priv->opl.Generate(&mix, sampleCount);
		len -= sampleCount;
	}
	return;
}
