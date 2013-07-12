/**
 * @file   test-midi.hpp
 * @brief  Test code for generic MIDI functions.
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

#include <camoto/stream_string.hpp>
#include <camoto/util.hpp> // createString()
#include <camoto/gamemusic.hpp>
#include "../src/decode-midi.hpp"
#include "../src/encode-midi.hpp"
#include "tests.hpp"

using namespace camoto;
namespace gm = camoto::gamemusic;

struct midi_fixture: public default_sample {

	stream::string_sptr base;
	gm::MusicPtr music;

	midi_fixture()
		:	base(new stream::string())
	{
	}

	void init_read(const std::string& data)
	{
		this->base << data;
		this->base->seekg(0, stream::start);

		stream::input_sptr s = this->base;
		this->music = midiDecode(s, gm::MIDIFlags::Default,
			gm::MIDI_DEF_TICKS_PER_QUARTER_NOTE, gm::MIDI_DEF_uS_PER_QUARTER_NOTE);
	}

	void init_write()
	{
		this->music.reset(new gm::Music());
		this->music->events.reset(new gm::EventVector());

		gm::PatchBankPtr pb(new gm::PatchBank());
		pb->reserve(1);
		gm::MIDIPatchPtr p(new gm::MIDIPatch());
		p->midiPatch = 20; // Instrument #0 is MIDI patch #20
		p->percussion = false;
		pb->push_back(p);
		this->music->patches = pb;
	}

	boost::test_tools::predicate_result is_equal(const std::string& strExpected)
	{
		gm::tempo_t usPerTick;
		stream::output_sptr s = this->base;
		midiEncode(s, gm::MIDIFlags::Default, this->music, &usPerTick, NULL);

		return this->default_sample::is_equal(strExpected, this->base->str());
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
	check_freq(    8661,   1, -8);
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

	this->init_read(makeString("\x00\x90\x45\x7f" "\x00\xe0\x00\x38"));

	// Make sure enough events were generated
	BOOST_REQUIRE_GT(this->music->events->size(), 2);
	gm::EventPtr ev = this->music->events->at(2); // 0=tempo,1=note on,2=pitchbend

	gm::PitchbendEvent *pevTyped = dynamic_cast<gm::PitchbendEvent *>(ev.get());
	BOOST_REQUIRE_MESSAGE(pevTyped, createString(
		"Pitchbend event was wrongly interpreted as " << ev->getContent()));

	BOOST_REQUIRE_CLOSE(pevTyped->milliHertz / 1000.0, 433.700, 0.01);
}

BOOST_AUTO_TEST_CASE(midi_pitchbend_write)
{
	BOOST_TEST_MESSAGE("Testing generation of pitchbend event");

	this->init_write();

	gm::NoteOnEvent *pevNote = new gm::NoteOnEvent();
	gm::EventPtr ev(pevNote);
	pevNote->absTime = 0;
	pevNote->channel = 1;
	pevNote->milliHertz = 440000;
	pevNote->instrument = 0;
	this->music->events->push_back(ev);

	gm::PitchbendEvent *pevTyped = new gm::PitchbendEvent();
	ev.reset(pevTyped);
	pevTyped->absTime = 10;
	pevTyped->channel = 1;
	pevTyped->milliHertz = 433700;
	this->music->events->push_back(ev);

	BOOST_CHECK_MESSAGE(
		is_equal(makeString(
			"\x00\xc0\x14"        // set instrument
			"\x00\x90\x45\x7f"    // note on
			"\x0a\xe0\x00\x38"    // pitchbend
			"\x00\xff\x2f\x00"    // eof
		)),
		"Error generating pitchbend event"
	);

}

BOOST_AUTO_TEST_CASE(midi_runningstatus_write)
{
	BOOST_TEST_MESSAGE("Testing generation of running status events");

	this->init_write();

	gm::NoteOnEvent *pevNote = new gm::NoteOnEvent();
	gm::EventPtr ev(pevNote);
	pevNote->absTime = 0;
	pevNote->channel = 1;
	pevNote->milliHertz = 440000;
	pevNote->instrument = 0;
	this->music->events->push_back(ev);

	gm::NoteOffEvent *pevNoteOff = new gm::NoteOffEvent();
	ev.reset(pevNoteOff);
	pevNoteOff->absTime = 0;
	pevNoteOff->channel = 1;
	this->music->events->push_back(ev);

	pevNote = new gm::NoteOnEvent();
	ev.reset(pevNote);
	pevNote->absTime = 0;
	pevNote->channel = 2;
	pevNote->milliHertz = 440000;
	pevNote->instrument = 0;
	this->music->events->push_back(ev);

	pevNoteOff = new gm::NoteOffEvent();
	ev.reset(pevNoteOff);
	pevNoteOff->absTime = 0;
	pevNoteOff->channel = 2;
	this->music->events->push_back(ev);

	pevNote = new gm::NoteOnEvent();
	ev.reset(pevNote);
	pevNote->absTime = 0;
	pevNote->channel = 2;
	pevNote->milliHertz = 440000;
	pevNote->instrument = 0;
	this->music->events->push_back(ev);

	pevNoteOff = new gm::NoteOffEvent();
	ev.reset(pevNoteOff);
	pevNoteOff->absTime = 0;
	pevNoteOff->channel = 2;
	this->music->events->push_back(ev);


	BOOST_CHECK_MESSAGE(
		is_equal(makeString(
			"\x00\xc0\x14"        // set instrument
			"\x00\x90\x45\x7f"    // note on
			"\x00\x45\x00"        // note off
			"\x00\xc1\x14"        // set instrument
			"\x00\x91\x45\x7f"    // note on
			"\x00\x45\x00"        // note off
			"\x00\x45\x7f"        // note on
			"\x00\x45\x00"        // note off
			"\x00\xff\x2f\x00"    // eof
		)),
		"Error generating of running status events"
	);

}

BOOST_AUTO_TEST_SUITE_END()
