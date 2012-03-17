/**
 * @file   encode-opl.cpp
 * @brief  Function to convert a Music instance into raw OPL data.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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

#include "encode-opl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

class OPLEncoder: virtual private OPLWriterCallback
{
	public:
		/// Set encoding parameters.
		/**
		 * @param cb
		 *   Callback to handle generated OPL data.
		 *
		 * @param delayType
		 *   Where the delay is actioned - before its associated data pair is sent
		 *   to the OPL chip, or after.
		 *
		 * @param fnumConversion
		 *   Conversion constant to use when converting Hertz into OPL frequency
		 *   numbers.  Can be one of OPL_FNUM_* or a raw value.
		 *
		 * @param flags
		 *   One or more OPLWriteFlags to use to control the conversion.
		 */
		OPLEncoder(OPLWriterCallback *cb, DelayType delayType,
			double fnumConversion, unsigned int flags)
			throw ();

		/// Destructor.
		~OPLEncoder()
			throw ();

		/// Process the instruments and events, and write out OPL data.
		/**
		 * This function will call writeNextPair() repeatedly until all the events
		 * in the song have been written out.
		 *
		 * @param music
		 *   The instance to convert to OPL data.
		 *
		 * @throw stream:error
		 *   If the output data could not be written for some reason.
		 *
		 * @throw format_limitation
		 *   If the song could not be converted to OPL for some reason (e.g. it has
		 *   sampled instruments.)
		 */
		void encode(const MusicPtr music)
			throw (stream::error, format_limitation);

		// OPLWriterCallback

		virtual void writeNextPair(const OPLEvent *oplEvent)
			throw (stream::error);

		virtual void writeTempoChange(unsigned long usPerTick)
			throw (stream::error);

	private:
		OPLWriterCallback *cb;     ///< Callback to use when writing OPL data
		DelayType delayType;       ///< Location of the delay
		double fnumConversion;     ///< Conversion value to use in fnum -> Hz calc
		unsigned int flags;        ///< One or more OPLWriteFlags

		unsigned long lastTempo;   ///< Last tempo value (to avoid duplicate events)
		uint8_t oplState[2][256];  ///< Current register values
};


void camoto::gamemusic::oplEncode(OPLWriterCallback *cb, DelayType delayType,
	double fnumConversion, unsigned int flags, MusicPtr music)
	throw (stream::error, format_limitation)
{
	OPLEncoder encoder(cb, delayType, fnumConversion, flags);
	encoder.encode(music);
	return;
}

OPLEncoder::OPLEncoder(OPLWriterCallback *cb, DelayType delayType,
	double fnumConversion, unsigned int flags)
	throw ()
	: cb(cb),
	  delayType(delayType),
	  fnumConversion(fnumConversion),
	  flags(flags),
	  lastTempo(0)
{
	// Initialise all OPL registers to zero (this is assumed the initial state
	// upon playback)
	memset(this->oplState, 0, sizeof(this->oplState));
}

OPLEncoder::~OPLEncoder()
	throw ()
{
}

void OPLEncoder::encode(const MusicPtr music)
	throw (stream::error, format_limitation)
{
	EventConverter_OPL conv(this, this->delayType, this->fnumConversion,
		this->flags);
	try {
		conv.setPatchBank(music->patches);
		conv.handleAllEvents(music->events);
	} catch (const EBadPatchType& e) {
		throw format_limitation(std::string("Bad patch type: ") + e.what());
	}
	return;
}

void OPLEncoder::writeNextPair(const OPLEvent *oplEvent)
	throw (stream::error)
{
	/// @todo Optimise before calling writeNextPair()
	this->cb->writeNextPair(oplEvent);
	return;
}

void OPLEncoder::writeTempoChange(unsigned long usPerTick)
	throw (stream::error)
{
	if (usPerTick != this->lastTempo) {
		this->cb->writeTempoChange(usPerTick);
		this->lastTempo = usPerTick;
	}
	return;
}
