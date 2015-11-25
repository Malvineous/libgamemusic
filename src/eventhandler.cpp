/**
 * @file  eventhandler.cpp
 * @brief Implementation of EventHandler functions.
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

#include <list>
#include <camoto/gamemusic/eventhandler.hpp>

using namespace camoto::gamemusic;

struct MergedEvent
{
	MergedEvent(std::shared_ptr<const Event> ev)
		:	event(ev)
	{
	}
	unsigned long absTime;
	unsigned int trackIndex;
	std::shared_ptr<const Event> event;
};

bool trackMergeByTime(const MergedEvent& a, const MergedEvent& b)
{
	if (a.absTime == b.absTime) {
		// These events occur at the same time, so try to put all the note-offs
		// first, to minimise any unnecessary note polyphony
		auto a_off = dynamic_cast<const NoteOffEvent*>(a.event.get());
		auto b_off = dynamic_cast<const NoteOffEvent*>(b.event.get());
		if (a_off && b_off) return false;
		return a_off;
	}
	return a.absTime < b.absTime;
}

void EventHandler::handleAllEvents(EventHandler::EventOrder eventOrder,
	const Music& music)
{
	switch (eventOrder) {
		case Pattern_Row_Track: {
			unsigned int patternIndex = 0;
			// For each pattern
			for (auto& pp : music.patterns) {
				this->processPattern_mergeTracks(music, pp, patternIndex);
				patternIndex++;
			}
			break;
		}

		case Pattern_Track_Row: {
			unsigned int patternIndex = 0;
			// For each pattern
			for (auto& pp : music.patterns) {
				this->processPattern_separateTracks(music, pp, patternIndex);
				patternIndex++;
			}
			break;
		}

		case Order_Row_Track: {
			unsigned int orderIndex = 0;
			// For each pattern
			for (auto& patternIndex : music.patternOrder) {
				auto& pattern = music.patterns[patternIndex];

				this->processPattern_mergeTracks(music, pattern, patternIndex);
				orderIndex++;
			}
			break;
		}

		case Order_Track_Row: {
			unsigned int orderIndex = 0;
			// For each pattern
			for (auto& patternIndex : music.patternOrder) {
				auto& pattern = music.patterns[patternIndex];

				this->processPattern_separateTracks(music, pattern, patternIndex);
				orderIndex++;
			}
			break;
		}
	}
	return;
}

void EventHandler::processPattern_mergeTracks(const Music& music,
	const Pattern& pattern, unsigned int patternIndex)
{
	// Merge all the tracks together into one big track, with all events in
	// chronological order
	std::list<MergedEvent> full;

	unsigned int trackIndex = 0;
	// For each track
	for (auto& pt : pattern) {
		std::list<MergedEvent> ltrack;

		unsigned long trackTime = 0;
		// For each event in the track
		for (auto& te : pt) {
			// If this assertion is thrown, it's because the format reader or
			// some processing function got events out of order.  They must be
			// chronological.
			trackTime += te.delay;
			MergedEvent mevent(te.event);
			mevent.absTime = trackTime;
			mevent.trackIndex = trackIndex;
			ltrack.push_back(mevent);
		}
		// Merge in with master list
		full.merge(ltrack, trackMergeByTime);
		trackIndex++;
	}

	unsigned long trackTime = 0;
	// Now run through the master list, which has all events sorted
	for (auto& me : full) {
		unsigned long deltaTime = me.absTime - trackTime;
		trackTime = me.absTime;
		me.event->processEvent(deltaTime, me.trackIndex, patternIndex, this);
	}
	assert(trackTime <= music.ticksPerTrack);
	this->endOfPattern(music.ticksPerTrack - trackTime);
	return;
}

void EventHandler::processPattern_separateTracks(const Music& music,
	const Pattern& pattern, unsigned int patternIndex)
{
	unsigned int trackIndex = 0;
	unsigned long maxTrackTime = 0;
	// For each track
	for (auto& pt : pattern) {
		unsigned long trackTime = 0;
		// For each event in the track
		for (auto& te : pt) {
			trackTime += te.delay;
			te.event->processEvent(te.delay, trackIndex, patternIndex, this);
		}
		if (trackTime > maxTrackTime) maxTrackTime = trackTime;
		this->endOfTrack(music.ticksPerTrack - trackTime);
		trackIndex++;
	}
	this->endOfPattern(music.ticksPerTrack - maxTrackTime);
	return;
}
