/**
 * @file   mus-got.cpp
 * @brief  Support for God of Thunder song files.
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

#include <camoto/iostream_helpers.hpp>
#include "decode-opl.hpp"
#include "encode-opl.hpp"
#include "mus-got.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define HERTZ_TO_uS(x) (1000000 / (x))

/// Decode data in an .imf file to provide register/value pairs.
class OPLReaderCallback_GOT: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_GOT(stream::input_sptr input, unsigned int speed)
			:	input(input),
				first(true),
				speed(speed)
		{
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			if (this->lenData == 0) return false;
			if (this->first) {
				// First call, set tempo
				this->first = false;
				oplEvent->tempo = HERTZ_TO_uS(this->speed);
				oplEvent->delay = 0;
				oplEvent->reg = 0;
				oplEvent->val = 0;
				oplEvent->chipIndex = 0;
				return true; // empty event w/ initial tempo
			}

			try {
				oplEvent->chipIndex = 0; // IMF only supports one OPL2
				uint8_t delay;
				this->input
					>> u8(delay)
					>> u8(oplEvent->reg)
					>> u8(oplEvent->val)
				;
				oplEvent->delay = delay;
				if ((oplEvent->delay == 0) && (oplEvent->reg == 0) && (oplEvent->val == 0)) {
					// End of song
					return false;
				}
			} catch (const stream::incomplete_read&) {
				return false;
			}
			return true;
		}

	protected:
		stream::input_sptr input;   ///< Input file
		bool first;                 ///< Is this the first time readNextPair() has been called?
		unsigned int speed;         ///< IMF clock rate
		unsigned long lenData;      ///< Length of song data (where song stops and tags start)
};


/// Encode OPL register/value pairs into .imf file data.
class OPLWriterCallback_GOT: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_GOT(stream::output_sptr output, unsigned int speed)
			:	output(output),
				speed(speed),
				usPerTick(HERTZ_TO_uS(speed)) // default to speed ticks per second
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			// Convert ticks into an IMF delay
			unsigned long delay = oplEvent->delay * this->usPerTick / (HERTZ_TO_uS(this->speed));
			// Write out super long delays as dummy events to an unused port.
			while (delay > 0xFF) {
				this->output
					<< u8(0xFF)
					<< u8(0x00)
					<< u8(0x00)
				;
				delay -= 0xFF;
			}

			/// @todo Put this as a format capability
			if (oplEvent->chipIndex != 0) {
				throw stream::error("God of Thunder music files are single-OPL2 only");
			}
			assert(oplEvent->chipIndex == 0);  // make sure the format capabilities are being honoured

			this->output
				<< u8(delay)
				<< u8(oplEvent->reg)
				<< u8(oplEvent->val)
			;
			return;
		}

		virtual void writeTempoChange(tempo_t usPerTick)
		{
			assert(usPerTick != 0);
			this->usPerTick = usPerTick;
			return;
		}

	protected:
		stream::output_sptr output; ///< Output file
		unsigned int speed;         ///< IMF clock rate
		tempo_t usPerTick;          ///< Latest microseconds per tick value (tempo)
};


std::string MusicType_GOT::getCode() const
{
	return "got";
}

std::string MusicType_GOT::getFriendlyName() const
{
	return "God of Thunder";
}

std::vector<std::string> MusicType_GOT::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	return vcExtensions;
}

MusicType::Certainty MusicType_GOT::isInstance(stream::input_sptr input) const
{
	stream::len len = input->size();

	// Must be enough room for header + footer
	// TESTED BY: mus_got_isinstance_c01
	if (len < 6) return DefinitelyNo;

	// Uneven size.
	// TESTED BY: mus_got_isinstance_c02
	if (len % 3 != 0) return DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_got_isinstance_c03
	uint16_t sig;
	input->seekg(0, stream::start);
	input >> u16le(sig);
	if (sig != 0x0001) return DefinitelyNo;

	// Make sure it ends with a loop-to-start marker
	// TESTED BY: mus_got_isinstance_c04
	uint32_t end;
	input->seekg(-4, stream::end);
	input >> u32le(end);
	if (end != 0) return DefinitelyNo;

	// TESTED BY: mus_got_isinstance_c00
	return PossiblyYes;
}

MusicPtr MusicType_GOT::read(stream::input_sptr input, SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	input->seekg(2, stream::start);

	OPLReaderCallback_GOT cb(input, 120);
	MusicPtr music = oplDecode(&cb, DelayIsPostData, OPL_FNUM_DEFAULT);

	return music;
}

void MusicType_GOT::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	output << u16le(1);

	// Call the generic OPL writer.
	OPLWriterCallback_GOT cb(output, 120);
	oplEncode(&cb, DelayIsPostData, OPL_FNUM_DEFAULT, flags, music);

	// Zero event (3 bytes) plus final 0x00
	output << nullPadded("", 4);

	// Set final filesize to this
	output->truncate_here();

	return;
}

SuppFilenames MusicType_GOT::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_GOT::getMetadataList() const
{
	Metadata::MetadataTypes types;
	return types;
}
