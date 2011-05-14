/**
 * @file   test-midi.hpp
 * @brief  Test code for generic MIDI functions.
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

#include <camoto/util.hpp> // createString()
#include <camoto/gamemusic.hpp>
#include "../src/mus-generic-midi.hpp"
#include "tests.hpp"

using namespace camoto;
namespace gm = camoto::gamemusic;

struct midi_fixture: public default_sample {

	typedef boost::shared_ptr<std::stringstream> sstr_ptr;

	sstr_ptr baseData;
	camoto::iostream_sptr baseStream;
	gm::MusicReaderPtr musIn;
	gm::MusicWriterPtr musOut;
	std::map<camoto::SuppItem::Type, sstr_ptr> suppBase;
	gm::MusicTypePtr pTestType;
	gm::PatchBankPtr bank;

	midi_fixture() :
		baseData(new std::stringstream),
		baseStream(this->baseData)
	{
		this->baseData->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
	}

	void init_read(const std::string& data)
	{
		(*this->baseData) << data;

		BOOST_REQUIRE_NO_THROW(
			this->baseData->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

			gm::ManagerPtr pManager = gm::getManager();
			this->pTestType = pManager->getMusicTypeByCode("rawmidi");
		);
		BOOST_REQUIRE_MESSAGE(pTestType, "Could not find music type rawmidi");

		camoto::SuppData suppData;
		this->musIn = this->pTestType->open(this->baseStream, suppData);
		BOOST_REQUIRE_MESSAGE(this->musIn, "Could not create music reader class");

		this->bank = this->musIn->getPatchBank();
		//BOOST_REQUIRE_MESSAGE(this->bank, "Music reader didn't supply an instrument bank");
	}

	void init_write()
	{
		BOOST_REQUIRE_NO_THROW(
			this->baseData->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

			gm::ManagerPtr pManager = gm::getManager();
			this->pTestType = pManager->getMusicTypeByCode("rawmidi");
		);
		BOOST_REQUIRE_MESSAGE(pTestType, "Could not find music type rawmidi");

		camoto::SuppData suppData;
		this->musOut = this->pTestType->create(this->baseStream, suppData);
		BOOST_REQUIRE_MESSAGE(this->musOut, "Could not create music writer class");

		gm::MIDIPatchBankPtr pb(new gm::MIDIPatchBank());
		pb->setPatchCount(1);
		gm::MIDIPatchPtr p(new gm::MIDIPatch());
		p->midiPatch = 0;
		p->percussion = false;
		pb->setPatch(0, p);
		this->musOut->setPatchBank(pb);
		this->musOut->start();
	}

	boost::test_tools::predicate_result is_equal(const std::string& strExpected)
	{
		this->musOut->finish();
		return this->default_sample::is_equal(strExpected, this->baseData->str());
	}

};

BOOST_FIXTURE_TEST_SUITE(midi, midi_fixture)

BOOST_AUTO_TEST_CASE(noteToFreq)
{
	BOOST_TEST_MESSAGE("Testing note to frequency conversion");

	BOOST_CHECK_EQUAL(gm::midiToFreq(  0),     8175);
	BOOST_CHECK_EQUAL(gm::midiToFreq(  1),     8661);
	BOOST_CHECK_EQUAL(gm::midiToFreq( 45),   110000);
	BOOST_CHECK_EQUAL(gm::midiToFreq( 57),   220000);
	BOOST_CHECK_EQUAL(gm::midiToFreq( 69),   440000);
	BOOST_CHECK_EQUAL(gm::midiToFreq( 93),  1760000);
	BOOST_CHECK_EQUAL(gm::midiToFreq(117),  7040000);
	BOOST_CHECK_EQUAL(gm::midiToFreq(123),  9956063);
	BOOST_CHECK_EQUAL(gm::midiToFreq(127), 12543853);
}

BOOST_AUTO_TEST_CASE(freqToNote)
{
	BOOST_TEST_MESSAGE("Testing frequency to note conversion");

	uint8_t note;
	int16_t bend;
#define check_freq(f, n, b) \
	gm::freqToMIDI(f, &note, &bend, 0xFF); \
	BOOST_CHECK_EQUAL(note, n); \
	BOOST_CHECK_EQUAL(bend, b);

	check_freq(    8175,   0, 0);
	check_freq(    8661,   1, 0);
	check_freq(  110000,  45, 0);
	check_freq(  220000,  57, 0);
	check_freq(  440000,  69, 0);
	check_freq( 1760000,  93, 0);
	check_freq( 7040000, 117, 0);
	check_freq( 9956063, 123, 0);
	check_freq(12543853, 127, 0);

}

BOOST_AUTO_TEST_CASE(midi_pitchbend_read)
{
	BOOST_TEST_MESSAGE("Testing interpretation of pitchbend event");

	this->init_read(makeString("\x00\x90\x45\x40" "\x00\xe0\x00\x38"));

	gm::EventPtr ev = this->musIn->readNextEvent(); // implicit tempo
	ev = this->musIn->readNextEvent(); // note on
	ev = this->musIn->readNextEvent(); // pitchbend
	BOOST_REQUIRE_MESSAGE(ev, "Test data contains no events!");

	gm::PitchbendEvent *pevTyped = dynamic_cast<gm::PitchbendEvent *>(ev.get());
	BOOST_REQUIRE_MESSAGE(pevTyped, createString(
		"Event was wrongly interpreted as " << ev->getContent()));

	BOOST_REQUIRE_CLOSE(pevTyped->milliHertz / 1000.0, 433.700, 0.01);
}

BOOST_AUTO_TEST_CASE(midi_pitchbend_write)
{
	BOOST_TEST_MESSAGE("Testing generation of pitchbend event");

	this->init_write();

	gm::NoteOnEvent *pevNote = new gm::NoteOnEvent();
	gm::EventPtr ev(pevNote);
	pevNote->absTime = 0;
	pevNote->channel = 0;
	pevNote->milliHertz = 440000;
	pevNote->instrument = 0;
	ev->processEvent(this->musOut.get());

	gm::PitchbendEvent *pevTyped = new gm::PitchbendEvent();
	ev.reset(pevTyped);
	pevTyped->absTime = 10;
	pevTyped->channel = 0;
	pevTyped->milliHertz = 433700;
	ev->processEvent(this->musOut.get());

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(
			"\x00\xc0\x00"        // set instrument
			"\x00\x90\x45\x40"    // note on
			"\x0a\xe0\x00\x38"    // pitchbend
			"\x00\xff\x2f\x00"    // eof
		)),
		"Error generating pitchbend event"
	);

}

BOOST_AUTO_TEST_SUITE_END()
