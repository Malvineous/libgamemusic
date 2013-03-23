/**
 * @file   encode-opl.hpp
 * @brief  Function to convert a Music instance into raw OPL data.
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

#ifndef _CAMOTO_GAMEMUSIC_ENCODE_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_ENCODE_OPL_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/stream.hpp>

namespace camoto {
namespace gamemusic {

void oplEncode(OPLWriterCallback *cb, DelayType delayType,
	double fnumConversion, unsigned int flags, MusicPtr music);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_ENCODE_OPL_HPP_
