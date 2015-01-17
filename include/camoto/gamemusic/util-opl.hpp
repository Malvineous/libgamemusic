/**
 * @file   opl-util.hpp
 * @brief  Utility functions related to OPL chips.
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

#ifndef _CAMOTO_GAMEMUSIC_UTIL_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_UTIL_OPL_HPP_

#include <math.h>

namespace camoto {
namespace gamemusic {

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
#define OPL2_OFF2CHANNEL(off)   (((off) % 8 % 3) + ((off) / 8 * 3))

/// Convert a logarithmic volume into a libgamemusic linear velocity
/**
 * @param vol
 *   Logarithmic volume value, with 0 being silent, and max being loudest.
 *
 * @param max
 *   Maximum value of vol.
 *
 * @return Linear value between 0 and 255 inclusive.
 */
inline unsigned int log_volume_to_lin_velocity(unsigned int vol,
	unsigned int max)
{
	return round(
		255 * (1 - log((float)(max + 1) - vol) / log((double)max + 1))
	);
}

/// Convert a libgamemusic linear velocity into a logarithmic volume value
/**
 * @param vel
 *   Linear velocity, with 0 being silent, and 255 being loudest.
 *
 * @param max
 *   Maximum value of the return value, when vel is 255.
 *
 * @return Logarithmic value between 0 and max inclusive.
 */
inline unsigned int lin_velocity_to_log_volume(unsigned int vel,
	unsigned int max)
{
	return round((max + 1) - pow(max + 1, 1 - vel / 255.0));
}

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

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

typedef enum {
	/// Matching operators: mod -> mod, car -> car, for all rhythm instruments.
	OPLPerc_MatchingOps,
	/// Carrier-only perc have settings loaded from the modulator fields.
	OPLPerc_CarFromMod,
	/// Modulator-only perc have settings loaded from the carrier fields.
	OPLPerc_ModFromCar,
} OPL_NORMALISE_PERC;

} // namespace gamemusic
} // namespace camoto

#include <camoto/gamemusic/music.hpp>

namespace camoto {
namespace gamemusic {

/// Ensure all the percussive instruments are set correctly.
/**
 * This runs through all the events on all channels and any instrument played
 * on a percussive channel is set as a percussive instrument.  If the same
 * instrument is also played on a normal channel then it is duplicated.
 *
 * It also swaps the modulator and carrier fields for those file formats
 * where the percussive operators are cross-loaded into different operators
 * than they are stored in in the music file.
 *
 * @param music
 *   Song to examine and alter.
 *
 * @param method
 *   Flag indicating whether percussive instruments need their operators
 *   swapped.
 */
void oplDenormalisePerc(MusicPtr music, OPL_NORMALISE_PERC method);

/// Remove all possible duplicate percussive instruments.
/**
 * This runs through all the events on all channels and any instrument is
 * compared against others and removed if it is a duplicate.
 *
 * It also swaps the modulator and carrier fields for those file formats
 * where the percussive operators are cross-loaded into different operators
 * than they are stored in in the music file.
 *
 * @param music
 *   Song to examine.
 *
 * @param method
 *   Flag indicating whether percussive instruments need their operators
 *   swapped.
 *
 * @return The new instrument bank, possible with swapped operators.  Some
 *   patches will point to the same place as in the original bank - only
 *   the modified patches will be different pointers.
 */
PatchBankPtr oplNormalisePerc(MusicPtr music, OPL_NORMALISE_PERC method);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_UTIL_OPL_HPP_
