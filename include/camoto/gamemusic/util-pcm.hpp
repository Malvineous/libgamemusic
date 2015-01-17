/**
 * @file   audio-util.hpp
 * @brief  Utility functions for processing audio.
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

#ifndef _CAMOTO_GAMEMUSIC_UTIL_PCM_HPP_
#define _CAMOTO_GAMEMUSIC_UTIL_PCM_HPP_

#include <cassert>

namespace camoto {
namespace gamemusic {

// Clipping function to prevent integer wraparound after amplification
inline int pcm_clip_s16(int s)
{
	static const int SAMPLE_SIZE = 2;
	static const int SAMPLE_BITS = SAMPLE_SIZE << 3;
	static const int SAMPLE_MAX = (1 << (SAMPLE_BITS - 1)) - 1;
	static const int SAMPLE_MIN = -(1 << (SAMPLE_BITS - 1));
	return ((s) > SAMPLE_MAX) ? SAMPLE_MAX : (((s) < SAMPLE_MIN) ? SAMPLE_MIN : (s));
}

/// Convert unsigned 8-bit sample (0..255) to signed 16-bit (-32768..32767)
inline int pcm_u8_to_s16(int s)
{
	return (s | (s << 8)) - 32768L;
}

/// Mix two PCM samples and return the new combined sample
inline long pcm_mix_s16(long a, long b)
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

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_UTIL_PCM_HPP_
