/**
 * @file   decode-opl.hpp
 * @brief  Function to convert raw OPL data into a Music instance.
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

#ifndef _CAMOTO_GAMEMUSIC_DECODE_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_DECODE_OPL_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/stream.hpp>

namespace camoto {
namespace gamemusic {

/// Callback class used to supply OPL data to oplDecode().
class OPLReaderCallback {
	public:
		/// Read the next reg/val pair from the source data.
		/**
		 * @param oplEvent
		 *   When returning true, this structure is populated with the next reg/val
		 *   pair and associated data.  To supply delay events or tempo changes that
		 *   have no associated data bytes, set both the register and value to 0.
		 *
		 * @return true if oplEvent or tempo is valid or false to signify the end of
		 *   the song.
		 */
		virtual bool readNextPair(OPLEvent *oplEvent) = 0;
};

/// Convert caller-supplied OPL data into a Music instance.
/**
 * @param delayType
 *   Where the delay is actioned - before its associated data pair is sent
 *   to the OPL chip, or after.
 *
 * @param fnumConversion
 *   Conversion constant to use when converting OPL frequency numbers into
 *   Hertz.  Can be one of OPL_FNUM_* or a raw value.
 *
 * @param cb
 *   Callback class used to read the actual OPL data bytes from the file.
 */
MusicPtr oplDecode(OPLReaderCallback *cb, DelayType delayType,
	double fnumConversion);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_DECODE_OPL_HPP_
