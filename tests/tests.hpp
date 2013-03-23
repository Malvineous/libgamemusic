/**
 * @file   tests.hpp
 * @brief  Test code core.
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

#ifndef _CAMOTO_GAMEMUSIC_TESTS_HPP_
#define _CAMOTO_GAMEMUSIC_TESTS_HPP_

#include <boost/test/unit_test.hpp>

/// Allow a string constant to be passed around with embedded nulls
#define makeString(x)  std::string((x), sizeof((x)) - 1)

struct default_sample {

	void printNice(boost::test_tools::predicate_result& res, const std::string& s,
		const std::string& diff);

	void print_wrong(boost::test_tools::predicate_result& res,
		const std::string& strExpected, const std::string& strResult);

	boost::test_tools::predicate_result is_equal(const std::string& strExpected,
		const std::string& strCheck);

};

#endif // _CAMOTO_GAMEMUSIC_TESTS_HPP_
