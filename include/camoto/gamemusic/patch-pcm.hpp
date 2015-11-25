/**
 * @file  camoto/gamemusic/patch-pcm.hpp
 * @brief Derived Patch for PCM patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCH_PCM_HPP_
#define _CAMOTO_GAMEMUSIC_PATCH_PCM_HPP_

#include <boost/shared_array.hpp>
#include <camoto/gamemusic/patch.hpp>
#include <stdint.h>

namespace camoto {
namespace gamemusic {

/// Descendent of Patch for storing PCM instruments.
struct CAMOTO_GAMEMUSIC_API PCMPatch: public Patch
{
	/// Default constructor sets everything to zero/defaults.
	PCMPatch();

	unsigned long sampleRate; ///< Sampling rate in Hertz to get a middle-C note
	unsigned int bitDepth;    ///< Sample size in bits (8/16)
	unsigned int numChannels; ///< Channel count (1=mono, 2=stereo)

	unsigned long loopStart;  ///< Beginning of loop (offset of first sample)
	unsigned long loopEnd;    ///< End of loop, 0=no loop (offset of last sample+1)

	/// Actual sample data.
	/**
	 * If bitDepth is 8, this is unsigned 8-bit PCM data, one byte per sample.  If
	 * bitDepth is 16, then this is signed 16-bit PCM data (two bytes per sample),
	 * in host-byte order.  Since most PCM data is little-endian, 16-bit PCM data
	 * will need to be converted to host-byte order when it is loaded into this
	 * buffer.
	 */
	std::vector<uint8_t> data;
};

/// Shared pointer to a Patch.
typedef boost::shared_ptr<PCMPatch> PCMPatchPtr;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCH_PCM_HPP_
