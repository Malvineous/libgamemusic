/**
 * @file   test-opl.hpp
 * @brief  Test code for generic OPL functions.
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

#include <boost/test/unit_test.hpp>

#include <camoto/util.hpp> // createString()
#include <camoto/gamemusic.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include "tests.hpp"

using namespace camoto;
namespace gm = camoto::gamemusic;

BOOST_AUTO_TEST_CASE(noteToFreq)
{
	BOOST_TEST_MESSAGE("Testing note to frequency conversion");

	BOOST_CHECK_EQUAL(gm::fnumToMilliHertz( 545, 1, 49716),   51680);

	BOOST_CHECK_EQUAL(gm::fnumToMilliHertz( 128, 2, 49716),   24275);
	BOOST_CHECK_EQUAL(gm::fnumToMilliHertz( 128, 5, 49716),  194203);

	BOOST_CHECK_EQUAL(gm::fnumToMilliHertz(1023, 1, 49716),   97006);
	BOOST_CHECK_EQUAL(gm::fnumToMilliHertz(1023, 2, 49716),  194013);
	BOOST_CHECK_EQUAL(gm::fnumToMilliHertz(1023, 7, 49716), 6208431);
}

BOOST_AUTO_TEST_CASE(freqToNote)
{
	BOOST_TEST_MESSAGE("Testing frequency to note conversion");

	unsigned int fnum, block;
#define check_freq(f, n, b) \
	gm::milliHertzToFnum(f, &fnum, &block, 49716); \
	BOOST_CHECK_EQUAL(fnum, n); \
	BOOST_CHECK_EQUAL(block, b);

	check_freq(  51680,  545, 1);

	check_freq(  24275,  512, 0);
	check_freq( 194203,  512, 3);

	check_freq(  97006, 1023, 1);
	check_freq( 194013, 1023, 2);
	check_freq(6208431, 1023, 7);
}

BOOST_AUTO_TEST_CASE(oplCalc)
{
	BOOST_CHECK_EQUAL(OPLOFFSET_MOD(1-1), 0x00);
	BOOST_CHECK_EQUAL(OPLOFFSET_MOD(5-1), 0x09);
	BOOST_CHECK_EQUAL(OPLOFFSET_MOD(9-1), 0x12);
	BOOST_CHECK_EQUAL(OPLOFFSET_CAR(1-1), 0x03);
	BOOST_CHECK_EQUAL(OPLOFFSET_CAR(5-1), 0x0C);
	BOOST_CHECK_EQUAL(OPLOFFSET_CAR(9-1), 0x15);
	BOOST_CHECK_EQUAL(OPL_OFF2CHANNEL(0x00), 1-1);
	BOOST_CHECK_EQUAL(OPL_OFF2CHANNEL(0x09), 5-1);
	BOOST_CHECK_EQUAL(OPL_OFF2CHANNEL(0x12), 9-1);
	BOOST_CHECK_EQUAL(OPL_OFF2CHANNEL(0x03), 1-1);
	BOOST_CHECK_EQUAL(OPL_OFF2CHANNEL(0x0C), 5-1);
	BOOST_CHECK_EQUAL(OPL_OFF2CHANNEL(0x15), 9-1);
}
