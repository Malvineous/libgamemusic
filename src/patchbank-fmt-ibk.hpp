/**
 * @file   patchbank-fmt-ibk.hpp
 * @brief  PatchBank interface to .IBK (Instrument Bank) file format.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCHBANK_FMT_IBK_HPP_
#define _CAMOTO_GAMEMUSIC_PATCHBANK_FMT_IBK_HPP_

#include <iostream>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patchbank-singletype.hpp>

namespace camoto {
namespace gamemusic {

/// PatchBank interface to an .IBK (Instrument Bank) file.
class PatchBank_IBK: virtual public SingleTypePatchBank<OPLPatch> {

	public:

		PatchBank_IBK(std::iostream data)
			throw ();

		virtual ~PatchBank_IBK()
			throw ();

		virtual std::string getName()
			throw ();

		virtual void setPatchCount(unsigned int newCount)
			throw ();

		/// Call parent class to modify patch, then write out to stream
		virtual void setPatch(unsigned int index, PatchPtr newPatch)
			throw (bad_patch);

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCHBANK_FMT_IBK_HPP_
