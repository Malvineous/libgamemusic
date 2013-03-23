/**
 * @file   opl-util.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_OPL_UTIL_HPP_
#define _CAMOTO_GAMEMUSIC_OPL_UTIL_HPP_

// OPL register offsets
#define BASE_CHAR_MULT  0x20
#define BASE_SCAL_LEVL  0x40
#define BASE_ATCK_DCAY  0x60
#define BASE_SUST_RLSE  0x80
#define BASE_FNUM_L     0xA0
#define BASE_KEYON_FREQ 0xB0
#define BASE_RHYTHM     0xBD
#define BASE_WAVE       0xE0
#define BASE_FEED_CONN  0xC0

/// Bit in keyon/freq register for turning a note on.
#define OPLBIT_KEYON    0x20

/// Supplied with a channel, return the offset from a base OPL register for the
/// Modulator cell (e.g. channel 4's modulator is at offset 0x09.  Since 0x60 is
/// the attack/decay function, register 0x69 will thus set the attack/decay for
/// channel 4's modulator.)  Channels go from 0 to 8 inclusive.
#define OPLOFFSET_MOD(channel)   (((channel) / 3) * 8 + ((channel) % 3))

/// Supplied with a channel, return the offset from a base OPL register for the
/// Carrier cell (e.g. channel 4's carrier is at offset 0x0C.  Since 0x60 is
/// the attack/decay function, register 0x6C will thus set the attack/decay for
/// channel 4's carrier.)  Channels go from 0 to 8 inclusive.
#define OPLOFFSET_CAR(channel)   (OPLOFFSET_MOD(channel) + 3)

/// Supplied with an operator offset, return the OPL channel it is associated
/// with (0-8).  Note that this only works in 2-operator mode, the OPL3's 4-op
/// mode requires a different formula.
#define OPL_OFF2CHANNEL(off)   (((off) % 8 % 3) + ((off) / 8 * 3))

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

namespace camoto {
namespace gamemusic {

/// Convert the given f-number and block into a note frequency.
/**
 * @param fnum
 *   Input frequency number, between 0 and 1023 inclusive.  Values outside this
 *   range will cause assertion failures.
 *
 * @param block
 *   Input block number, between 0 and 7 inclusive.  Values outside this range
 *   will cause assertion failures.
 *
 * @param conversionFactor
 *   Conversion factor to use.  Normally will be 49716 and occasionally 50000.
 *
 * @return The converted frequency in milliHertz.
 */
int DLL_EXPORT fnumToMilliHertz(unsigned int fnum, unsigned int block,
	unsigned int conversionFactor);

/// Convert a frequency into an OPL f-number
/**
 * @param milliHertz
 *   Input frequency.
 *
 * @param fnum
 *   Output frequency number for OPL chip.  This is a 10-bit number, so it will
 *   always be between 0 and 1023 inclusive.
 *
 * @param block
 *   Output block number for OPL chip.  This is a 3-bit number, so it will
 *   always be between 0 and 7 inclusive.
 *
 * @param conversionFactor
 *   Conversion factor to use.  Normally will be 49716 and occasionally 50000.
 *
 * @post fnum will be set to a value between 0 and 1023 inclusive.  block will
 *   be set to a value between 0 and 7 inclusive.  assert() calls inside this
 *   function ensure this will always be the case.
 *
 * @note As the block value increases, the frequency difference between two
 *   adjacent fnum values increases.  This means the higher the frequency,
 *   the less precision is available to represent it.  Therefore, converting
 *   a value to fnum/block and back to milliHertz is not guaranteed to reproduce
 *   the original value.
 */
void DLL_EXPORT milliHertzToFnum(unsigned int milliHertz,
	unsigned int *fnum, unsigned int *block, unsigned int conversionFactor);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_OPL_UTIL_HPP_
