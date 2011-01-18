/**
 * @file   patchbank-opl.hpp
 * @brief  Declaration of a PatchBank that only holds OPL patches.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_PATCHBANK_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_PATCHBANK_OPL_HPP_

#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patchbank-singletype.hpp>

namespace camoto {
namespace gamemusic {

/// PatchBank-derived class which only holds OPL patches.
typedef SingleTypePatchBank<OPLPatch> OPLPatchBank;

/// Shared pointer to a OPLPatchBank.
typedef boost::shared_ptr< OPLPatchBank > OPLPatchBankPtr;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCHBANK_OPL_HPP_
