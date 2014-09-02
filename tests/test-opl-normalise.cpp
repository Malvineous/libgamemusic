/**
 * @file   test-opl-normalise.cpp
 * @brief  Test code for OPL normalisation functions.
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
#include <boost/pointer_cast.hpp>
#include <camoto/stream_string.hpp>
#include <camoto/util.hpp> // createString()
#include <camoto/gamemusic.hpp>
#include "tests.hpp"

using namespace camoto;
namespace gm = camoto::gamemusic;

gm::MusicPtr createDefaultMusic()
{
	gm::MusicPtr music(new gm::Music);
	music->patches.reset(new gm::PatchBank);

	music->patternOrder.push_back(0);

	gm::PatternPtr pattern(new gm::Pattern());
	music->patterns.push_back(pattern);

	for (unsigned int c = 0; c < 6; c++) {
		gm::TrackInfo ti;
		if (c < 1) {
			ti.channelType = gm::TrackInfo::OPLChannel;
			ti.channelIndex = c;
		} else {
			assert(c-1 < 5);
			ti.channelType = gm::TrackInfo::OPLPercChannel;
			ti.channelIndex = c - 1;
		}
		music->trackInfo.push_back(ti);

		gm::TrackPtr t(new gm::Track());
		pattern->push_back(t);

		{
			gm::TrackEvent te;
			te.delay = 1;
			gm::NoteOnEvent *ev = new gm::NoteOnEvent();
			te.event.reset(ev);
			ev->instrument = c;
			ev->milliHertz = 440000;
			ev->velocity = 255;
			t->push_back(te);
		}
	}

	{
		// Play perc inst (hihat/m) on normal channel
		gm::TrackPtr t = pattern->at(0);
		gm::TrackEvent te;
		te.delay = 1;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->instrument = 1;
		ev->milliHertz = 440000;
		ev->velocity = 255;
		t->push_back(te);
	}
	{
		// Play perc inst (topcym/c) on normal channel
		gm::TrackPtr t = pattern->at(0);
		gm::TrackEvent te;
		te.delay = 1;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->instrument = 2;
		ev->milliHertz = 440000;
		ev->velocity = 255;
		t->push_back(te);
	}
	{
		// Play normal inst on perc (hihat/m) channel
		gm::TrackPtr t = pattern->at(1);
		gm::TrackEvent te;
		te.delay = 1;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->instrument = 0;
		ev->milliHertz = 440000;
		ev->velocity = 255;
		t->push_back(te);
	}
	{
		// Play normal inst on perc (topcym/c) channel
		gm::TrackPtr t = pattern->at(2);
		gm::TrackEvent te;
		te.delay = 1;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->instrument = 0;
		ev->milliHertz = 440000;
		ev->velocity = 255;
		t->push_back(te);
	}

	gm::OPLPatchPtr opl;

	opl.reset(new gm::OPLPatch);
	opl->c.attackRate = 1;
	opl->m.attackRate = 2;
	opl->rhythm = 0;
	music->patches->push_back(opl);

	opl.reset(new gm::OPLPatch);
	opl->c.attackRate = 3;
	opl->m.attackRate = 4;
	opl->rhythm = 0;
	music->patches->push_back(opl);

	opl.reset(new gm::OPLPatch);
	opl->c.attackRate = 5;
	opl->m.attackRate = 6;
	opl->rhythm = 0;
	music->patches->push_back(opl);

	opl.reset(new gm::OPLPatch);
	opl->c.attackRate = 7;
	opl->m.attackRate = 8;
	opl->rhythm = 0;
	music->patches->push_back(opl);

	opl.reset(new gm::OPLPatch);
	opl->c.attackRate = 9;
	opl->m.attackRate = 10;
	opl->rhythm = 0;
	music->patches->push_back(opl);

	opl.reset(new gm::OPLPatch);
	opl->c.attackRate = 11;
	opl->m.attackRate = 12;
	opl->rhythm = 0;
	music->patches->push_back(opl);

	return music;
}

BOOST_AUTO_TEST_SUITE(opl_normalise)

BOOST_AUTO_TEST_CASE(matching_ops)
{
	BOOST_TEST_MESSAGE("Testing normalisation with matching operators");

	gm::MusicPtr music = createDefaultMusic();
	gm::oplNormalisePerc(music, gm::OPLPerc_MatchingOps);

	gm::OPLPatchPtr oplPatch;

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(0));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(1));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0); // got changed to normal inst

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(2));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0); // got changed to normal inst

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(3));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 3);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(4));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 4);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(5));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 5);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(6));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 1); // got copied from (1) and set to hihat

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(7));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 1); // normal inst played as hihat

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(8));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 2); // got copied from (2) and set to topcym

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(9));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 2); // normal inst played as topcym
}

BOOST_AUTO_TEST_CASE(car_from_mod)
{
	BOOST_TEST_MESSAGE("Testing normalisation with carrier populated from modulator");

	gm::MusicPtr music = createDefaultMusic();
	gm::oplNormalisePerc(music, gm::OPLPerc_CarFromMod);

	gm::OPLPatchPtr oplPatch;

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(0));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(1));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0); // got changed to normal inst

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(2));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0); // got changed to normal inst

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(3));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 3);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(4));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 10); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 4);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(5));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 5);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(6));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 1); // got copied from (1) and set to hihat

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(7));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 1); // normal inst played as hihat

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(8));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 6); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 2); // got copied from (2) and set to topcym

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(9));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 2); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 2); // normal inst played as topcym
}

BOOST_AUTO_TEST_CASE(mod_from_car)
{
	BOOST_TEST_MESSAGE("Testing normalisation with carrier populated from modulator");

	gm::MusicPtr music = createDefaultMusic();
	gm::oplNormalisePerc(music, gm::OPLPerc_ModFromCar);

	gm::OPLPatchPtr oplPatch;

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(0));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(1));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0); // got changed to normal inst

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(2));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 0); // got changed to normal inst

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(3));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 8); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 3);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(4));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 4);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(5));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 5);

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(6));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 4); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 1); // got copied from (1) and set to hihat

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(7));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 2); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 1); // normal inst played as hihat

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(8));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 2); // got copied from (2) and set to topcym

	oplPatch = boost::dynamic_pointer_cast<gm::OPLPatch>(music->patches->at(9));
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, 2); // normal inst played as topcym
}

BOOST_AUTO_TEST_SUITE_END()
