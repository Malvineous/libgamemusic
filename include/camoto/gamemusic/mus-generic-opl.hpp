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
#include <camoto/stream.hpp>
#include <stdint.h>

namespace camoto {
namespace gamemusic {

#define OPLBIT_KEYON    0x20 ///< Bit in keyon/freq register for turning a note on

/// Supplied with a channel, return the offset from a base OPL register for the
/// Modulator cell (e.g. channel 4's modulator is at offset 0x09.  Since 0x60 is
/// the attack/decay function, register 0x69 will thus set the attack/decay for
/// channel 4's modulator.)  Channels go from 0 to 8 inclusive.
#define OPLOFFSET_MOD(channel)   (((channel) / 3) * 8 + ((channel) % 3))
#define OPLOFFSET_CAR(channel)   (OPLOFFSET_MOD(channel) + 3)

/// Supplied with an operator offset, return the OPL channel it is associated
/// with (0-8).  Note that this only works in 2-operator mode, the OPL3's 4-op
/// mode requires a different formula.
#define OPL_OFF2CHANNEL(off)   (((off) % 8 % 3) + ((off) / 8 * 3))

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
			throw (stream::error);

		// MusicReader_GenericOPL functions

		/// Read some more bytes and add at least one event to the queue.
		/**
		 * @return true if %eventBuffer has at least one more event appended,
		 *   or false if the end of the song has been reached.
		 */
		bool populateEventBuffer()
			throw (stream::error);

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
			throw (stream::error) = 0;

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
			throw (stream::error);

		virtual void handleEvent(TempoEvent *ev)
			throw (stream::error);

		virtual void handleEvent(NoteOnEvent *ev)
			throw (stream::error, EChannelMismatch, EBadPatchType);

		virtual void handleEvent(NoteOffEvent *ev)
			throw (stream::error);

		virtual void handleEvent(PitchbendEvent *ev)
			throw (stream::error);

		virtual void handleEvent(ConfigurationEvent *ev)
			throw (stream::error);

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
			throw (stream::error) = 0;

	private:
		/// Update oplState then call nextPair()
		/**
		 * @param  delay  the number of ticks to wait *before* processing reg/val
		 */
		void writeNextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
			throw (stream::error);

		/// Write one operator's patch settings (modulator or carrier)
		/**
		 * @param  opNum  0 for modulator, 1 for carrier
		 */
		void writeOpSettings(int chipIndex, int oplChannel, int opNum,
			OPLPatchPtr i, int velocity)
			throw (stream::error);

};

/// Convert the given f-number and block into a note frequency.
/**
 * @param fnum
 *   Input frequency number, between 0 and 1023 inclusive.  Values outside this
 *   range will cause assertion failures.
 *
 * @param block
 *   Input block number, between 0 and 7 inclusive.  Values outside this range
 *   will cause assertion failures.
 *
 * @param conversionFactor
 *   Conversion factor to use.  Normally will be 49716 and occasionally 50000.
 *
 * @return The converted frequency in milliHertz.
 */
int fnumToMilliHertz(unsigned int fnum, unsigned int block, unsigned int conversionFactor)
	throw ();

/// Convert a frequency into an OPL f-number
/**
 * @param milliHertz
 *   Input frequency.
 *
 * @param fnum
 *   Output frequency number for OPL chip.  This is a 10-bit number, so it will
 *   always be between 0 and 1023 inclusive.
 *
 * @param block
 *   Output block number for OPL chip.  This is a 3-bit number, so it will
 *   always be between 0 and 7 inclusive.
 *
 * @param conversionFactor
 *   Conversion factor to use.  Normally will be 49716 and occasionally 50000.
 *
 * @post fnum will be set to a value between 0 and 1023 inclusive.  block will
 *   be set to a value between 0 and 7 inclusive.  assert() calls inside this
 *   function ensure this will always be the case.
 *
 * @note As the block value increases, the frequency difference between two
 *   adjacent fnum values increases.  This means the higher the frequency,
 *   the less precision is available to represent it.  Therefore, converting
 *   a value to fnum/block and back to milliHertz is not guaranteed to reproduce
 *   the original value.
 */
void milliHertzToFnum(unsigned int milliHertz, unsigned int *fnum, unsigned int *block, unsigned int conversionFactor)
	throw ();


} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_GENERIC_OPL_HPP_
