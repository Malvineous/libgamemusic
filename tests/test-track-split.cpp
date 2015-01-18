/**
 * @file   test-track-split.cpp
 * @brief  Test code for track split algorithm.
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
#include <camoto/util.hpp> // createString()
#include <camoto/gamemusic.hpp>
#include "../src/track-split.hpp"
#include "tests.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

BOOST_AUTO_TEST_SUITE(track_split)

BOOST_AUTO_TEST_CASE(split)
{
	BOOST_TEST_MESSAGE("Testing track split");

	MusicPtr music(new Music());
	PatternPtr pattern(new Pattern());
	music->patterns.push_back(pattern);
	music->patternOrder.push_back(0);

	TrackInfo ti;
	ti.channelType = TrackInfo::MIDIChannel;
	ti.channelIndex = 0;
	music->trackInfo.push_back(ti);

	ti.channelType = TrackInfo::MIDIChannel;
	ti.channelIndex = 1;
	music->trackInfo.push_back(ti);

	ti.channelType = TrackInfo::MIDIChannel;
	ti.channelIndex = 2;
	music->trackInfo.push_back(ti);

	// Start off with a 'normal' track that requires no overflow
	TrackPtr track1(new Track());
	pattern->push_back(track1);

	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 330000;
		ev->instrument = 0;
		track1->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 10;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 330000;
		te.event.reset(ev);
		track1->push_back(te);
	}

	// Then do overflow
	TrackPtr track2(new Track());
	pattern->push_back(track2);

	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 440000;
		ev->instrument = 0;
		track2->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 550000;
		ev->instrument = 0;
		track2->push_back(te);
	}
	// Do note-offs in reverse order but at the same instant
	{
		TrackEvent te;
		te.delay = 10;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 550000;
		te.event.reset(ev);
		track2->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 440000;
		te.event.reset(ev);
		track2->push_back(te);
	}

	{
		TrackEvent te;
		te.delay = 10;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 441000;
		ev->instrument = 0;
		track2->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 442000;
		ev->instrument = 0;
		track2->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 443000;
		ev->instrument = 0;
		track2->push_back(te);
	}
	// Note offs
	{
		TrackEvent te;
		te.delay = 10;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 442000;
		te.event.reset(ev);
		track2->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 443000;
		te.event.reset(ev);
		track2->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 441000;
		te.event.reset(ev);
		track2->push_back(te);
	}

	// Then do another overflow to ensure track insertion happens in the correctly
	TrackPtr track3(new Track());
	pattern->push_back(track3);

	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 660000;
		ev->instrument = 0;
		track3->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		NoteOnEvent *ev = new NoteOnEvent();
		te.event.reset(ev);
		ev->milliHertz = 770000;
		ev->instrument = 0;
		track3->push_back(te);
	}
	// This time do note-offs in the same order as note-ons
	{
		TrackEvent te;
		te.delay = 10;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 660000;
		te.event.reset(ev);
		track3->push_back(te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
		ev->milliHertz = 770000;
		te.event.reset(ev);
		track3->push_back(te);
	}

	// Original track count
	BOOST_CHECK_EQUAL(pattern->size(), 3);
	BOOST_CHECK_EQUAL(music->trackInfo.size(), 3);

	// Do the split
	splitPolyphonicTracks(music);

	// Post-split track count
	BOOST_REQUIRE_EQUAL(pattern->size(), 6);
	BOOST_REQUIRE_EQUAL(music->trackInfo.size(), 6);

	TrackPtr track = pattern->at(0);
	BOOST_CHECK_EQUAL(track->size(), 2);
	{
		TrackEvent& te = track->at(0);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 0);
		BOOST_CHECK_EQUAL(ev->milliHertz, 330000);
	}
	{
		TrackEvent& te = track->at(1);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	track = pattern->at(1);
	BOOST_CHECK_EQUAL(track->size(), 4);
	{
		TrackEvent& te = track->at(0);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 0);
		BOOST_CHECK_EQUAL(ev->milliHertz, 440000);
	}
	{
		TrackEvent& te = track->at(1);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	{
		TrackEvent& te = track->at(2);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_CHECK_EQUAL(ev->milliHertz, 441000);
	}
	{
		TrackEvent& te = track->at(3);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	track = pattern->at(2);
	BOOST_CHECK_EQUAL(track->size(), 4);
	{
		TrackEvent& te = track->at(0);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 0);
		BOOST_CHECK_EQUAL(ev->milliHertz, 550000);
	}
	{
		TrackEvent& te = track->at(1);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	{
		TrackEvent& te = track->at(2);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_CHECK_EQUAL(ev->milliHertz, 442000);
	}
	{
		TrackEvent& te = track->at(3);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	track = pattern->at(3);
	BOOST_CHECK_EQUAL(track->size(), 2);
	{
		TrackEvent& te = track->at(0);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 20);
		BOOST_CHECK_EQUAL(ev->milliHertz, 443000);
	}
	{
		TrackEvent& te = track->at(1);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	track = pattern->at(4);
	BOOST_CHECK_EQUAL(track->size(), 2);
	{
		TrackEvent& te = track->at(0);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 0);
		BOOST_CHECK_EQUAL(ev->milliHertz, 660000);
	}
	{
		TrackEvent& te = track->at(1);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}
	track = pattern->at(5);
	BOOST_CHECK_EQUAL(track->size(), 2);
	{
		TrackEvent& te = track->at(0);
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
		BOOST_REQUIRE(ev);
		BOOST_CHECK_EQUAL(te.delay, 0);
		BOOST_CHECK_EQUAL(ev->milliHertz, 770000);
	}
	{
		TrackEvent& te = track->at(1);
		NoteOffEvent *ev = dynamic_cast<NoteOffEvent *>(te.event.get());
		BOOST_CHECK_EQUAL(te.delay, 10);
		BOOST_REQUIRE(ev);
	}

	BOOST_CHECK_EQUAL(music->trackInfo[0].channelType, TrackInfo::MIDIChannel);
	BOOST_CHECK_EQUAL(music->trackInfo[0].channelIndex, 0);
	BOOST_CHECK_EQUAL(music->trackInfo[1].channelType, TrackInfo::MIDIChannel);
	BOOST_CHECK_EQUAL(music->trackInfo[1].channelIndex, 1);
	BOOST_CHECK_EQUAL(music->trackInfo[2].channelType, TrackInfo::MIDIChannel);
	BOOST_CHECK_EQUAL(music->trackInfo[2].channelIndex, 1);
	BOOST_CHECK_EQUAL(music->trackInfo[3].channelType, TrackInfo::MIDIChannel);
	BOOST_CHECK_EQUAL(music->trackInfo[3].channelIndex, 1);
	BOOST_CHECK_EQUAL(music->trackInfo[4].channelType, TrackInfo::MIDIChannel);
	BOOST_CHECK_EQUAL(music->trackInfo[4].channelIndex, 2);
	BOOST_CHECK_EQUAL(music->trackInfo[5].channelType, TrackInfo::MIDIChannel);
	BOOST_CHECK_EQUAL(music->trackInfo[5].channelIndex, 2);
}

BOOST_AUTO_TEST_SUITE_END()
