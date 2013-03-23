/**
 * @file   manager.hpp
 * @brief  Manager class, used for accessing the various music format readers.
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

#ifndef _CAMOTO_GAMEMUSIC_MANAGER_HPP_
#define _CAMOTO_GAMEMUSIC_MANAGER_HPP_

#include <boost/shared_ptr.hpp>
#include <camoto/gamemusic/musictype.hpp>

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

namespace camoto {
namespace gamemusic {

/// Top-level class to manage music types.
/**
 * This class provides access to the different music file formats supported
 * by the library.
 *
 * In order to open a music file, this class must be used to access an instance
 * of the music type.  This MusicType instance is then used to create a
 * Music instance around a particular file.  It is this Music instance that
 * is then used to manipulate the file itself.
 *
 * @note Use the free function getManager() to obtain a pointer to an instance
 *   of an object implementing the Manager interface.
 */
class Manager
{
	public:
		/// Get a MusicType instance for a supported file format.
		/**
		 * This can be used to enumerate all available file formats.
		 *
		 * @param index
		 *   Index of the format, starting from 0.
		 *
		 * @return A shared pointer to a MusicType for the given index, or an empty
		 *   pointer once iIndex goes out of range.
		 */
		virtual const MusicTypePtr getMusicType(unsigned int index) const = 0;

		/// Get a MusicType instance by its code.
		/**
		 * @param  code %Music code (e.g. "imf-idsoftware")
		 * @return A shared pointer to a MusicType for the given code, or
		 *         an empty pointer on an invalid code.
		 */
		virtual const MusicTypePtr getMusicTypeByCode(const std::string& code)
			const = 0;
};

/// Shared pointer to a Manager.
typedef boost::shared_ptr<Manager> ManagerPtr;

/// Library entry point.
/**
 * All further functionality is provided by calling functions in the Manager
 * class.
 *
 * @return A shared pointer to a Manager instance.
 */
const ManagerPtr DLL_EXPORT getManager(void);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MANAGER_HPP_
