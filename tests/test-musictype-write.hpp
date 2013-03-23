/**
 * @file   test-musictype-write.hpp
 * @brief  Generic test code for MusicType class descendents writing out files.
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

#include <boost/algorithm/string.hpp> // for case-insensitive string compare
#include <boost/bind.hpp>
#include <camoto/stream_string.hpp>
#include <camoto/util.hpp> // for TOSTRING
#include <camoto/gamemusic.hpp>
#include <iostream>
#include <iomanip>

#include "tests.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

// Defines to allow code reuse
#define COMBINE_CLASSNAME_EXP(c, n)  c ## _ ## n
#define COMBINE_CLASSNAME(c, n)  COMBINE_CLASSNAME_EXP(c, n)

#define TEST_VAR(n)        COMBINE_CLASSNAME(MUSIC_CLASS, n)
#define TEST_NAME(n)       TEST_VAR(n)
#define TEST_RESULT(n)     testdata_ ## n

#define FIXTURE_NAME       TEST_VAR(write_sample)
#define SUITE_NAME         TEST_VAR(write_suite)
#define INITIALSTATE_NAME  TEST_RESULT(noteonoff)

struct FIXTURE_NAME: public default_sample {

	stream::string_sptr base;
	MusicPtr music;
	camoto::SuppData suppData;
	MusicTypePtr pTestType;

	FIXTURE_NAME()
		:	base(new stream::string())
	{
		BOOST_REQUIRE_NO_THROW(
			ManagerPtr pManager = getManager();
			this->pTestType = pManager->getMusicTypeByCode(MUSIC_TYPE);
		);
		BOOST_REQUIRE_MESSAGE(pTestType, "Could not find music type " MUSIC_TYPE);

		this->music.reset(new Music());
		this->music->events.reset(new EventVector());
		this->music->ticksPerQuarterNote = 192;
	}

	void init(bool setInstruments)
	{
		if (setInstruments) {
			// Set some default instruments
			this->music->patches.reset(new PatchBank());
			this->music->patches->reserve(1);
#if INSTRUMENT_TYPE == 0
			OPLPatchPtr defInst(new OPLPatch());
			defInst->m.enableTremolo = defInst->c.enableTremolo = true;
			defInst->m.enableVibrato = defInst->c.enableVibrato = false;
			defInst->m.enableSustain = defInst->c.enableSustain = true;
			defInst->m.enableKSR     = defInst->c.enableKSR     = false;

			defInst->m.freqMult      = 14;
			defInst->m.scaleLevel    = 1;
			defInst->m.outputLevel   = 63;
			defInst->m.attackRate    = 14;
			defInst->m.decayRate     = 13;
			defInst->m.sustainRate   = 12;
			defInst->m.releaseRate   = 11;
			defInst->m.waveSelect    = 6;

			defInst->c.freqMult      = 7;
			defInst->c.scaleLevel    = 0;
			defInst->c.outputLevel   = 31;
			defInst->c.attackRate    = 6;
			defInst->c.decayRate     = 5;
			defInst->c.sustainRate   = 4;
			defInst->c.releaseRate   = 3;
			defInst->c.waveSelect    = 2;

			defInst->feedback = 2;
#elif INSTRUMENT_TYPE == 1
			MIDIPatchPtr defInst(new MIDIPatch());
			defInst->midiPatch = 0;
			defInst->percussion = false;
#else
#error Unknown instrument type
#endif
			this->music->patches->push_back(defInst);
		}

		// Set a default tempo
		TempoEvent *pevTempo = new TempoEvent;
		EventPtr evTempo(pevTempo);
		pevTempo->absTime = 0;
		pevTempo->channel = 0;
		pevTempo->usPerTick = INITIAL_TEMPO;
		this->music->events->push_back(evTempo);
	}

	/// Read the given data in as a music instance
	void read(const std::string& data)
	{
		this->base << data;

		this->music = this->pTestType->read(this->base, this->suppData);
		BOOST_REQUIRE_MESSAGE(this->music, "Could not create music reader class");
		this->base->truncate(0);
	}

	boost::test_tools::predicate_result is_equal(const std::string& strExpected)
	{
		this->pTestType->write(this->base, this->suppData, this->music, MusicType::Default);
		return this->default_sample::is_equal(strExpected, this->base->str());
	}

	boost::test_tools::predicate_result is_supp_equal(camoto::SuppItem::Type type, const std::string& strExpected)
	{
		stream::inout_sptr b = this->suppData[type];
		stream::string_sptr s = boost::dynamic_pointer_cast<stream::string>(b);
		assert(s);
		return this->default_sample::is_equal(strExpected, s->str());
	}

	/// Test a rhythm-mode instrument
	/**
	 * @param rhythm
	 *   Rhythm instrument (1 == hihat, etc.)
	 *
	 * @param opIndex
	 *   Operator index (0 == mod, 1 == car, 2 == both)
	 */
	void testRhythm(int rhythm, int opIndex)
	{
		PatchBankPtr instruments(new PatchBank());
		instruments->reserve(1);
		OPLPatchPtr newInst(new OPLPatch());
		newInst->rhythm = rhythm;
		newInst->feedback = 4;
		newInst->connection = true;
		OPLOperator *op;
setInstrumentAgain:
		if (opIndex == 0) op = &newInst->m;
		else op = &newInst->c;
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
		if (opIndex == 2) {
			opIndex = 0;
			goto setInstrumentAgain;
		}
		instruments->push_back(newInst);
		this->music->patches = instruments;

		this->init(false); // set tempo but not instruments

		NoteOnEvent *pevOn = new NoteOnEvent();
		EventPtr evOn(pevOn);
		pevOn->milliHertz = 440000;
		pevOn->absTime = 0;
		pevOn->channel = 1 + 8 + rhythm;
		pevOn->instrument = 0;
		this->music->events->push_back(evOn);

		NoteOffEvent *pevOff = new NoteOffEvent();
		EventPtr evOff(pevOff);
		pevOff->absTime = 0x10;
		pevOff->channel = 1 + 8 + rhythm;
		this->music->events->push_back(evOff);
	}

};

