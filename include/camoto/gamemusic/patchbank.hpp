/**
 * @file  camoto/gamemusic/patchbank.hpp
 * @brief Declaration of top-level PatchBank class, for managing
 *        collections of patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_
#define _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_

#include <memory>
#include <string>
#include <vector>
#include <camoto/gamemusic/patch.hpp>
#include <camoto/gamemusic/exceptions.hpp>

namespace camoto {
namespace gamemusic {

/// A PatchBank is a collection of patches, of any type (OPL, MIDI, etc.)
typedef std::vector<std::shared_ptr<Patch>> PatchBank;

/// Convert Patch types into a string.
template <class T>
struct PatchTypeName
{
	static const char *name;
};

/// Require only certain patches in a std::shared_ptr<PatchBank>.
template <class T>
void requirePatches(const PatchBank& p)
{
	for (auto& i : p) {
		if (!dynamic_cast<T *>(i.get())) {
			throw format_limitation("This file format can only store "
				+ std::string(PatchTypeName<T>::name) + " instruments.");
		}
	}
	return;
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_
