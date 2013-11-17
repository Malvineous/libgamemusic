/**
 * @file   patch-pcm.hpp
 * @brief  Derived Patch for PCM patches.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
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
struct DLL_EXPORT PCMPatch: public Patch
{
	/// Default constructor sets everything to zero/defaults.
	PCMPatch();

	uint32_t sampleRate; ///< Sampling rate in Hertz to get a middle-C note
	uint8_t bitDepth;    ///< Sample size in bits (8/16)
	uint8_t numChannels; ///< Channel count (1=mono, 2=stereo)

	uint32_t loopStart;  ///< Beginning of loop (offset of first sample)
	uint32_t loopEnd;    ///< End of loop, 0=no loop (offset of last sample+1)

	uint32_t lenData;    ///< Size of data in bytes

	/// Actual sample data
	boost::shared_array<uint8_t> data;
};

/// Shared pointer to a Patch.
typedef boost::shared_ptr<PCMPatch> PCMPatchPtr;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCH_PCM_HPP_
