/**
 * @file   opl-util.cpp
 * @brief  Utility functions related to OPL chips.
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

#include <iostream>
#include <cassert>
#include <camoto/gamemusic/opl-util.hpp>

#define log2(x) (log(x) / 0.30102999566398119521373889472449)

namespace camoto {
namespace gamemusic {

int fnumToMilliHertz(unsigned int fnum, unsigned int block,
	unsigned int conversionFactor)
{
	assert(block < 8);
	assert(fnum < 1024);

	// Original formula
	//return 1000 * conversionFactor * (double)fnum * pow(2, (double)((signed)block - 20));

	// More efficient version
	return (1000ull * conversionFactor * fnum) >> (20 - block);
}

void milliHertzToFnum(unsigned int milliHertz,
	unsigned int *fnum, unsigned int *block, unsigned int conversionFactor)
{
	// Special case to avoid divide by zero
	if (milliHertz == 0) {
		*block = 0; // actually any block will work
		*fnum = 0;
		return;
	}

	// Special case for frequencies too high to produce
	if (milliHertz > 6208431) {
		*block = 7;
		*fnum = 1023;
		return;
	}

	/// This formula will provide a pretty good estimate as to the best block to
	/// use for a given frequency.  It tries to use the lowest possible block
	/// number that is capable of representing the given frequency.  This is
	/// because as the block number increases, the precision decreases (i.e. there
	/// are larger steps between adjacent note frequencies.)  The 6M constant is
	/// the largest frequency (in milliHertz) that can be represented by the
	/// block/fnum system.
	//int invertedBlock = log2(6208431 / milliHertz);

	// Very low frequencies will produce very high inverted block numbers, but
	// as they can all be covered by inverted block 7 (block 0) we can just clip
	// the value.
	//if (invertedBlock > 7) invertedBlock = 7;
	//*block = 7 - invertedBlock;

	// This is a bit more efficient and doesn't need log2() from math.h
	if (milliHertz > 3104215) *block = 7;
	else if (milliHertz > 1552107) *block = 6;
	else if (milliHertz > 776053) *block = 5;
	else if (milliHertz > 388026) *block = 4;
	else if (milliHertz > 194013) *block = 3;
	else if (milliHertz > 97006) *block = 2;
	else if (milliHertz > 48503) *block = 1;
	else *block = 0;

	// Original formula
	//*fnum = milliHertz * pow(2, 20 - *block) / 1000 / conversionFactor + 0.5;

	// Slightly more efficient version
	*fnum = ((unsigned long long)milliHertz << (20 - *block)) / (conversionFactor * 1000.0) + 0.5;

	if ((*block == 7) && (*fnum > 1023)) {
		std::cerr << "Warning: Frequency out of range, clipped to max" << std::endl;
		*fnum = 1023;
	}

	// Make sure the values come out within range
	//assert(*block >= 0);
	assert(*block <= 7);
	//assert(*fnum >= 0);
	//assert(*fnum < 1024);
	if (*fnum > 1024) *fnum = 1024; // TEMP
	return;
}

} // namespace gamemusic
} // namespace camoto
