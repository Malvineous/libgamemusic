/**
 * @file   events.cpp
 * @brief  Implementation of all Event types.
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

#include <sstream>
#include <camoto/gamemusic/events.hpp>

using namespace camoto::gamemusic;

std::string Event::getContent() const
	throw ()
{
	std::ostringstream ss;
	ss
		<< ";channel=" << this->channel
		<< ";time=" << this->absTime
	;
	return ss.str();
}

std::string TempoEvent::getContent() const
	throw ()
{
	std::ostringstream ss;
	ss
		<< "event=tempo;us_per_tick=" << this->usPerTick
		<< this->Event::getContent()
	;
	return ss.str();
}

void TempoEvent::processEvent(EventHandler *handler)
	throw (std::ios::failure)
{
	handler->handleEvent(this);
	return;
}

std::string NoteOnEvent::getContent() const
	throw ()
{
	std::ostringstream ss;
	ss
		<< "event=note-on;freq=" << this->centiHertz
		<< ";instrument=" << this->instrument
		<< ";velocity=" << (int)this->velocity
		<< this->Event::getContent()
	;
	return ss.str();
}

void NoteOnEvent::processEvent(EventHandler *handler)
	throw (std::ios::failure)
{
	handler->handleEvent(this);
	return;
}

std::string NoteOffEvent::getContent() const
	throw ()
{
	std::ostringstream ss;
	ss
		<< "event=note-off"
		<< this->Event::getContent()
	;
	return ss.str();
}

void NoteOffEvent::processEvent(EventHandler *handler)
	throw (std::ios::failure)
{
	handler->handleEvent(this);
	return;
}

std::string PitchbendEvent::getContent() const
	throw ()
{
	std::ostringstream ss;
	ss
		<< "event=pitchbend;freq=" << this->centiHertz
		<< this->Event::getContent()
	;
	return ss.str();
}

void PitchbendEvent::processEvent(EventHandler *handler)
	throw (std::ios::failure)
{
	handler->handleEvent(this);
	return;
}
