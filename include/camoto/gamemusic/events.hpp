/**
 * @file  camoto/gamemusic/events.hpp
 * @brief Declaration of all the Event types.
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

#ifndef _CAMOTO_GAMEMUSIC_EVENTS_HPP_
#define _CAMOTO_GAMEMUSIC_EVENTS_HPP_

#include <exception>
#include <memory>
#include <vector>
#include <stdint.h>
#include <camoto/error.hpp>
#include <camoto/stream.hpp>
#include <camoto/gamemusic/tempo.hpp>

namespace camoto {
namespace gamemusic {

class EventHandler;

/// All channel numbers in the %Event struct must be less than this value.
const unsigned int MAX_CHANNELS = 256;

/// Base class to represent events in the file.
/**
 * Will be extended by descendent classes to hold format-specific data.
 * The entries here will be valid for all music types.
 */
struct CAMOTO_GAMEMUSIC_API Event
{
	/// Helper function (for debugging) to return all the data as a string
	virtual std::string getContent() const;

	/// Call the handleEvent() function in an EventHandler class, passing this
	/// event as the parameter.
	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const = 0;
};

struct CAMOTO_GAMEMUSIC_API TrackEvent
{
	unsigned long delay;
	std::shared_ptr<Event> event;
};

/// Vector of shared event pointers
typedef std::vector<TrackEvent> Track;

/// Vector of tracks (a pattern)
typedef std::vector<Track> Pattern;

/// Changing the tempo changes the rate at which the ticks tick.
/**
 * Remember that since the tick length can vary, you cannot calculate the
 * absolute time of a note by multiplying the tick length by Event.absTime,
 * as this does not take into account any tempo changes in the middle of the
 * song.
 */
struct CAMOTO_GAMEMUSIC_API TempoEvent: virtual public Event
{
	/// New tempo.
	Tempo tempo;

	virtual std::string getContent() const;

	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const;
};


/// A single note is now playing on this channel.
/**
 * @note Only one note can be playing on each channel at a time.  For formats
 *       which allow multiple notes on each channel (e.g. MIDI) some "virtual"
 *       channels will have to be created.
 */
struct CAMOTO_GAMEMUSIC_API NoteOnEvent: virtual public Event
{
	virtual std::string getContent() const;

	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const;

	/// Instrument to play this note with.  This is an index into the vector
	/// returned by Music::getInstruments().
	unsigned int instrument;

	/// Note frequency (440000 == 440Hz)
	unsigned int milliHertz;

	/// Velocity: 0=silent, 255=loud, DefaultVelocity(-1)=default/unknown
	int velocity;
};
static const int DefaultVelocity = -1;


/// All notes on this channel are now silenced.
struct CAMOTO_GAMEMUSIC_API NoteOffEvent: virtual public Event
{
	virtual std::string getContent() const;

	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const;
};


/// Alter the way the note is currently being played on a channel.
struct CAMOTO_GAMEMUSIC_API EffectEvent: virtual public Event
{
	enum class Type {
		PitchbendNote, ///< Change note freq
		Volume,        ///< Change note volume
	};

	/// Type of effect.
	Type type;

	/// Effect data.
	/**
	 * - PitchbendNote: new frequency in milliHertz
	 * - Volume: new volume, 0=silent, 255=loud
	 */
	unsigned int data;

	virtual std::string getContent() const;

	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const;
};


/// Change the way playback is progressing.
struct CAMOTO_GAMEMUSIC_API GotoEvent: virtual public Event
{
	enum class Type {
		CurrentPattern, ///< Stay on the current pattern but change row
		NextPattern,    ///< Jump to the next pattern specified in the order list
		SpecificOrder,  ///< Jump to the given index in the order list
	};

	/// Type of jump.
	Type type;

	/// Number of times to repeat the jump (after the first encounter), if
	/// jumping backwards so this event gets reached again.
	/**
	 * - 0=jump once, no repeat (do nothing the second time the event is reached)
	 * - 1=jump twice (single repeat), ignore the event on and after the third hit
	 */
	unsigned int repeat;

	/// Target entry in the order list.
	/**
	 * @pre Only valid when \a type is #SpecificOrder.  Set to 0 otherwise.
	 */
	unsigned int targetOrder;

	/// Target row in destination order.
	/**
	 * 0 is the first row in the pattern.
	 */
	unsigned int targetRow;

	virtual std::string getContent() const;

	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const;
};


/// Configure the synthesiser's global parameters.
/**
 * This event can occur at any time.
 */
struct CAMOTO_GAMEMUSIC_API ConfigurationEvent: virtual public Event
{
	/// What can be configured.
	/**
	 * For boolean values (Enable*) a value of 0 is false/disabled and a value
	 * of nonzero (usually 1) is true/enabled.
	 */
	enum class Type
	{
		/// No operation
		/**
		 * Dummy event that doesn't do anything.  Can be placed last in a file if
		 * there is a trailing delay.
		 */
		EmptyEvent,

		/// Enable OPL3 mode (or limit to OPL2)
		/**
		 * value: 1 for OPL3 mode, 0 for OPL2 mode
		 */
		EnableOPL3,

		/// Extend range of OPL tremolo
		/**
		 * value: bit0 = 1 to enable, 0 to disable
		 *        bit1 = 0-1 as chip index
		 */
		EnableDeepTremolo,

		/// Extend range of OPL vibrato
		/**
		 * value: bit0 = 1 to enable, 0 to disable
		 *        bit1 = 0-1 as chip index
		 */
		EnableDeepVibrato,

		/// Enable OPL rhythm mode
		/**
		 * value: 1 to enable, 0 to disable
		 *
		 * This is used by the CMF handler and MIDI controller event 0x67.
		 */
		EnableRhythm,

		/// Enable use of wave selection registers
		/**
		 * value: 1 to enable, 0 to disable
		 */
		EnableWaveSel,
	};

	/// What we are configuring
	Type configType;

	/// What value we are setting (meaning dependent on config type)
	int value;

	virtual std::string getContent() const;

	virtual bool processEvent(unsigned long delay, unsigned int trackIndex,
		unsigned int patternIndex, EventHandler *handler) const;
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_EVENTS_HPP_
