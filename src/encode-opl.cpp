/**
 * @file   encode-opl.cpp
 * @brief  Function to convert a Music instance into raw OPL data.
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
			double fnumConversion, unsigned int flags);

		/// Destructor.
		~OPLEncoder();

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
		void encode(const MusicPtr music);

		// OPLWriterCallback

		virtual void writeNextPair(const OPLEvent *oplEvent);

		virtual void writeTempoChange(tempo_t usPerTick);

	private:
		OPLWriterCallback *cb;     ///< Callback to use when writing OPL data
		DelayType delayType;       ///< Location of the delay
		double fnumConversion;     ///< Conversion value to use in fnum -> Hz calc
		unsigned int flags;        ///< One or more OPLWriteFlags

		unsigned long lastTempo;   ///< Last tempo value (to avoid duplicate events)
		bool firstPair;            ///< Is the next pair the first we have written?
		uint8_t oplState[2][256];  ///< Current register values
		OPLEvent delayedEvent;     ///< Delayed event data
};


void camoto::gamemusic::oplEncode(OPLWriterCallback *cb, DelayType delayType,
	double fnumConversion, unsigned int flags, MusicPtr music)
{
	OPLEncoder encoder(cb, delayType, fnumConversion, flags);
	encoder.encode(music);
	return;
}

OPLEncoder::OPLEncoder(OPLWriterCallback *cb, DelayType delayType,
	double fnumConversion, unsigned int flags)
	:	cb(cb),
		delayType(delayType),
		fnumConversion(fnumConversion),
		flags(flags),
		lastTempo(0),
		firstPair(true)
{
	// Initialise all OPL registers to zero (this is assumed the initial state
	// upon playback)
	memset(this->oplState, 0, sizeof(this->oplState));
	this->delayedEvent.delay = 0;
}

OPLEncoder::~OPLEncoder()
{
}

void OPLEncoder::encode(const MusicPtr music)
{
	PatchBankPtr oplPatches = boost::dynamic_pointer_cast<PatchBank>(music->patches);
	if (!oplPatches) {
		throw format_limitation("This format can only accept OPL patches.");
	}
	EventConverter_OPL conv(this, oplPatches, this->fnumConversion,
		this->flags);
	try {
		conv.handleAllEvents(music->events);
	} catch (const bad_patch& e) {
		throw format_limitation(std::string("Bad patch type: ") + e.what());
	}
	if (this->delayType == DelayIsPostData) {
		// Flush out the last event
		this->delayedEvent.delay = 0; // cut off any EOF silence
		this->cb->writeNextPair(&this->delayedEvent);
	}
	return;
}

void OPLEncoder::writeNextPair(const OPLEvent *oplEvent)
{
	// TODO: Cache all the events with no delay then optimise them before
	// writing them out (once a delay is encountered.)  This should allow multiple
	// 0xBD register writes to be combined into one, for situations where multiple
	// perc instruments play at the same instant.
	if (this->oplState[oplEvent->chipIndex][oplEvent->reg] != oplEvent->val) {
		if (this->delayType == DelayIsPreData) {
			OPLEvent c = *oplEvent;
			c.delay += this->delayedEvent.delay;
			this->delayedEvent.delay = 0;
			this->cb->writeNextPair(&c);
		} else { // DelayIsPostData
			// The delay we've cached should happen before the next data bytes are
			// written.  But this is a DelayIsPostData format, which means the delay
			// value we pass to nextPair() will happen *after* the data bytes.  So
			// instead we have to now write the previous set of bytes, with the cached
			// delay, and then remember the current bytes to write out later!
			if (!this->firstPair) {
				this->delayedEvent.delay += oplEvent->delay;
				this->cb->writeNextPair(&this->delayedEvent);
				//this->nextPair(this->cachedDelay + delay, chipIndex,
				//	this->delayedReg, this->delayedVal);
			} else {
				// This is the first data pair, so there are no delayed values to write,
				// we just cache the new values.  We will lose any delay before the
				// first note starts, but that should be fine...if you want silence
				// at the start of a song then wait longer before pressing play :-)
				this->firstPair = false;
			}
			this->delayedEvent = *oplEvent;
			this->delayedEvent.delay = 0; // this will also cut off any initial silence
		}
		this->oplState[oplEvent->chipIndex][oplEvent->reg] = oplEvent->val;
	} else {
		// Value was already set, don't write redundant data, but remember the
		// delay so it can be added to the next write.
		this->delayedEvent.delay += oplEvent->delay;
	}
	return;
}

void OPLEncoder::writeTempoChange(tempo_t usPerTick)
{
	if (usPerTick != this->lastTempo) {
		this->cb->writeTempoChange(usPerTick);
		this->lastTempo = usPerTick;
	}
	return;
}
