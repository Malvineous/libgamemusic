/**
 * @file   patchbank.hpp
 * @brief  Declaration of top-level PatchBank class, for managing
 *         collections of patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_
#define _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <camoto/gamemusic/patch.hpp>
#include <camoto/gamemusic/exceptions.hpp>

namespace camoto {
namespace gamemusic {

/// A PatchBank is a collection of patches, of any type (OPL, MIDI, etc.)
typedef std::vector<PatchPtr> PatchBank;

/// Shared pointer to a PatchBank.
typedef boost::shared_ptr<PatchBank> PatchBankPtr;

/// Convert Patch types into a string.
template <class T>
struct PatchTypeName
{
	static const char *name;
};

/// Require only certain patches in a PatchBankPtr.
template <class T>
void requirePatches(const PatchBankPtr& p)
{
	for (PatchBank::const_iterator i = p->begin(); i != p->end(); i++) {
		if (!dynamic_cast<T *>(i->get())) {
			throw format_limitation("This file format can only store "
				+ std::string(PatchTypeName<T>::name) + " instruments.");
		}
	}
	return;
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_
