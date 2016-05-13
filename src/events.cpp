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

bool TempoEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler) const
{
	return handler->handleEvent(delay, trackIndex, patternIndex, this);
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

bool NoteOnEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler) const
{
	return handler->handleEvent(delay, trackIndex, patternIndex, this);
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

bool NoteOffEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler) const
{
	return handler->handleEvent(delay, trackIndex, patternIndex, this);
}

std::string EffectEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=effect;type=" << (int)this->type << ";data=" << this->data
	;
	return ss.str();
}

bool EffectEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler) const
{
	return handler->handleEvent(delay, trackIndex, patternIndex, this);
}

std::string GotoEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=goto;type=" << (int)this->type
		<< ";loop=" << this->loop
		<< ";targetOrder=" << this->targetOrder
		<< ";targetRow=" << this->targetRow
	;
	return ss.str();
}

bool GotoEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler) const
{
	return handler->handleEvent(delay, trackIndex, patternIndex, this);
}

std::string ConfigurationEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=config;type="
	;
	switch (this->configType) {
		case Type::EmptyEvent:
			ss << "none";
			break;
		case Type::EnableOPL3:
			ss << "enableOPL3";
			break;
		case Type::EnableDeepTremolo:
			ss << "enableDeepTremolo";
			break;
		case Type::EnableDeepVibrato:
			ss << "enableDeepVibrato";
			break;
		case Type::EnableRhythm:
			ss << "enableRhythm";
			break;
		case Type::EnableWaveSel:
			ss << "enableWaveSel";
			break;
	}
	ss << ";value=" << this->value;
	return ss.str();
}

bool ConfigurationEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler) const
{
	return handler->handleEvent(delay, trackIndex, patternIndex, this);
}
