/**
 * @file   audio-util.hpp
 * @brief  Utility functions for processing audio.
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

#ifndef _CAMOTO_GAMEMUSIC_AUDIO_UTIL_HPP_
#define _CAMOTO_GAMEMUSIC_AUDIO_UTIL_HPP_

// Clipping function to prevent integer wraparound after amplification
#define SAMPLE_SIZE 2
#define SAMP_BITS (SAMPLE_SIZE << 3)
#define SAMP_MAX ((1 << (SAMP_BITS-1)) - 1)
#define SAMP_MIN -((1 << (SAMP_BITS-1)))
#define CLIP(v) (((v) > SAMP_MAX) ? SAMP_MAX : (((v) < SAMP_MIN) ? SAMP_MIN : (v)))

/// Convert unsigned 8-bit sample (0..255) to signed 16-bit (-32768..32767)
#define U8_TO_S16(s) (s | (s << 8)) - 32768L;

inline long mix_pcm(long a, long b)
{
	a += 32768;
	b += 32768;
	unsigned long m;
	if ((a < 32768) && (b < 32768)) {
		m = (long long)a * b / 32768LL;
	} else {
		m = 2 * ((long long)a + b) - ((long long)a * b) / 32768LL - 65536;
	}
	if (m == 65536) m = 65535;
	assert(m < 65536);
	return -32768 + m;
}

#endif // _CAMOTO_GAMEMUSIC_AUDIO_UTIL_HPP_
