/**
 * @file   events.cpp
 * @brief  Implementation of all Event types.
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

#include <sstream>
#include <camoto/gamemusic/events.hpp>

using namespace camoto::gamemusic;

std::string Event::getContent() const
{
	std::ostringstream ss;
	ss
		<< "time=" << this->absTime
		<< ";channel=" << this->channel
	;
	return ss.str();
}

std::string TempoEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< ";event=tempo;us_per_tick=" << this->usPerTick
	;
	return ss.str();
}

void TempoEvent::processEvent(EventHandler *handler)
{
	handler->handleEvent(this);
	return;
}

std::string NoteOnEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< ";event=note-on;freq=" << this->milliHertz
		<< ";instrument=" << this->instrument
		<< ";velocity=" << (int)this->velocity
	;
	return ss.str();
}

void NoteOnEvent::processEvent(EventHandler *handler)
{
	handler->handleEvent(this);
	return;
}

std::string NoteOffEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< ";event=note-off"
	;
	return ss.str();
}

void NoteOffEvent::processEvent(EventHandler *handler)
{
	handler->handleEvent(this);
	return;
}

std::string PitchbendEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< ";event=pitchbend;freq=" << this->milliHertz
	;
	return ss.str();
}

void PitchbendEvent::processEvent(EventHandler *handler)
{
	handler->handleEvent(this);
	return;
}

std::string ConfigurationEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< ";event=config;type="
	;
	switch (this->configType) {
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

void ConfigurationEvent::processEvent(EventHandler *handler)
{
	handler->handleEvent(this);
	return;
}

void EventHandler::handleAllEvents(const EventVectorPtr& events)
{
	for (EventVector::const_iterator i = events->begin(); i != events->end(); i++) {
		i->get()->processEvent(this);
	}
	return;
}
