/**
 * @file  events.cpp
 * @brief Implementation of all Event types.
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

#include <sstream>
#include <list>
#include <camoto/gamemusic/events.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>

using namespace camoto::gamemusic;

std::string Event::getContent() const
{
	return std::string();
}

std::string TempoEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=tempo;us_per_tick=" << this->tempo.usPerTick
		<< ";frames_per_tick=" << this->tempo.framesPerTick
		<< ";ticks_per_beat=" << this->tempo.ticksPerBeat
		<< ";beat_length=" << this->tempo.beatLength
		<< ";beats_per_bar=" << this->tempo.beatsPerBar
	;
	return ss.str();
}

void TempoEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string NoteOnEvent::getContent() const
{
	double semitone = freqToMIDI(this->milliHertz);
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=note-on;freq=" << this->milliHertz
		<< ";freq_midi=" << semitone
		<< ";freq_oct=" << ((int)semitone / 12)
		<< ";instrument=" << this->instrument
		<< ";velocity=" << (int)this->velocity
	;
	return ss.str();
}

void NoteOnEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string NoteOffEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=note-off"
	;
	return ss.str();
}

void NoteOffEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string EffectEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=effect;type=" << this->type << ";data=" << this->data
	;
	return ss.str();
}

void EffectEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string GotoEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=goto;type=" << this->type
		<< ";loop=" << this->loop
		<< ";targetOrder=" << this->targetOrder
		<< ";targetRow=" << this->targetRow
	;
	return ss.str();
}

void GotoEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string ConfigurationEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=config;type="
	;
	switch (this->configType) {
		case EmptyEvent:
			ss << "none";
			break;
		case EnableOPL3:
			ss << "enableOPL3";
			break;
		case EnableDeepTremolo:
			ss << "enableDeepTremolo";
			break;
		case EnableDeepVibrato:
			ss << "enableDeepVibrato";
			break;
		case EnableRhythm:
			ss << "enableRhythm";
			break;
		case EnableWaveSel:
			ss << "enableWaveSel";
			break;
	}
	ss << ";value=" << this->value;
	return ss.str();
}

void ConfigurationEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

struct MergedEvent
{
	MergedEvent(const EventPtr& ev)
		:	event(ev)
	{
	}
	unsigned long absTime;
	unsigned int trackIndex;
	const EventPtr event;
};

bool trackMergeByTime(const MergedEvent& a, const MergedEvent& b)
{
	if (a.absTime == b.absTime) {
		// These events occur at the same time, so try to put all the note-offs
		// first, to minimise any unnecessary note polyphony
		NoteOffEvent *a_off = dynamic_cast<NoteOffEvent *>(a.event.get());
		NoteOffEvent *b_off = dynamic_cast<NoteOffEvent *>(b.event.get());
		if (a_off && b_off) return false;
		return a_off;
	}
	return a.absTime < b.absTime;
}

void EventHandler::handleAllEvents(EventHandler::EventOrder eventOrder,
	ConstMusicPtr music)
{
	switch (eventOrder) {
		case Pattern_Row_Track: {
			unsigned int patternIndex = 0;
			// For each pattern
			for (std::vector<PatternPtr>::const_iterator
				pp = music->patterns.begin(); pp != music->patterns.end(); pp++, patternIndex++
			) {
				this->processPattern_mergeTracks(music, *pp, patternIndex);
			}
			break;
		}

		case Pattern_Track_Row: {
			unsigned int patternIndex = 0;
			// For each pattern
			for (std::vector<PatternPtr>::const_iterator
				pp = music->patterns.begin(); pp != music->patterns.end(); pp++, patternIndex++
			) {
				this->processPattern_separateTracks(music, *pp, patternIndex);
			}
			break;
		}

		case Order_Row_Track: {
			unsigned int orderIndex = 0;
			// For each pattern
			for (std::vector<unsigned int>::const_iterator
				o = music->patternOrder.begin(); o != music->patternOrder.end(); o++, orderIndex++
			) {
				const unsigned int& patternIndex = *o;
				const PatternPtr& pattern = music->patterns[patternIndex];

				this->processPattern_mergeTracks(music, pattern, patternIndex);
			}
			break;
		}

		case Order_Track_Row: {
			unsigned int orderIndex = 0;
			// For each pattern
			for (std::vector<unsigned int>::const_iterator
				o = music->patternOrder.begin(); o != music->patternOrder.end(); o++, orderIndex++
			) {
				const unsigned int& patternIndex = *o;
				const PatternPtr& pattern = music->patterns[patternIndex];

				this->processPattern_separateTracks(music, pattern, patternIndex);
			}
			break;
		}
	}
	return;
}

void EventHandler::processPattern_mergeTracks(const ConstMusicPtr& music,
	const PatternPtr& pattern, unsigned int patternIndex)
{
	// Merge all the tracks together into one big track, with all events in
	// chronological order
	std::list<MergedEvent> full;

	unsigned int trackIndex = 0;
	// For each track
	for (Pattern::const_iterator
		pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++
	) {
		std::list<MergedEvent> ltrack;

		unsigned long trackTime = 0;
		// For each event in the track
		for (Track::const_iterator
			ev = (*pt)->begin(); ev != (*pt)->end(); ev++
		) {
			const TrackEvent& te = *ev;
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
	}

	unsigned long trackTime = 0;
	// Now run through the master list, which has all events sorted
	for (std::list<MergedEvent>::const_iterator
		fev = full.begin(); fev != full.end(); fev++
	) {
		const MergedEvent& me = *fev;
		unsigned long deltaTime = me.absTime - trackTime;
		trackTime = me.absTime;
		me.event->processEvent(deltaTime, me.trackIndex, patternIndex, this);
	}
	assert(trackTime <= music->ticksPerTrack);
	this->endOfPattern(music->ticksPerTrack - trackTime);
	return;
}

void EventHandler::processPattern_separateTracks(const ConstMusicPtr& music,
	const PatternPtr& pattern, unsigned int patternIndex)
{
	unsigned int trackIndex = 0;
	unsigned long maxTrackTime = 0;
	// For each track
	for (Pattern::const_iterator
		pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++
	) {
		unsigned long trackTime = 0;
		// For each event in the track
		for (Track::const_iterator
			ev = (*pt)->begin(); ev != (*pt)->end(); ev++
		) {
			const TrackEvent& te = *ev;
			trackTime += te.delay;
			te.event->processEvent(te.delay, trackIndex, patternIndex, this);
		}
		if (trackTime > maxTrackTime) maxTrackTime = trackTime;
		this->endOfTrack(music->ticksPerTrack - trackTime);
	}
	this->endOfPattern(music->ticksPerTrack - maxTrackTime);
	return;
}
