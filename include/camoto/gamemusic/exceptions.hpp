/**
 * @file  exceptions.hpp
 * @brief Common errors.
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

#ifndef _CAMOTO_GAMEMUSIC_EXCEPTIONS_HPP_
#define _CAMOTO_GAMEMUSIC_EXCEPTIONS_HPP_

#include <camoto/error.hpp>

namespace camoto {
namespace gamemusic {

/// Exception thrown when adding a patch to a bank that doesn't support that
/// patch type (e.g. adding a sampled patch to an OPL bank)
class bad_patch: public error
{
	public:
		/// Constructor.
		/**
		 * @param msg
		 *   Error description for UI messages.
		 */
		bad_patch(const std::string& msg);
};

/// Exception thrown when a file format cannot store the required data.
class format_limitation: public error
{
	public:
		/// Constructor.
		/**
		 * @param msg
		 *   Error description for UI messages.
		 */
		format_limitation(const std::string& msg);
};


} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_EXCEPTIONS_HPP_
