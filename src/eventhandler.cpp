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

EventHandler::Position EventHandler::handleAllEvents(EventOrder eventOrder,
	const Music& music, unsigned int targetLoopCount)
{
	this->isGotoPending = false;
	this->tempo = music.initialTempo;

	Position pos;
	pos.orderIndex = 0;
	switch (eventOrder) {
		case Pattern_Row_Track:
		case Pattern_Track_Row:
			// Ignoring order list, so start at the first pattern
			pos.patternIndex = 0;
			break;
		case Order_Row_Track:
		case Order_Track_Row:
			// Using order list, so start at whatever pattern the first order points to
			if (pos.orderIndex < music.patternOrder.size()) {
				pos.patternIndex = music.patternOrder[pos.orderIndex];
			} else {
				pos.patternIndex = 0;
			}
			break;
	}
	pos.nextOrderIndex = -1; // should be overridden below
	pos.nextPatternIndex = -1; // should be overridden below
	pos.startRow = 0;
	pos.loop = 0;
	pos.us = 0;

	bool processing = true;
	do {
		assert(pos.orderIndex < music.patternOrder.size());
		assert(pos.patternIndex < music.patterns.size());

		// Figure out what the next pattern will be.  This might be overridden by
		// a GotoEvent later.
		if (this->isGotoPending) {
			pos.startRow = this->pendingGoto.targetRow;
			switch (this->pendingGoto.type) {
				case GotoEvent::Type::CurrentPattern:
					// Play the same pattern again
					pos.nextOrderIndex = pos.orderIndex;
					pos.nextPatternIndex = pos.patternIndex;
					break;
				case GotoEvent::Type::NextPattern:
					// Don't need to do anything, we will already be going to the next
					// pattern.
					pos.nextPatternIndex = pos.patternIndex + 1;
					pos.nextOrderIndex = pos.orderIndex + 1;
					break;
				case GotoEvent::Type::SpecificOrder:
					pos.nextOrderIndex = this->pendingGoto.targetOrder;

					// Make sure we aren't trying to process a jump-to-order when we are
					// ignoring orders and iterating through pattern by pattern.
					assert((eventOrder != Pattern_Row_Track) && (eventOrder != Pattern_Track_Row));
					pos.nextPatternIndex = 0;
					break;
			}
			this->isGotoPending = false;
		} else {
			switch (eventOrder) {
				case Pattern_Row_Track:
				case Pattern_Track_Row:
					pos.nextPatternIndex = pos.patternIndex + 1;
					break;
				case Order_Row_Track:
				case Order_Track_Row:
					pos.nextOrderIndex = pos.orderIndex + 1;
					break;
			}
			pos.startRow = 0;
		}

		// Process the current pattern
		auto& pattern = music.patterns[pos.patternIndex];
		switch (eventOrder) {
			case Pattern_Row_Track:
				processing = this->processPattern_mergeTracks(music, pattern, &pos);
				break;
			case Pattern_Track_Row:
				processing = this->processPattern_separateTracks(music, pattern, &pos);
				break;
			case Order_Row_Track:
				processing = this->processPattern_mergeTracks(music, pattern, &pos);
				break;
			case Order_Track_Row:
				processing = this->processPattern_separateTracks(music, pattern, &pos);
				break;
		}

		// Now either all the pattern's events have been processed, or a jump
		// was encountered and the pattern processing was cut off early.  Either
		// way we need to advance to a new pattern now, so update the "next" fields
		// if a jump event has been processed.
		if (this->isGotoPending) {
			pos.startRow = this->pendingGoto.targetRow;
			switch (this->pendingGoto.type) {
				case GotoEvent::Type::CurrentPattern:
					// Play the same pattern again
					pos.nextOrderIndex = pos.orderIndex;
					pos.nextPatternIndex = pos.patternIndex;
					break;
				case GotoEvent::Type::NextPattern:
					// Don't need to do anything, we will already be going to the next
					// pattern.
					break;
				case GotoEvent::Type::SpecificOrder:
					pos.nextOrderIndex = this->pendingGoto.targetOrder;

					// Make sure we aren't trying to process a jump-to-order when we are
					// ignoring orders and iterating through pattern by pattern.
					assert((eventOrder != Pattern_Row_Track) && (eventOrder != Pattern_Track_Row));
					pos.nextPatternIndex = 0;
					break;
			}
			this->isGotoPending = false;
		}

		// If we should stop processing then end now before we move to the next
		// order/pattern.  This way the next order/pattern will be preserved in
		// the pos variable, ready to be used if we were calculating a seek
		// operation.
		if (!processing) break;

		// Advance to the next pattern.  The values could be out of range here,
		// meaning we need to loop, so we have to check that.  These "next" values
		// could be either the ones we calculated at the start of the loop, or new
		// values that were substituted in during event processing, or new values
		// as a result of processing the pending GotoEvent just now.
		switch (eventOrder) {
			case Pattern_Row_Track:
			case Pattern_Track_Row:
				pos.patternIndex = pos.nextPatternIndex;
				if (pos.patternIndex >= music.patterns.size()) {
					pos.patternIndex = 0;
					pos.loop++;
				}
				break;
			case Order_Row_Track:
			case Order_Track_Row:
				pos.orderIndex = pos.nextOrderIndex;
				if (pos.orderIndex >= music.patternOrder.size()) {
					// This order is past the end of the song, so loop
					if ((music.loopDest >= 0)
						&& ((unsigned int)music.loopDest < music.patternOrder.size())
					) {
						pos.orderIndex = music.loopDest;
					} else {
						pos.orderIndex = 0;
					}
					pos.loop++;
				}
				pos.patternIndex = music.patternOrder[pos.orderIndex];
				break;
		}
	} while (processing && ((targetLoopCount == 0) || (pos.loop < targetLoopCount)));

	return pos;
}

