/**
 * @file   test-musicreader.hpp
 * @brief  Generic test code for MusicReader class descendents.
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

#define FIXTURE_NAME       TEST_VAR(read_sample)
//#define EMPTY_FIXTURE_NAME TEST_VAR(read_sample_empty)
#define SUITE_NAME         TEST_VAR(read_suite)
//#define EMPTY_SUITE_NAME   TEST_VAR(read_suite_empty)
//#define INITIALSTATE_NAME  TEST_RESULT(initialstate)
#define INITIALSTATE_NAME  TEST_RESULT(noteonoff)

// Allow a string constant to be passed around with embedded nulls
#define makeString(x)  std::string((x), sizeof((x)) - 1)

struct FIXTURE_NAME: public default_sample {

	typedef boost::shared_ptr<std::stringstream> sstr_ptr;

	sstr_ptr baseData;
	camoto::iostream_sptr baseStream;
	gm::MusicReaderPtr music;
	gm::MP_SUPPDATA suppData;
	std::map<gm::E_SUPPTYPE, sstr_ptr> suppBase;
	gm::MusicTypePtr pTestType;
	gm::PatchBankPtr bank;

	FIXTURE_NAME() :
		baseData(new std::stringstream),
		baseStream(this->baseData)
	{
		this->baseData->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
	}

	void init(const std::string& data)
	{
		(*this->baseData) << data;

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

		this->music = this->pTestType->open(this->baseStream, this->suppData);
		BOOST_REQUIRE_MESSAGE(this->music, "Could not create music reader class");

		this->bank = this->music->getPatchBank();
		BOOST_REQUIRE_MESSAGE(this->bank, "Music reader didn't supply an instrument bank");
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
		gm::EventPtr evOn = this->music->readNextEvent();
		BOOST_REQUIRE_MESSAGE(evOn, "Test data contains no events!");

		gm::NoteOnEvent *pevNoteOn = dynamic_cast<gm::NoteOnEvent *>(evOn.get());

		if (!pevNoteOn) {
			// First event wasn't note-on, keep reading until we reach one.  This
			// allows extra initial events (like tempo) to be ignored.
			do {
				evOn = this->music->readNextEvent();
				pevNoteOn = dynamic_cast<gm::NoteOnEvent *>(evOn.get());
			} while ((evOn) && (!pevNoteOn));
		}

		BOOST_REQUIRE_MESSAGE(pevNoteOn, "No note-on events found");

		// Note must play immediately at start of song
		BOOST_REQUIRE_EQUAL(pevNoteOn->absTime, 0);

		gm::OPLPatchBankPtr instruments = boost::dynamic_pointer_cast<gm::OPLPatchBank>(this->bank);
		BOOST_REQUIRE_MESSAGE(instruments, "Test fault: Tried to run OPL test for "
			"music format that doesn't have OPL instruments");

		gm::OPLPatchPtr inst = instruments->getTypedPatch(0);
		// Make sure instrument is the correct rhythm-mode one
		BOOST_REQUIRE_EQUAL(inst->rhythm, rhythm);

		// Make sure the correct parameters have been set
		gm::OPLOperator *op;
checkInstrumentAgain:
		if (opIndex == 0) op = &inst->m;
		else op = &inst->c;
		BOOST_REQUIRE_EQUAL(op->enableTremolo, true);
		BOOST_REQUIRE_EQUAL(op->enableVibrato, false);
		BOOST_REQUIRE_EQUAL(op->enableSustain, true);
		BOOST_REQUIRE_EQUAL(op->enableKSR, false);
		BOOST_REQUIRE_EQUAL(op->freqMult, 14);
		BOOST_REQUIRE_EQUAL(op->scaleLevel, 1);
		BOOST_REQUIRE_EQUAL(op->outputLevel, 63);
		BOOST_REQUIRE_EQUAL(op->attackRate, 14);
		BOOST_REQUIRE_EQUAL(op->decayRate, 13);
		BOOST_REQUIRE_EQUAL(op->sustainRate, 12);
		BOOST_REQUIRE_EQUAL(op->releaseRate, 11);
		BOOST_REQUIRE_EQUAL(op->waveSelect, 6);
		if (opIndex == 2) {
			opIndex = 0;
			goto checkInstrumentAgain;
		}

		gm::EventPtr evOff = this->music->readNextEvent();
		BOOST_REQUIRE_MESSAGE(evOff,
			"Test data didn't contain an event following the note-on!");

		gm::NoteOffEvent *pevNoteOff = dynamic_cast<gm::NoteOffEvent *>(evOff.get());
		BOOST_REQUIRE_MESSAGE(pevNoteOff,
			"Event following note-on was not a note-off");

		BOOST_REQUIRE_EQUAL(pevNoteOff->channel, pevNoteOn->channel);
		// Note must be of a particular length.
		BOOST_REQUIRE_EQUAL(pevNoteOff->absTime, 0x10);
	}

};

BOOST_FIXTURE_TEST_SUITE(SUITE_NAME, FIXTURE_NAME)

// Define an ISINSTANCE_TEST macro which we use to confirm the initial state
// is a valid instance of this format.  This is defined as a macro so the
// format-specific code can reuse it later to test various invalid formats.
#define ISINSTANCE_TEST(c, d, r) \
	BOOST_AUTO_TEST_CASE(TEST_NAME(isinstance_ ## c)) \
	{ \
		BOOST_TEST_MESSAGE("isInstance check (" MUSIC_TYPE "; " #c ")"); \
		\
		gm::ManagerPtr pManager(gm::getManager()); \
		gm::MusicTypePtr pTestType(pManager->getMusicTypeByCode(MUSIC_TYPE)); \
		BOOST_REQUIRE_MESSAGE(pTestType, "Could not find music type " MUSIC_TYPE); \
		\
		boost::shared_ptr<std::stringstream> psstrBase(new std::stringstream); \
		(*psstrBase) << makeString(d); \
		camoto::iostream_sptr psBase = boost::dynamic_pointer_cast<std::iostream>(psstrBase); \
		\
		BOOST_CHECK_EQUAL(pTestType->isInstance(psBase), r); \
	}

ISINSTANCE_TEST(c00, INITIALSTATE_NAME, gm::EC_DEFINITELY_YES);


// Define an INVALIDDATA_TEST macro which we use to confirm the reader correctly
// rejects a file with invalid data.  This is defined as a macro so the
// format-specific code can reuse it later to test various invalid formats.
#ifdef HAS_FAT
#	define INVALIDDATA_FATCODE(d) \
	{ \
		boost::shared_ptr<std::stringstream> suppSS(new std::stringstream); \
		suppSS->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit); \
		(*suppSS) << makeString(d); \
		camoto::iostream_sptr suppStream(suppSS); \
		gm::SuppItem si; \
		si.stream = suppStream; \
		si.fnTruncate = boost::bind<void>(stringStreamTruncate, suppSS.get(), _1); \
		suppData[gm::EST_FAT] = si; \
	}
#else
#	define INVALIDDATA_FATCODE(d)
#endif

#define INVALIDDATA_TEST(c, d) \
	INVALIDDATA_TEST_FULL(c, d, 0)

#define INVALIDDATA_TEST_FAT(c, d, f) \
	INVALIDDATA_TEST_FULL(c, d, f)

#define INVALIDDATA_TEST_FULL(c, d, f) \
	/* Run an isInstance test first to make sure the data is accepted */ \
	ISINSTANCE_TEST(invaliddata_ ## c, d, gm::EC_DEFINITELY_YES); \
	\
	BOOST_AUTO_TEST_CASE(TEST_NAME(invaliddata_ ## c)) \
	{ \
		BOOST_TEST_MESSAGE("invalidData check (" MUSIC_TYPE "; " #c ")"); \
		\
		boost::shared_ptr<gm::Manager> pManager(gm::getManager()); \
		gm::MapTypePtr pTestType(pManager->getMapTypeByCode(MUSIC_TYPE)); \
		\
		/* Prepare an invalid map */ \
		boost::shared_ptr<std::stringstream> psstrBase(new std::stringstream); \
		(*psstrBase) << makeString(d); \
		camoto::iostream_sptr psBase(psstrBase); \
		\
		gm::MP_SUPPDATA suppData; \
		INVALIDDATA_FATCODE(f) \
		\
		BOOST_CHECK_THROW( \
			gm::MapPtr pMap(pTestType->open(psBase, suppData)), \
			std::ios::failure \
		); \
	}

BOOST_AUTO_TEST_CASE(TEST_NAME(noteonoff))
{
	BOOST_TEST_MESSAGE("Testing note on/off");

	this->init(makeString(testdata_noteonoff));

	gm::EventPtr evOn = this->music->readNextEvent();
	BOOST_REQUIRE_MESSAGE(evOn, "Test data contains no events!");

	gm::NoteOnEvent *pevNoteOn = dynamic_cast<gm::NoteOnEvent *>(evOn.get());

	if (!pevNoteOn) {
		// First event wasn't note-on, keep reading until we reach one.  This
		// allows extra initial events (like tempo) to be ignored.
		do {
			evOn = this->music->readNextEvent();
			pevNoteOn = dynamic_cast<gm::NoteOnEvent *>(evOn.get());
		} while ((evOn) && (!pevNoteOn));
	}

	BOOST_REQUIRE_MESSAGE(pevNoteOn, "No note-on events found");

	// Allow a certain amount of leeway, as frequency values can only be
	// approximated by many music formats.
	BOOST_REQUIRE_CLOSE(pevNoteOn->milliHertz / 1000.0, 440.0, 0.01);

	gm::EventPtr evOff = this->music->readNextEvent();
	BOOST_REQUIRE_MESSAGE(evOff,
		"Test data didn't contain an event following the note-on!");

	gm::NoteOffEvent *pevNoteOff = dynamic_cast<gm::NoteOffEvent *>(evOff.get());
	BOOST_REQUIRE_MESSAGE(pevNoteOff,
		"Event following note-on was not a note-off");

	BOOST_REQUIRE_EQUAL(pevNoteOff->channel, pevNoteOn->channel);
}

#ifdef HAS_OPL_RHYTHM_INSTRUMENTS
BOOST_AUTO_TEST_CASE(TEST_NAME(rhythm_hihat))
{
	BOOST_TEST_MESSAGE("Testing hihat rhythm instrument");

	this->init(makeString(testdata_rhythm_hihat));

	this->testRhythm(
		1, // hihat
		0  // modulator
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(rhythm_cymbal))
{
	BOOST_TEST_MESSAGE("Testing top cymbal rhythm instrument");

	this->init(makeString(testdata_rhythm_cymbal));

	this->testRhythm(
		2, // top cymbal
		1  // carrier
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(rhythm_tom))
{
	BOOST_TEST_MESSAGE("Testing tomtom rhythm instrument");

	this->init(makeString(testdata_rhythm_tom));

	this->testRhythm(
		3, // tomtom
		0  // modulator
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(rhythm_snare))
{
	BOOST_TEST_MESSAGE("Testing snare rhythm instrument");

	this->init(makeString(testdata_rhythm_snare));

	this->testRhythm(
		4, // snare drum
		1  // carrier
	);
}

BOOST_AUTO_TEST_CASE(TEST_NAME(rhythm_bassdrum))
{
	BOOST_TEST_MESSAGE("Testing bass drum rhythm instrument");

	this->init(makeString(testdata_rhythm_bassdrum));

	this->testRhythm(
		5, // bass drum
		2  // modulator + carrier
	);
}
#endif

BOOST_AUTO_TEST_SUITE_END()

#undef FIXTURE_NAME
#undef SUITE_NAME
