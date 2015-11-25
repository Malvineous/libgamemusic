/**
 * @file  encode-opl.cpp
 * @brief Function to convert a Music instance into raw OPL data.
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
		 * @param music
		 *   The instance to convert to OPL data.
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
		OPLEncoder(OPLWriterCallback *cb, const Music& music, DelayType delayType,
			double fnumConversion, OPLWriteFlags flags);

		/// Destructor.
		~OPLEncoder();

		/// Process the instruments and events, and write out OPL data.
		/**
		 * This function will call writeNextPair() repeatedly until all the events
		 * in the song have been written out.
		 *
		 * @throw stream:error
		 *   If the output data could not be written for some reason.
		 *
		 * @throw format_limitation
		 *   If the song could not be converted to OPL for some reason (e.g. it has
		 *   sampled instruments.)
		 */
		void encode();

		// OPLWriterCallback
		virtual void writeNextPair(const OPLEvent *oplEvent);

	private:
		OPLWriterCallback *cb;     ///< Callback to use when writing OPL data
		const Music& music;        ///< Song to convert
		DelayType delayType;       ///< Location of the delay
		double fnumConversion;     ///< Conversion value to use in fnum -> Hz calc
		OPLWriteFlags flags;           ///< One or more OPLWriteFlags
		Tempo lastTempo;           ///< Last tempo supplied
		uint8_t lastChipIndex;     ///< Last chip index used (only valid if lastReg != 0)
		uint8_t lastReg;           ///< Last OPL register written to
		uint8_t lastVal;           ///< Last value written to register (only valid if lastReg != 0)
};


void camoto::gamemusic::oplEncode(OPLWriterCallback *cb, const Music& music,
	DelayType delayType, double fnumConversion, OPLWriteFlags flags)
{
	OPLEncoder encoder(cb, music, delayType, fnumConversion, flags);
	encoder.encode();
	return;
}


OPLEncoder::OPLEncoder(OPLWriterCallback *cb, const Music& music,
	DelayType delayType, double fnumConversion, OPLWriteFlags flags)
	:	cb(cb),
		music(music),
		delayType(delayType),
		fnumConversion(fnumConversion),
		flags(flags),
		lastTempo(music.initialTempo),
		lastReg(0)
{
}

OPLEncoder::~OPLEncoder()
{
}

void OPLEncoder::encode()
{
	// Create a dummy shared_ptr to pass to EventConverter_OPL, which takes the
	// reference to the Music instance and converts it into a fake smart_ptr
	// (which will never try to delete the reference.)
	// This is safe because the EventConverter_MIDI instance holding this fake
	// smart_ptr does not outlive the current function.
	std::shared_ptr<const Music> music_ptr(&this->music, [](const Music *p){ return; });

	EventConverter_OPL conv(this, music_ptr, this->fnumConversion, this->flags);
	conv.handleAllEvents(EventHandler::Order_Row_Track);

	if ((this->delayType == DelayType::DelayIsPostData) && this->lastReg) {
		// Flush out the last event
		OPLEvent out;
		out.valid = OPLEvent::Regs;
		out.chipIndex = this->lastChipIndex;
		out.reg = this->lastReg;
		out.val = this->lastVal;
		out.tempo = this->lastTempo;
		this->cb->writeNextPair(&out);
	}
	return;
}

void OPLEncoder::writeNextPair(const OPLEvent *oplEvent)
{
	// There's nothing technically wrong with this, but it typically indicates a
	// bug so we'll fail if it happens to assist with debugging.
	assert(oplEvent->valid != 0);

	OPLEvent out;
	out.valid = 0;

	if (oplEvent->valid & OPLEvent::Tempo) {
		out.valid |= OPLEvent::Tempo;
		this->lastTempo = oplEvent->tempo;
	}
	out.tempo = this->lastTempo;

	if (oplEvent->valid & OPLEvent::Delay) {
		out.valid |= OPLEvent::Delay;
		out.delay = oplEvent->delay;
	}

	if (this->delayType == DelayType::DelayIsPreData) {
		if (oplEvent->valid & OPLEvent::Regs) {
			assert(oplEvent->chipIndex < 2);

			out.valid |= OPLEvent::Regs;
			out.chipIndex = oplEvent->chipIndex;
			out.reg = oplEvent->reg;
			out.val = oplEvent->val;
		}

	} else { // DelayType::DelayIsPostData
		if (this->lastReg) {
			out.valid |= OPLEvent::Regs;
			out.chipIndex = this->lastChipIndex;
			out.reg = this->lastReg;
			out.val = this->lastVal;
			this->lastReg = 0;
		}
		if (oplEvent->valid & OPLEvent::Regs) {
			this->lastChipIndex = oplEvent->chipIndex;
			this->lastReg = oplEvent->reg;
			this->lastVal = oplEvent->val;
		}
	}

	if (out.valid) {
		this->cb->writeNextPair(&out);
	}
	return;
}