#define TEST_BEFORE_AFTER(name, before, after)	\
\
BOOST_FIXTURE_TEST_SUITE(TEST_VAR(write_suite), TEST_VAR(write_sample)) \
\
BOOST_AUTO_TEST_CASE(TEST_NAME(before_after_ ## name)) \
{ \
	BOOST_TEST_MESSAGE("Testing before/after: " TOSTRING(name)); \
\
	this->read(makeString(before)); \
\
	BOOST_CHECK_MESSAGE( \
		is_equal(makeString(after)), \
		"Before/after test failed" \
	); \
} \
\
BOOST_AUTO_TEST_SUITE_END()


BOOST_FIXTURE_TEST_SUITE(SUITE_NAME, FIXTURE_NAME)

BOOST_AUTO_TEST_CASE(TEST_NAME(write_noteonoff))
{
	BOOST_TEST_MESSAGE("Testing note on/off");

	this->init(true); // set instruments and tempo

	NoteOnEvent *pevOn = new NoteOnEvent();
	EventPtr evOn(pevOn);
	pevOn->absTime = 0;
	pevOn->channel = 1;
	pevOn->milliHertz = 440000;
	pevOn->instrument = 0;
	this->music->events->push_back(evOn);

	NoteOffEvent *pevOff = new NoteOffEvent();
	EventPtr evOff(pevOff);
	pevOff->absTime = 0x10;
	pevOff->channel = 1;
	this->music->events->push_back(evOff);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(noteonoff))),
		"Error generating note on/off events"
	);
}

#ifdef HAS_OPL_RHYTHM_INSTRUMENTS
BOOST_AUTO_TEST_CASE(TEST_NAME(write_rhythm_hihat))
{
	BOOST_TEST_MESSAGE("Testing write of hihat rhythm instrument");

	this->testRhythm(
		1, // hihat
		0  // modulator
	);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rhythm_hihat))),
		"Error generating hihat rhythm on/off event"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(write_rhythm_cymbal))
{
	BOOST_TEST_MESSAGE("Testing write of top cymbal rhythm instrument");

	this->testRhythm(
		2, // cymbal
		1  // carrier
	);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rhythm_cymbal))),
		"Error generating top cymbal rhythm on/off event"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(write_rhythm_tom))
{
	BOOST_TEST_MESSAGE("Testing write of tomtom rhythm instrument");

	this->testRhythm(
		3, // tomtom
		0  // modulator
	);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rhythm_tom))),
		"Error generating tomtom rhythm on/off event"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(write_rhythm_snare))
{
	BOOST_TEST_MESSAGE("Testing write of snare drum rhythm instrument");

	this->testRhythm(
		4, // snare drum
		1  // carrier
	);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rhythm_snare))),
		"Error generating snare drum rhythm on/off event"
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(write_rhythm_bassdrum))
{
	BOOST_TEST_MESSAGE("Testing write of bassdrum rhythm instrument");

	this->testRhythm(
		5, // bass drum
		2  // modulator + carrier
	);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(TEST_RESULT(rhythm_bassdrum))),
		"Error generating bass drum rhythm on/off event"
	);
}
#endif

BOOST_AUTO_TEST_SUITE_END()

#undef FIXTURE_NAME
#undef SUITE_NAME
