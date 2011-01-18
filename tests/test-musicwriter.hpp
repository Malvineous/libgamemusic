/**
 * @file   test-musicwriter.hpp
 * @brief  Generic test code for MusicWriter class descendents.
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

#include <boost/test/unit_test.hpp>

#include <boost/algorithm/string.hpp> // for case-insensitive string compare
#include <boost/iostreams/copy.hpp>
#include <boost/bind.hpp>
#include <camoto/gamemusic.hpp>
#include <iostream>
#include <iomanip>

#include "tests.hpp"

// Local headers that will not be installed
//#include <camoto/segmented_stream.hpp>

using namespace camoto;
namespace gm = camoto::gamemusic;

// Defines to allow code reuse
#define COMBINE_CLASSNAME_EXP(c, n)  c ## _ ## n
#define COMBINE_CLASSNAME(c, n)  COMBINE_CLASSNAME_EXP(c, n)

#define TEST_VAR(n)        COMBINE_CLASSNAME(MUSIC_CLASS, n)
#define TEST_NAME(n)       TEST_VAR(n)
#define TEST_RESULT(n)     testdata_ ## n

#define FIXTURE_NAME       TEST_VAR(write_sample)
//#define EMPTY_FIXTURE_NAME TEST_VAR(sample_empty)
#define SUITE_NAME         TEST_VAR(write_suite)
//#define EMPTY_SUITE_NAME   TEST_VAR(suite_empty)
//#define INITIALSTATE_NAME  TEST_RESULT(initialstate)
#define INITIALSTATE_NAME  TEST_RESULT(noteonoff)

// Allow a string constant to be passed around with embedded nulls
#define makeString(x)  std::string((x), sizeof((x)) - 1)

struct FIXTURE_NAME: public default_sample {

	typedef boost::shared_ptr<std::stringstream> sstr_ptr;

	sstr_ptr baseData;
	void *_do; // unused var, but allows a statement to run in constructor init
	camoto::iostream_sptr baseStream;
	gm::MusicWriterPtr music;
	gm::MP_SUPPDATA suppData;
	std::map<gm::E_SUPPTYPE, sstr_ptr> suppBase;
	gm::MusicTypePtr pTestType;

	FIXTURE_NAME() :
		baseData(new std::stringstream),
		baseStream(this->baseData)
	{
		#ifdef HAS_FAT
		{
			boost::shared_ptr<std::stringstream> suppSS(new std::stringstream);
			suppSS->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
			(*suppSS) << makeString(TEST_RESULT(FAT_initialstate));
			camoto::iostream_sptr suppStream(suppSS);
			gm::SuppItem si;
			si.stream = suppStream;
			si.fnTruncate = boost::bind<void>(stringStreamTruncate, suppSS.get(), _1);
			this->suppData[gm::EST_FAT] = si;
			this->suppBase[gm::EST_FAT] = suppSS;
		}
		#endif

		BOOST_REQUIRE_NO_THROW(
			this->baseData->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

			gm::ManagerPtr pManager = gm::getManager();
			this->pTestType = pManager->getMusicTypeByCode(MUSIC_TYPE);
		);
		BOOST_REQUIRE_MESSAGE(pTestType, "Could not find music type " MUSIC_TYPE);

		this->music = this->pTestType->create(this->baseStream, this->suppData);
		BOOST_REQUIRE_MESSAGE(this->music, "Could not create music reader class");

	}

	void init(bool setInstruments)
	{
		if (setInstruments) {
			// Set some default instruments
			gm::OPLPatchBankPtr instruments(new gm::OPLPatchBank);
			instruments->setPatchCount(1);
			gm::OPLPatchPtr defInst(new gm::OPLPatch);
			instruments->setPatch(0, defInst);
			this->music->setPatchBank(instruments);
		}

		// Write the file header
		this->music->start();

		// Set a default tempo
		gm::TempoEvent *pevTempo = new gm::TempoEvent;
		gm::EventPtr evTempo(pevTempo);
		pevTempo->absTime = 0;
		pevTempo->usPerTick = INITIAL_TEMPO;
		evTempo->processEvent(this->music.get());
	}

	boost::test_tools::predicate_result is_equal(const std::string& strExpected)
	{
		this->music->finish();
		return this->default_sample::is_equal(strExpected, this->baseData->str());
	}

	boost::test_tools::predicate_result is_supp_equal(gm::E_SUPPTYPE type, const std::string& strExpected)
	{
		this->music->finish();
		return this->default_sample::is_equal(strExpected, this->suppBase[type]->str());
	}

};

BOOST_FIXTURE_TEST_SUITE(SUITE_NAME, FIXTURE_NAME)

BOOST_AUTO_TEST_CASE(TEST_NAME(write_noteonoff))
{
	BOOST_TEST_MESSAGE("Testing note on/off");

	this->init(true); // set instruments and tempo

	gm::NoteOnEvent *pevOn = new gm::NoteOnEvent();
	gm::EventPtr evOn(pevOn);
	pevOn->absTime = 0;
	pevOn->channel = 0;
	pevOn->centiHertz = 44000;
	pevOn->instrument = 0;
	evOn->processEvent(this->music.get());

	gm::NoteOffEvent *pevOff = new gm::NoteOffEvent();
	gm::EventPtr evOff(pevOff);
	pevOff->absTime = 0x10;
	pevOff->channel = 0;
	evOff->processEvent(this->music.get());

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(noteonoff))),
		"Error generating note on/off events"
	);
}

#ifdef HAS_OPL_RHYTHM_INSTRUMENTS
BOOST_AUTO_TEST_CASE(TEST_NAME(write_rhythm_hihat))
{
	BOOST_TEST_MESSAGE("Testing write of hihat rhythm instrument");

	gm::OPLPatchBankPtr instruments(new gm::OPLPatchBank);
	instruments->setPatchCount(1);
	gm::OPLPatchPtr newInst(new gm::OPLPatch);
	newInst->rhythm = 1; // hihat
	gm::OPLOperator *op = &newInst->m; // hihat uses modulator only
	op->enableTremolo = true;
	op->enableVibrato = false;
	op->enableSustain = true;
	op->enableKSR = false;
	op->freqMult = 14;
	op->scaleLevel = 1;
	op->outputLevel = 63;
	op->attackRate = 14;
	op->decayRate = 13;
	op->sustainRate = 12;
	op->releaseRate = 11;
	op->waveSelect = 6;
	instruments->setPatch(0, newInst);
	this->music->setPatchBank(instruments);

	this->init(false); // set tempo but not instruments

	gm::NoteOnEvent *pevOn = new gm::NoteOnEvent();
	gm::EventPtr evOn(pevOn);
	pevOn->centiHertz = 44000;
	pevOn->absTime = 0;
	pevOn->channel = 9; // TEMP
	pevOn->instrument = 0;
	evOn->processEvent(this->music.get());

	gm::NoteOffEvent *pevOff = new gm::NoteOffEvent();
	gm::EventPtr evOff(pevOff);
	pevOff->absTime = 0x10;
	pevOff->channel = 9;  // TEMP
	evOff->processEvent(this->music.get());

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rhythm_hihat))),
		"Error generating rhythm hihat on/off event"
	);
}
#endif

BOOST_AUTO_TEST_SUITE_END()

#undef FIXTURE_NAME
#undef SUITE_NAME

