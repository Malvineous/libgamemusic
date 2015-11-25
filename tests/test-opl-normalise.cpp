/**
 * @file   test-opl-normalise.cpp
 * @brief  Test code for OPL normalisation functions.
 *
 * Copyright (C) 2010-2015 Adam Nielsen <malvineous@shikadi.net>
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
#include <camoto/util.hpp> // createString(), make_unique
#include <camoto/gamemusic.hpp>
#include "tests.hpp"

using namespace camoto;
namespace gm = camoto::gamemusic;

std::unique_ptr<gm::Music> createDefaultMusic()
{
	auto music = std::make_unique<gm::Music>();
	music->patches = std::make_shared<gm::PatchBank>();

	music->patternOrder.push_back(0);

	music->patterns.emplace_back();
	auto& pattern = music->patterns.back();

	for (unsigned int c = 0; c < 6; c++) {
		music->trackInfo.emplace_back();
		auto& ti = music->trackInfo.back();
		if (c < 1) {
			ti.channelType = gm::TrackInfo::ChannelType::OPL;
			ti.channelIndex = c;
		} else {
			assert(c-1 < 5);
			ti.channelType = gm::TrackInfo::ChannelType::OPLPerc;
			ti.channelIndex = c - 1;
		}

		pattern.emplace_back();
		auto& t = pattern.back();
		{
			auto ev = std::make_shared<gm::NoteOnEvent>();
			ev->instrument = c;
			ev->milliHertz = 440000;
			ev->velocity = 255;

			gm::TrackEvent te;
			te.delay = 1;
			te.event = ev;
			t.push_back(te);
		}
	}

	{
		// Play perc inst (hihat/m) on normal channel
		auto ev = std::make_shared<gm::NoteOnEvent>();
		ev->instrument = 1;
		ev->milliHertz = 440000;
		ev->velocity = 255;

		gm::TrackEvent te;
		te.delay = 1;
		te.event = ev;
		pattern[0].push_back(te);
	}
	{
		// Play perc inst (topcym/c) on normal channel
		auto ev = std::make_shared<gm::NoteOnEvent>();
		ev->instrument = 2;
		ev->milliHertz = 440000;
		ev->velocity = 255;

		gm::TrackEvent te;
		te.delay = 1;
		te.event = ev;
		pattern[0].push_back(te);
	}
	{
		// Play normal inst on perc (hihat/m) channel
		auto ev = std::make_shared<gm::NoteOnEvent>();
		ev->instrument = 0;
		ev->milliHertz = 440000;
		ev->velocity = 255;

		gm::TrackEvent te;
		te.delay = 1;
		te.event = ev;
		pattern[1].push_back(te);
	}
	{
		// Play normal inst on perc (topcym/c) channel
		auto ev = std::make_shared<gm::NoteOnEvent>();
		ev->instrument = 0;
		ev->milliHertz = 440000;
		ev->velocity = 255;

		gm::TrackEvent te;
		te.delay = 1;
		te.event = ev;
		pattern[2].push_back(te);
	}

	auto opl = std::make_shared<gm::OPLPatch>();
	opl->c.attackRate = 1;
	opl->m.attackRate = 2;
	opl->rhythm = gm::OPLPatch::Rhythm::Melodic;
	music->patches->push_back(opl);

	opl = std::make_shared<gm::OPLPatch>();
	opl->c.attackRate = 3;
	opl->m.attackRate = 4;
	opl->rhythm = gm::OPLPatch::Rhythm::Melodic;
	music->patches->push_back(opl);

	opl = std::make_shared<gm::OPLPatch>();
	opl->c.attackRate = 5;
	opl->m.attackRate = 6;
	opl->rhythm = gm::OPLPatch::Rhythm::Melodic;
	music->patches->push_back(opl);

	opl = std::make_shared<gm::OPLPatch>();
	opl->c.attackRate = 7;
	opl->m.attackRate = 8;
	opl->rhythm = gm::OPLPatch::Rhythm::Melodic;
	music->patches->push_back(opl);

	opl = std::make_shared<gm::OPLPatch>();
	opl->c.attackRate = 9;
	opl->m.attackRate = 10;
	opl->rhythm = gm::OPLPatch::Rhythm::Melodic;
	music->patches->push_back(opl);

	opl = std::make_shared<gm::OPLPatch>();
	opl->c.attackRate = 11;
	opl->m.attackRate = 12;
	opl->rhythm = gm::OPLPatch::Rhythm::Melodic;
	music->patches->push_back(opl);

	return music;
}

BOOST_AUTO_TEST_SUITE(opl_denormalise)

BOOST_AUTO_TEST_CASE(matching_ops)
{
	BOOST_TEST_MESSAGE("Testing denormalisation with matching operators");

	auto music = createDefaultMusic();
	gm::oplDenormalisePerc(*music, gm::OPLNormaliseType::MatchingOps);

	gm::OPLPatch* oplPatch;

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic); // got changed to normal inst

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic); // got changed to normal inst

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TomTom);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::SnareDrum);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::BassDrum);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym
}

BOOST_AUTO_TEST_CASE(car_from_mod)
{
	BOOST_TEST_MESSAGE("Testing denormalisation with carrier populated from modulator");

	auto music = createDefaultMusic();
	gm::oplDenormalisePerc(*music, gm::OPLNormaliseType::CarFromMod);

	gm::OPLPatch* oplPatch;

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic); // got changed to normal inst

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic); // got changed to normal inst

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TomTom);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 10); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::SnareDrum);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::BassDrum);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 6); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 2); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym
}

BOOST_AUTO_TEST_CASE(mod_from_car)
{
	BOOST_TEST_MESSAGE("Testing denormalisation with carrier populated from modulator");

	auto music = createDefaultMusic();
	gm::oplDenormalisePerc(*music, gm::OPLNormaliseType::ModFromCar);

	gm::OPLPatch* oplPatch;

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic); // got changed to normal inst

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Melodic); // got changed to normal inst

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 8); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TomTom);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::SnareDrum);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::BassDrum);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 4); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 2); // swapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(opl_normalise)

BOOST_AUTO_TEST_CASE(matching_ops)
{
	BOOST_TEST_MESSAGE("Testing normalisation with matching operators");

	auto music = createDefaultMusic();
	gm::oplDenormalisePerc(*music, gm::OPLNormaliseType::MatchingOps);
	auto newPatchBank = gm::oplNormalisePerc(*music, gm::OPLNormaliseType::MatchingOps);

	//BOOST_CHECK_EQUAL(music->patches->size(), 6);

	gm::OPLPatch* oplPatch;

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym


	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym
}

BOOST_AUTO_TEST_CASE(car_from_mod)
{
	BOOST_TEST_MESSAGE("Testing normalisation with carrier populated from modulator");

	auto music = createDefaultMusic();
	gm::oplDenormalisePerc(*music, gm::OPLNormaliseType::CarFromMod);
	auto newPatchBank = gm::oplNormalisePerc(*music, gm::OPLNormaliseType::CarFromMod);

	//BOOST_CHECK_EQUAL(music->patches->size(), 6);

	gm::OPLPatch* oplPatch;

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 10); // still swapped/orig unchanged
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 9);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 6); // still swapped/orig unchanged
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 5);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 2); // still swapped/orig unchanged
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 1);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym


	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9); // unswapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5); // unswapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1); // unswapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym
}

BOOST_AUTO_TEST_CASE(mod_from_car)
{
	BOOST_TEST_MESSAGE("Testing normalisation with carrier populated from modulator");

	auto music = createDefaultMusic();
	gm::oplDenormalisePerc(*music, gm::OPLNormaliseType::ModFromCar);
	auto newPatchBank = gm::oplNormalisePerc(*music, gm::OPLNormaliseType::ModFromCar);

	//BOOST_CHECK_EQUAL(music->patches->size(), 6);

	gm::OPLPatch* oplPatch;

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 8); // still swapped/orig unchanged
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 7);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 4); // still swapped/orig unchanged
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 3);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 2); // still swapped/orig unchanged
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 1);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(music->patches->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym


	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(0).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(1).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(2).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(3).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 7); // unswapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 8);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(4).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 9);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 10);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(5).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 11);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 12);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::Unknown);

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(6).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 3); // unswapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 4);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // got copied from (1) and set to hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(7).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1); // unswapped
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::HiHat); // normal inst played as hihat

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(8).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 5);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 6);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // got copied from (2) and set to topcym

	oplPatch = dynamic_cast<gm::OPLPatch*>(newPatchBank->at(9).get());
	assert(oplPatch);
	BOOST_CHECK_EQUAL(oplPatch->c.attackRate, 1);
	BOOST_CHECK_EQUAL(oplPatch->m.attackRate, 2);
	//BOOST_CHECK_EQUAL(oplPatch->rhythm, gm::OPLPatch::Rhythm::TopCymbal); // normal inst played as topcym
}

BOOST_AUTO_TEST_SUITE_END()