bool EventHandler::processPattern_mergeTracks(const Music& music,
	const Pattern& pattern, Position *pos)
{
	// Merge all the tracks together into one big track, with all events in
	// chronological order
	std::list<MergedEvent> full;

	pos->row = pos->startRow;
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

		unsigned long midDelay = 0;
		if ((trackTime < pos->startRow) && (trackTime + deltaTime > pos->startRow)) {
			// startRow is after the previous event but before the current one, so
			// we need to apportion the delay.
			midDelay = pos->startRow - (trackTime + deltaTime);
		}

		trackTime = me.absTime;

		// Skip events until we reach the point where we need to start processing
		if (trackTime < pos->startRow) continue;

		pos->row += deltaTime;
		pos->us += (midDelay + deltaTime) * this->tempo.usPerTick;
		if (!me.event->processEvent(deltaTime, me.trackIndex,
			pos->patternIndex, this)
		) {
			return false;
		}
	}
	assert(trackTime <= music.ticksPerTrack);
	this->endOfPattern(music.ticksPerTrack - trackTime);
	return true;
}

bool EventHandler::processPattern_separateTracks(const Music& music,
	const Pattern& pattern, Position *pos)
{
	unsigned long maxTrackTime = 0;

	pos->row = pos->startRow;
	unsigned int trackIndex = 0;
	// For each track
	for (auto& pt : pattern) {
		unsigned long trackTime = 0;
		// For each event in the track
		for (auto& te : pt) {
			unsigned long midDelay = 0;
			if ((trackTime < pos->startRow) && (trackTime + te.delay > pos->startRow)) {
				// startRow is after the previous event but before the current one, so
				// we need to apportion the delay.
				midDelay = pos->startRow - (trackTime + te.delay);
			}

			trackTime += te.delay;

			// Skip events until we reach the point where we need to start processing
			if (trackTime < pos->startRow) continue;

			pos->row = trackTime;
			pos->us += (midDelay + te.delay) * this->tempo.usPerTick;
			if (!te.event->processEvent(te.delay, trackIndex,
				pos->patternIndex, this)
			) {
				return false;
			}
		}
		if (trackTime > maxTrackTime) maxTrackTime = trackTime;
		this->endOfTrack(music.ticksPerTrack - trackTime);
		trackIndex++;
	}
	this->endOfPattern(music.ticksPerTrack - maxTrackTime);
	return true;
}

void EventHandler::performGoto(const GotoEvent *ev)
{
	this->pendingGoto = *ev;
	this->isGotoPending = true;
	return;
}

void EventHandler::updateTempo(const Tempo& newTempo)
{
	this->tempo = newTempo;
	return;
}
