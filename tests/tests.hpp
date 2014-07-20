/**
 * @file   tests.hpp
 * @brief  Test code core.
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
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
#include <camoto/util.hpp>

/// Allow a string constant to be passed around with embedded nulls
#define STRING_WITH_NULLS(x)  std::string((x), sizeof((x)) - 1)

/// Base class for all tests
class test_main
{
	public:
		test_main();

		void printNice(boost::test_tools::predicate_result& res,
			const std::string& s, const std::string& diff);

		void print_wrong(boost::test_tools::predicate_result& res,
			const std::string& strExpected, const std::string& strResult);

		boost::test_tools::predicate_result is_equal(const std::string& strExpected,
			const std::string& strCheck);

	public:
		boost::unit_test::test_suite *ts; ///< Suite to add tests to
		std::string basename; ///< Name of final class
		unsigned int outputWidth; ///< Width of output hexdump, as number of bytes shown per line
};

/// Template class to add supported tests for each format to the test tree
template<class T>
class suite_test_tmpl {
	public:
		boost::shared_ptr<T> test_results;

		suite_test_tmpl(const std::string& basename)
		{
			boost::unit_test::test_suite *ts
				= BOOST_TEST_SUITE("test_" + basename);
			this->test_results.reset(new T);
			test_results->ts = ts;
			test_results->basename = basename;

			test_results->addTests();
			boost::unit_test::framework::master_test_suite().add(ts);
		}
};

/// Add the tests for a given format
#define IMPLEMENT_TESTS(fmt) \
	class suite_ ## fmt: public suite_test_tmpl<test_ ## fmt> { \
		public: \
			suite_ ## fmt() \
				:	suite_test_tmpl<test_ ## fmt>(TOSTRING(fmt)) \
			{} \
	} suite_ ## fmt ## _inst;

#endif // _CAMOTO_GAMEMUSIC_TESTS_HPP_
