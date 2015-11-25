/**
 * @file  camoto/gamemusic/patch.hpp
 * @brief Declaration of top-level Patch class, for managing patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCH_HPP_
#define _CAMOTO_GAMEMUSIC_PATCH_HPP_

#include <string>

#ifndef CAMOTO_GAMEMUSIC_API
#define CAMOTO_GAMEMUSIC_API
#endif

namespace camoto {
namespace gamemusic {

/// Primary interface to a patch (sound settings for an instrument)
/**
 * This class represents a patch, which is the low level data required to play
 * the sound of an instrument.  It is specialised for the particular type of
 * patch (OPL, sampled, etc.)
 */
struct CAMOTO_GAMEMUSIC_API Patch
{
	/// Title of the instrument.
	std::string name;

	/// Default volume, 0=silent, 255=full.
	/**
	 * This is overridden by the note-on velocity, but if that's -1, then the
	 * value here is used.
	 */
	unsigned int defaultVolume;

	Patch();
	virtual ~Patch();
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCH_HPP_
