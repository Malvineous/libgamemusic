/**
 * @file   patchbank-singletype.hpp
 * @brief  Declaration of a PatchBank that only supports a single Patch type.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCHBANK_SINGLETYPE_HPP_
#define _CAMOTO_GAMEMUSIC_PATCHBANK_SINGLETYPE_HPP_

#include <boost/pointer_cast.hpp>
#include <camoto/gamemusic/patchbank.hpp>

namespace camoto {
namespace gamemusic {

/// PatchBank-derived class which only supports a single patch type (e.g. OPL
/// only or sampled patches only)
template <class TPatch>
class SingleTypePatchBank: virtual public PatchBank {

	public:
		/// Shared pointer to a TPatch.
		typedef boost::shared_ptr<TPatch> TPatchPtr;

		SingleTypePatchBank()
			throw ()
		{
		}

		SingleTypePatchBank(const PatchBank& base)
			throw (EBadPatchType)
		{
			this->setPatchCount(base.getPatchCount());
			for (int i = 0; i < base.getPatchCount(); ++i) {
				this->setPatch(i, base.getPatch(i));
				/*TPatchPtr oplPatch = boost::dynamic_pointer_cast<TPatch>(base.getPatch(i));
				if (!oplPatch) throw EBadPatchType();
				this->setPatch(i, oplPatch);*/
			}
		}

		virtual ~SingleTypePatchBank()
			throw ()
		{
		}

		/// Get the patch at the given index in its derived type.
		/**
		 * @preconditions index can be passed as-is to getPatch()
		 *
		 * @return A shared pointer to the requested patch, in its derived type.
		 */
		virtual const TPatchPtr getTypedPatch(unsigned int index)
			throw ()
		{
			return boost::static_pointer_cast<TPatch>(this->getPatch(index));
		}

		/// Modify the patch at the given index.
		/**
		 * @note to patch bank implementors: There is a default implementation
		 * of this function which overwrites the patch in the vector.  You may
		 * wish to override this function to check if the patch is of the correct
		 * type before calling the default function.
		 *
		 * @preconditions Same as PatchBank::setPatch()
		 *
		 * @throws EBadPatchType if the supplied patch is not an instance of TPatch.
		 */
		virtual void setPatch(unsigned int index, TPatchPtr newPatch)
			throw (EBadPatchType)
		{
			this->PatchBank::setPatch(index, boost::static_pointer_cast<Patch>(newPatch));
		}

		/// Modify the patch at the given index.
		/**
		 * @note to patch bank implementors: There is a default implementation
		 * of this function which overwrites the patch in the vector.  You may
		 * wish to override this function to check if the patch is of the correct
		 * type before calling the default function.
		 *
		 * @preconditions Same as PatchBank::setPatch()
		 *
		 * @throws EBadPatchType if the supplied patch is not an instance of TPatch.
		 */
		virtual void setPatch(unsigned int index, PatchPtr newPatch)
			throw (EBadPatchType)
		{
			// We could use boost::dynamic_pointer_cast here, but all we want to
			// do is check the type so we just use dynamic_cast to avoid creating
			// a new shared_ptr instance that won't be used.
			//TPatchPtr tpp = boost::dynamic_pointer_cast<TPatch>(newPatch);
			TPatch *tpp = dynamic_cast<TPatch *>(newPatch.get());
			if (!tpp) throw EBadPatchType("This patch bank cannot store this type of instrument");
			this->PatchBank::setPatch(index, newPatch);
			return;
		}

};

/// Shared pointer to a SingleTypePatchBank.
//template <class TPatch>
//typedef boost::shared_ptr< SingleTypePatchBank<TPatch> > SingleTypePatchBankPtr<TPatch>;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCHBANK_SINGLETYPE_HPP_
