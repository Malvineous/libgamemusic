/**
 * @file   events.hpp
 * @brief  Declaration of all the Event types.
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

#ifndef _CAMOTO_GAMEARCHIVE_EVENTS_HPP_
#define _CAMOTO_GAMEARCHIVE_EVENTS_HPP_

#include <boost/shared_ptr.hpp>
#include <exception>
#include <vector>

#include <camoto/types.hpp>

namespace camoto {
namespace gamemusic {

class EventHandler;

/// Base class to represent events in the file.
/**
 * Will be extended by descendent classes to hold format-specific data.
 * The entries here will be valid for all music types.
 */
struct Event {

	/// The number of ticks (since the start of the song) when this event should
	/// be actioned
	uint32_t absTime;

	/// Channel number (1+, channel 0 means a global event affecting all channels)
	int channel;

	/// Helper function (for debugging) to return all the data as a string
	virtual std::string getContent() const
		throw ();

	/// Call the handleEvent() function in an EventHandler class, passing this
	/// event as the parameter.
	virtual void processEvent(EventHandler *handler)
		throw (std::ios::failure) = 0;
};

/// Shared pointer to an event
typedef boost::shared_ptr<Event> EventPtr;

/// Vector of shared event pointers
typedef std::vector<EventPtr> VC_EVENTPTR;


/// Changing the tempo changes the rate at which the ticks tick.
/**
 * Remember that since the tick length can vary, you cannot calculate the
 * absolute time of a note by multiplying the tick length by Event.absTime,
 * as this does not take into account any tempo changes in the middle of the
 * song.
 */
struct TempoEvent: virtual public Event
{
	/// Number of microseconds per tick.
	int usPerTick;

	virtual std::string getContent() const
		throw ();

	virtual void processEvent(EventHandler *handler)
		throw (std::ios::failure);
};


/// A single note is now playing on this channel.
/**
 * @note Only one note can be playing on each channel at a time.  For formats
 *       which allow multiple notes on each channel (e.g. MIDI) some "virtual"
 *       channels will have to be created.
 */
struct NoteOnEvent: virtual public Event
{
	/// Instrument to play this note with.  This is an index into the vector
	/// returned by Music::getInstruments().
	int instrument;

	/// Note frequency (44000 == 440Hz)
	int centiHertz;

	/// Velocity.  0 == unused/default, 1 == quiet, 255 = loud
	uint8_t velocity;

	virtual std::string getContent() const
		throw ();

	virtual void processEvent(EventHandler *handler)
		throw (std::ios::failure);
};


/// All notes on this channel are now silenced.
struct NoteOffEvent: virtual public Event
{
	virtual std::string getContent() const
		throw ();

	virtual void processEvent(EventHandler *handler)
		throw (std::ios::failure);
};


/// Change the pitch of the note on this channel.
/**
 * @note Only one note can be playing on each channel at a time.  For formats
 *       which allow multiple notes on each channel (e.g. MIDI) some "virtual"
 *       channels will have to be created.
 */
struct PitchbendEvent: virtual public Event
{
	/// Note frequency (4400 == 440Hz)
	int centiHertz;

	virtual std::string getContent() const
		throw ();

	virtual void processEvent(EventHandler *handler)
		throw (std::ios::failure);
};

/// Exception thrown when playing a note on an invalid channel
/**
 * Certain formats have limits on which types of instruments can be played on
 * which channels (e.g. OPL rhythm mode instruments must be played on special
 * rhythm channels.)  This exception is thrown when a mismatch occurs.  The
 * what() function will return a reason suitable for display to the user.
 */
class EChannelMismatch: virtual public std::exception {
	protected:
		int instIndex;      ///< Instrument ID (starting at 0)
		int targetChannel;  ///< Target channel (starting at 0, but +1 for messages)
		std::string reason; ///< User-readable reason for mismatch

		/// Message displayed to use.
		/**
		 * This is declared mutable as it will be populated by the first call to
		 * the const function what(), then the cached value will be returned on
		 * subsequent calls.
		 */
		mutable std::string msg;

	public:
		EChannelMismatch(int instIndex, int targetChannel, const std::string& reason)
			throw ();

		~EChannelMismatch()
			throw ();

		virtual const char *what() const
			throw ();
};

/// Callback interface
/**
 * Any class implementing this interface will be able to process different
 * events in typed functions, removing the need to dynamic_cast to figure
 * out the event type.
 */
class EventHandler
{
	public:
		virtual void handleEvent(TempoEvent *ev)
			throw (std::ios::failure) = 0;

		virtual void handleEvent(NoteOnEvent *ev)
			throw (std::ios::failure, EChannelMismatch) = 0;

		virtual void handleEvent(NoteOffEvent *ev)
			throw (std::ios::failure) = 0;

		virtual void handleEvent(PitchbendEvent *ev)
			throw (std::ios::failure) = 0;
};


} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEARCHIVE_EVENTS_HPP_
