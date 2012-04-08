/**
 * @file   patchbank.hpp
 * @brief  Declaration of top-level PatchBank class, for managing
 *         collections of patches.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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

/// Primary interface to a patchbank (list of patches available for use in a
/// song)
/**
 * This class represents a patchbank, which is a collection of patches.  It
 * is also the base class for editing instrument bank files.  It is not
 * specialised (so OPL and sampled patches can be added to the same bank)
 * however this is not guaranteed and may cause an exception if this is not
 * supported by the underlying song/bank file format.
 *
 * The default/base PatchBank implementation provides simple in-memory storage
 * of patches.  This class can also be extended to provide the same interface
 * to on-disk patch banks (like SoundFonts.)
 */
class PatchBank {

	protected:
		typedef std::vector<PatchPtr> VC_PATCH;
		VC_PATCH patches;

	public:

		PatchBank()
			throw ();

		virtual ~PatchBank()
			throw ();

		/// Return the patchbank's name (can be empty)
		virtual std::string getName() const
			throw ();

		/// Get the number of patches in this bank.
		virtual unsigned int getPatchCount() const
			throw ();

		/// Change the number of patches in this bank.
		/**
		 * If newCount is smaller than the current count the last few patches
		 * will be removed, and if newCount is larger some empty slots will be
		 * appended onto the end of the bank.  These empty slots must be filled
		 * by setPatch() before they are accessed or an exception will be thrown.
		 *
		 * @param newCount
		 *   Number of instruments in the bank.
		 */
		virtual void setPatchCount(unsigned int newCount)
			throw ();

		/// Get the patch at the given index.
		/**
		 * @note to patch bank implementors: There is a default implementation
		 * of this function which returns the patch straight out of the vector.
		 *
		 * @preconditions index < getPatchCount()
		 * @return A shared pointer to the requested patch.
		 */
		virtual const PatchPtr getPatch(unsigned int index) const
			throw ();

		/// Modify the patch at the given index.
		/**
		 * @note to patch bank implementors: There is a default implementation
		 * of this function which overwrites the patch in the vector.  You may
		 * wish to override this function to check if the patch is of the correct
		 * type before calling the default function.
		 *
		 * @preconditions index < getPatchCount()
		 * @throws bad_patch if the patch type is not supported by this bank
		 *         (e.g. adding a sampled patch to an OPL bank)
		 */
		virtual void setPatch(unsigned int index, PatchPtr newPatch)
			throw (bad_patch);

};

/// Shared pointer to a PatchBank.
typedef boost::shared_ptr<PatchBank> PatchBankPtr;

/// Vector of PatchBank shared pointers.
typedef std::vector<PatchBankPtr> VC_PATCHBANK;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCHBANK_HPP_
