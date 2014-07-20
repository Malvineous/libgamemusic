/**
 * @file   test-tempo.cpp
 * @brief  Test code for tempo manipulation functions.
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

#include <boost/test/unit_test.hpp>

#include <camoto/stream_string.hpp>
#include <camoto/util.hpp> // createString()
#include <camoto/gamemusic.hpp>
#include "tests.hpp"

using namespace camoto;
namespace gm = camoto::gamemusic;

BOOST_AUTO_TEST_SUITE(tempo)

BOOST_AUTO_TEST_CASE(hertz)
{
	BOOST_TEST_MESSAGE("Testing tempo in Hertz");

	gm::Tempo t;
	t.ticksPerBeat = 350;

	t.hertz(700);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 1429);
	BOOST_CHECK_EQUAL(t.bpm(), 120);
	BOOST_CHECK_EQUAL(t.module_tempo(), 4200);
	BOOST_CHECK_EQUAL(t.hertz(), 700);
	BOOST_CHECK_EQUAL(t.msPerTick(), 1);

	t.hertz(560);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 1786);
	BOOST_CHECK_EQUAL(t.bpm(), 96);
	BOOST_CHECK_EQUAL(t.module_tempo(), 3360);
	BOOST_CHECK_EQUAL(t.hertz(), 560);
	BOOST_CHECK_EQUAL(t.msPerTick(), 2);
}

BOOST_AUTO_TEST_CASE(ms)
{
	BOOST_TEST_MESSAGE("Testing tempo in milliseconds-per-tick");

	gm::Tempo t;

	t.msPerTick(375);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 375000);
	BOOST_CHECK_EQUAL(t.bpm(), 80);
	BOOST_CHECK_EQUAL(t.module_tempo(), 16);
	BOOST_CHECK_EQUAL(t.hertz(), 3);
	BOOST_CHECK_EQUAL(t.msPerTick(), 375);

	t.msPerTick(500);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 500000);
	BOOST_CHECK_EQUAL(t.bpm(), 60);
	BOOST_CHECK_EQUAL(t.module_tempo(), 12);
	BOOST_CHECK_EQUAL(t.hertz(), 2);
	BOOST_CHECK_EQUAL(t.msPerTick(), 500);
}

BOOST_AUTO_TEST_CASE(bpm)
{
	BOOST_TEST_MESSAGE("Testing tempo in beats-per-minute");

	gm::Tempo t;

	t.bpm(60);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 500000);
	BOOST_CHECK_EQUAL(t.bpm(), 60);
	BOOST_CHECK_EQUAL(t.module_tempo(), 12);
	BOOST_CHECK_EQUAL(t.hertz(), 2);
	BOOST_CHECK_EQUAL(t.msPerTick(), 500);

	t.bpm(240);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 125000);
	BOOST_CHECK_EQUAL(t.bpm(), 240);
	BOOST_CHECK_EQUAL(t.module_tempo(), 48);
	BOOST_CHECK_EQUAL(t.hertz(), 8);
	BOOST_CHECK_EQUAL(t.msPerTick(), 125);

	t.ticksPerBeat = 350;

	t.bpm(120);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 1429);
	BOOST_CHECK_EQUAL(t.bpm(), 120);
	BOOST_CHECK_EQUAL(t.module_tempo(), 4200);
	BOOST_CHECK_EQUAL(t.hertz(), 700);
	BOOST_CHECK_EQUAL(t.msPerTick(), 1);
}

BOOST_AUTO_TEST_CASE(module)
{
	BOOST_TEST_MESSAGE("Testing tempo in module speed/tempo");

	gm::Tempo t;
	t.ticksPerBeat = 4;

	t.module(5, 140);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 35714);
	BOOST_CHECK_EQUAL(t.bpm(), 420);
	BOOST_CHECK_EQUAL(t.module_tempo(), 140);
	BOOST_CHECK_EQUAL(t.hertz(), 28);
	BOOST_CHECK_EQUAL(t.msPerTick(), 36);
	BOOST_CHECK_EQUAL(t.module_speed(), 5);

	t.module(t.module_speed(), 150);
	BOOST_CHECK_EQUAL(round(t.usPerTick), 33333);
	BOOST_CHECK_EQUAL(t.bpm(), 450);
	BOOST_CHECK_EQUAL(t.module_tempo(), 150);
	BOOST_CHECK_EQUAL(t.hertz(), 30);
	BOOST_CHECK_EQUAL(t.msPerTick(), 33);
	BOOST_CHECK_EQUAL(t.module_speed(), 5);

	t.module(6, t.module_tempo());
	BOOST_CHECK_EQUAL(round(t.usPerTick), 40000);
	BOOST_CHECK_EQUAL(t.bpm(), 375);
	BOOST_CHECK_EQUAL(t.module_tempo(), 150);
	BOOST_CHECK_EQUAL(t.hertz(), 25);
	BOOST_CHECK_EQUAL(t.msPerTick(), 40);
	BOOST_CHECK_EQUAL(t.module_speed(), 6);
}

BOOST_AUTO_TEST_SUITE_END()
