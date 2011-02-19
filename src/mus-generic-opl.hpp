/**
 * @file   mus-generic-opl.hpp
 * @brief  MusicReader and MusicWriter classes to process raw OPL register data.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_GENERIC_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_GENERIC_OPL_HPP_

#include <deque>
#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/patchbank-opl.hpp>
#include <camoto/types.hpp>

namespace camoto {
namespace gamemusic {

/// Flag for delay type
enum DelayType {
	DelayIsPreData,  ///< The delay should occur before using the register value
	DelayIsPostData, ///< The delay should happen after the reg val is used
};

/// MusicReader class that understands Adlib/OPL register data.
class MusicReader_GenericOPL: virtual public MusicReader {

	protected:
		DelayType delayType;       ///< Location of the delay
		unsigned long lastTick;    ///< Time of last event
		uint8_t oplState[2][256];  ///< Current register values
		std::deque<EventPtr> eventBuffer; ///< List of events to return in readNextEvent()
		OPLPatchBankPtr patches;   ///< List of instruments in use
		double fnumConversion;     ///< Conversion value to use in fnum -> Hz calc

	public:

		MusicReader_GenericOPL(DelayType delayType)
			throw ();

		virtual ~MusicReader_GenericOPL()
			throw ();

		virtual PatchBankPtr getPatchBank()
			throw ();

		virtual EventPtr readNextEvent()
			throw (std::ios::failure);

		// MusicReader_GenericOPL functions

		/// Read some more bytes and add at least one event to the queue.
		/**
		 * @return true if %eventBuffer has at least one more event appended,
		 *   or false if the end of the song has been reached.
		 */
		bool populateEventBuffer()
			throw (std::ios::failure);

		/// Change the tempo of the song.
		/**
		 * This function will normally be called from within nextPair() when a
		 * format-specific command to change the song speed is encountered.
		 */
		virtual void changeSpeed(uint32_t usPerTick)
			throw ();

		/// Seek back to the start of the song data.
		/**
		 * @postcondition The next call to nextPair() will return the first
		 *   pair in the file.  This is the same point as when the object is first
		 *   instantiated.
		 */
		virtual void rewind()
			throw () = 0;

		/// Read the next register/value pair from the underlying format.
		/**
		 * This function must be implemented by a format-specific handler
		 * extending this class.
		 *
		 * @param  delay  The number of ticks to wait either before or after
		 *   sending the register values (depending on the value of %delayType
		 *   given in the constructor.
		 *
		 * @param  chipIndex  Which OPL chip to access (0 or 1)
		 *
		 * @param  reg  The OPL register to write to.
		 *
		 * @param  val  The value to write to the register.
		 */
		virtual bool nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg, uint8_t *val)
			throw (std::ios::failure) = 0;

	private:

		/// Extract an OPLPatch from the cached OPL register settings.
		OPLPatchPtr getCurrentPatch(int chipIndex, int oplChannel)
			throw ();

		void savePatch(OPLPatchPtr curPatch)
			throw ();

		EventPtr createNoteOn(uint8_t chipIndex, uint8_t oplChannel, int rhythm,
			int channel, int b0val)
			throw ();

		EventPtr createNoteOff(int channel)
			throw ();

		/// Write a pitchbend event or update an existing one.
		/**
		 * This function adds a pitchbend event to eventBuffer using the values
		 * given by a0val and b0val.  If a pitchbend event is already in the event
		 * buffer, it is updated instead of adding a new one.
		 */
		void createOrUpdatePitchbend(uint8_t chipIndex, int channel, int a0val,
			int b0val)
			throw ();

		/// Convert the given f-number and block into a note frequency.
		int fnumToMilliHertz(int fnum, int block)
			throw ();

};

/// MusicWriter class that can produce Adlib/OPL register/value pairs.
class MusicWriter_GenericOPL: virtual public MusicWriter {

	protected:
		DelayType delayType;       ///< Location of the delay
		unsigned long lastTick;    ///< Time of last event
		unsigned long cachedDelay; ///< Delay to add on to next reg write
		uint8_t oplState[2][256];  ///< Current register values
		OPLPatchBankPtr inst;      ///< Last set patch bank
		uint8_t delayedReg;        ///< Register to delay-write to
		uint8_t delayedVal;        ///< Value to delay-write with
		bool firstPair;            ///< Is this the first pair? (if true, delayedReg/Val are invalid)
		double fnumConversion;     ///< Conversion value to use in Hz -> fnum calc

	public:
		MusicWriter_GenericOPL(DelayType delayType)
			throw ();

		virtual ~MusicWriter_GenericOPL()
			throw ();

		virtual void setPatchBank(const PatchBankPtr& instruments)
			throw (EBadPatchType);

		/// Finish writing to the output stream.
		/**
		 * If overriding this function, before to call this version of it first!
		 */
		virtual void finish()
			throw (std::ios::failure);

		virtual void handleEvent(TempoEvent *ev)
			throw (std::ios::failure);

		virtual void handleEvent(NoteOnEvent *ev)
			throw (std::ios::failure, EChannelMismatch);

		virtual void handleEvent(NoteOffEvent *ev)
			throw (std::ios::failure);

		virtual void handleEvent(PitchbendEvent *ev)
			throw (std::ios::failure);

		// MusicReader_GenericOPL functions

		/// Change the tempo
		/**
		 * This function must be implemented by a format-specific handler
		 * extending this class.
		 */
		virtual void changeSpeed(uint32_t usPerTick)
			throw () = 0;

		/// Write the next register/value pair to the underlying format.
		/**
		 * This function must be implemented by a format-specific handler
		 * extending this class.
		 *
		 * @param  delay  the number of ticks to wait either before or after
		 *   processing reg/val, depending on the value of delayType.
		 */
		virtual void nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
			throw (std::ios::failure) = 0;

	private:
		/// Update oplState then call nextPair()
		/**
		 * @param  delay  the number of ticks to wait *before* processing reg/val
		 */
		void writeNextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
			throw (std::ios::failure);

		/// Write one operator's patch settings (modulator or carrier)
		/**
		 * @param  opNum  0 for modulator, 1 for carrier
		 */
		void writeOpSettings(int chipIndex, int oplChannel, int opNum,
			OPLPatchPtr i, int velocity)
			throw (std::ios::failure);

		/// Convert a frequency into an OPL f-number
		void milliHertzToFnum(int milliHertz, int *fnum, int *block)
			throw ();

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_GENERIC_OPL_HPP_
