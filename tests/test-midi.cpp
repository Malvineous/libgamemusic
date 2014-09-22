/**
 * @file   test-midi.cpp
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

struct test_midi: public test_main
{
	stream::string_sptr base;
	gm::MusicPtr music;
	gm::PatternPtr pattern;
	gm::TrackPtr track1, track2;

	test_midi()
		:	base(new stream::string())
	{
	}

	void init_read(const std::string& data)
	{
		this->base << data;
		this->base->seekg(0, stream::start);

		stream::input_sptr s = this->base;

		gm::Tempo initialTempo;
		initialTempo.ticksPerQuarterNote(gm::MIDI_DEF_TICKS_PER_QUARTER_NOTE);
		initialTempo.usPerQuarterNote(gm::MIDI_DEF_uS_PER_QUARTER_NOTE);

		this->music = midiDecode(s, gm::MIDIFlags::Default, initialTempo);
	}

	void init_write()
	{
		this->music.reset(new gm::Music());
		this->pattern.reset(new gm::Pattern);
		this->music->patterns.push_back(this->pattern);
		this->music->patternOrder.push_back(0);

		gm::TrackInfo ti;
		ti.channelType = gm::TrackInfo::MIDIChannel;
		ti.channelIndex = 0;
		this->music->trackInfo.push_back(ti);

		ti.channelType = gm::TrackInfo::MIDIChannel;
		ti.channelIndex = 1;
		this->music->trackInfo.push_back(ti);

		this->track1.reset(new gm::Track);
		this->track2.reset(new gm::Track);
		this->pattern->push_back(this->track1);
		this->pattern->push_back(this->track2);

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
		midiEncode(s, this->music, gm::MIDIFlags::Default, NULL,
			gm::EventHandler::Order_Row_Track, NULL);

		return this->test_main::is_equal(strExpected, this->base->str());
	}

};

BOOST_FIXTURE_TEST_SUITE(midi, test_midi)

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

	check_freq(    8175,   0, 0 +8192);
	check_freq(    8661,   1, -9 +8192);
	check_freq(  110000,  45, 0 +8192);
	check_freq(  220000,  57, 0 +8192);
	check_freq(  440000,  69, 0 +8192);
	check_freq( 1760000,  93, 0 +8192);
	check_freq( 7040000, 117, 0 +8192);
	check_freq( 9956063, 123, 0 +8192);
	check_freq(12543853, 127, 0 +8192);

}

BOOST_AUTO_TEST_CASE(midi_pitchbend_read)
{
	BOOST_TEST_MESSAGE("Testing interpretation of pitchbend event");

	this->init_read(STRING_WITH_NULLS("\x00\x90\x45\x7f" "\x00\xe0\x00\x38"));

	gm::PatternPtr pattern = this->music->patterns[0];
	gm::TrackPtr track = pattern->at(0);

	// Make sure enough events were generated
	BOOST_REQUIRE_EQUAL(track->size(), 2);
	gm::TrackEvent& te = track->at(1); // 0=note on,1=pitchbend

	gm::EffectEvent *pevTyped = dynamic_cast<gm::EffectEvent *>(te.event.get());
	BOOST_REQUIRE_MESSAGE(pevTyped, createString(
		"Pitchbend event was wrongly interpreted as " << te.event->getContent()));

	BOOST_REQUIRE_EQUAL(pevTyped->type, gm::EffectEvent::PitchbendNote);
	BOOST_REQUIRE_CLOSE(pevTyped->data / 1000.0, 433.700, 0.01);
}

BOOST_AUTO_TEST_CASE(midi_pitchbend_write)
{
	BOOST_TEST_MESSAGE("Testing generation of pitchbend event");

	this->init_write();

	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 440000;
		ev->instrument = 0;
		this->track1->push_back(te);
	}
	{
		gm::TrackEvent te;
		te.delay = 10;
		gm::EffectEvent *ev = new gm::EffectEvent();
		te.event.reset(ev);
		ev->type = gm::EffectEvent::PitchbendNote;
		ev->data = 433700;
		this->track1->push_back(te);
	}
	this->music->ticksPerTrack = 10;

	BOOST_CHECK_MESSAGE(
		is_equal(STRING_WITH_NULLS(
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

	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 440000;
		ev->instrument = 0;
		this->track1->push_back(te);
	}
	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOffEvent *ev = new gm::NoteOffEvent();
		te.event.reset(ev);
		this->track1->push_back(te);
	}
	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 440000;
		ev->instrument = 0;
		this->track2->push_back(te);
	}
	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOffEvent *ev = new gm::NoteOffEvent();
		te.event.reset(ev);
		this->track2->push_back(te);
	}
	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOnEvent *ev = new gm::NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 440000;
		ev->instrument = 0;
		this->track2->push_back(te);
	}
	{
		gm::TrackEvent te;
		te.delay = 0;
		gm::NoteOffEvent *ev = new gm::NoteOffEvent();
		te.event.reset(ev);
		this->track2->push_back(te);
	}
	this->music->ticksPerTrack = 10;

	BOOST_CHECK_MESSAGE(
		is_equal(STRING_WITH_NULLS(
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

BOOST_AUTO_TEST_CASE(midi_pitchbend_convert)
{
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend(-2.0), 0);
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend(-1.0), 4096);
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend(-0.5), 6144);
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend( 0  ), 8192);
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend( 0.5), 10240);
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend( 1.0), 12288);
	BOOST_CHECK_EQUAL(gm::midiSemitonesToPitchbend( 2.0), 16383);

	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(0),    -2.0);
	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(4096), -1.0);
	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(6144), -0.5);
	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(8192),  0);
	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(10240), 0.5);
	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(12288), 1.0);
	BOOST_CHECK_EQUAL(gm::midiPitchbendToSemitones(16384), 2.0); // 16383 should be just under 2.0
}

BOOST_AUTO_TEST_SUITE_END()
