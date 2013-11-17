/**
 * @file   patch.hpp
 * @brief  Declaration of top-level Patch class, for managing patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCH_HPP_
#define _CAMOTO_GAMEMUSIC_PATCH_HPP_

#include <string>
#include <boost/shared_ptr.hpp>

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

namespace camoto {
namespace gamemusic {

/// Primary interface to a patch (sound settings for an instrument)
/**
 * This class represents a patch, which is the low level data required to play
 * the sound of an instrument.  It is specialised for the particular type of
 * patch (OPL, sampled, etc.)
 */
struct DLL_EXPORT Patch {

	std::string name;

	virtual ~Patch();
};

/// Shared pointer to a Patch.
typedef boost::shared_ptr<Patch> PatchPtr;

// There is no vector of Patch pointers as this is what a PatchBank is for.
/// Vector of Patch shared pointers.
//typedef std::vector<PatchPtr> VC_PATCH;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCH_HPP_
