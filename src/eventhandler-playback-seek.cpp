/**
 * @file  eventhandler-playback-seek.cpp
 * @brief EventHandler implementation that can be used to seek by time.
 *
 * Copyright (C) 2010-2016 Adam Nielsen <malvineous@shikadi.net>
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

#include "eventhandler-playback-seek.hpp"

using namespace camoto::gamemusic;

EventHandler_Playback_Seek::EventHandler_Playback_Seek(
	std::shared_ptr<const Music> music, unsigned long loopCount)
	: music(music),
	  loopCount(loopCount)
{
	if (loopCount == 0) loopCount++;
}

EventHandler_Playback_Seek::~EventHandler_Playback_Seek()
{
}

void EventHandler_Playback_Seek::rewind()
{
}

void EventHandler_Playback_Seek::endOfTrack(unsigned long delay)
{
	return;
}

void EventHandler_Playback_Seek::endOfPattern(unsigned long delay)
{
	this->usTotal += delay * this->usPerTick;
	return;
}

bool EventHandler_Playback_Seek::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->updateTempo(ev->tempo);
	this->usTotal += delay * this->usPerTick;
	this->usPerTick = ev->tempo.usPerTick;
	return this->usTotal < this->usTarget;
}


#include <iostream>
bool EventHandler_Playback_Seek::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	this->usTotal += delay * this->usPerTick;
	return this->usTotal < this->usTarget;
}

bool EventHandler_Playback_Seek::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->usTotal += delay * this->usPerTick;
	return this->usTotal < this->usTarget;
}

bool EventHandler_Playback_Seek::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	this->usTotal += delay * this->usPerTick;
	return this->usTotal < this->usTarget;
}

bool EventHandler_Playback_Seek::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const GotoEvent *ev)
{
	this->usTotal += delay * this->usPerTick;
	this->performGoto(ev);
	return this->usTotal < this->usTarget;
}

bool EventHandler_Playback_Seek::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const ConfigurationEvent *ev)
{
	this->usTotal += delay * this->usPerTick;
	return this->usTotal < this->usTarget;
}

unsigned long EventHandler_Playback_Seek::getTotalLength()
{
	this->usTarget = (unsigned long)-1;
	this->usTotal = 0;
	this->usPerTick = this->music->initialTempo.usPerTick;
	this->handleAllEvents(EventOrder::Order_Row_Track, *this->music,
		this->loopCount);
	return this->usTotal / 1000;
}

EventHandler::Position EventHandler_Playback_Seek::seekTo(unsigned long msTarget)
{
	this->usTarget = msTarget * 1000;
	this->usTotal = 0;
	this->usPerTick = this->music->initialTempo.usPerTick;
	return this->handleAllEvents(EventOrder::Order_Row_Track, *this->music,
		this->loopCount);
}
