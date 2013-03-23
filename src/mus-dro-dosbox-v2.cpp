/**
 * @file   mus-dro-dosbox-v2.cpp
 * @brief  Support for the second version of the DOSBox Raw OPL .DRO format.
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

#include <iostream>
#include <camoto/iostream_helpers.hpp>
#include <camoto/stream_string.hpp>
#include "decode-opl.hpp"
#include "encode-opl.hpp"
#include "metadata-malv.hpp"
#include "mus-dro-dosbox-v2.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Length of each .dro tick in microseconds
#define DRO_CLOCK  1000

/// Command byte we will use to write a short delay
#define DRO2_CMD_SHORTDELAY 0xFF

/// Command byte we will use to write a long delay
#define DRO2_CMD_LONGDELAY 0xFE

/// Header value for single OPL2 chip
#define DRO2_OPLTYPE_OPL2 0

/// Header value for two OPL2 chips
#define DRO2_OPLTYPE_DUALOPL2 1

/// Header value for single OPL3 chip
#define DRO2_OPLTYPE_OPL3 2

/// Decode data in a .dro file to provide register/value pairs.
class OPLReaderCallback_DRO_v2: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_DRO_v2(stream::input_sptr input)
			:	input(input),
				first(true)
		{
			this->input->seekg(12, stream::start);
			this->input >> u32le(this->lenData);
			this->input->seekg(6, stream::cur);
			uint8_t compression;
			this->input
				>> u8(compression)
				>> u8(this->codeShortDelay)
				>> u8(this->codeLongDelay)
				>> u8(this->codemapLength)
			;
			if (compression != 0) throw stream::error("Compressed DRO files are not implemented (didn't even know they existed)");
			if (this->codemapLength > 127) throw stream::error("DRO code map too large");
			memset(this->codemap, 0xFF, sizeof(this->codemap));
			this->input->read(this->codemap, this->codemapLength);
			// Seek pointer is now at start of OPL data
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			if (this->lenData == 0) return false;

			oplEvent->delay = 0;

			if (this->first) {
				// First call, set tempo
				this->first = false;
				oplEvent->tempo = DRO_CLOCK;
				oplEvent->reg = 0;
				oplEvent->val = 0;
				oplEvent->chipIndex = 0;
				return true; // empty event w/ initial tempo
			}

			try {
				uint8_t code, arg;
nextCode:
				this->input >> u8(code) >> u8(arg);
				this->lenData--;
				if (code == this->codeShortDelay) {
					oplEvent->delay += arg + 1;
					goto nextCode;
				} else if (code == this->codeLongDelay) {
					oplEvent->delay += (arg + 1) << 8;
					goto nextCode;
				}

				// High bit indicates which chip to use
				oplEvent->chipIndex = code >> 7;

				if ((code & 0x7F) >= this->codemapLength) {
					std::cerr << "WARNING: DRO file is using codes past the end of the code "
						"map!" << std::endl;
					// But continue as we will just use register 0xFF in these cases.
				}
				oplEvent->reg = this->codemap[code & 0x7F];
				oplEvent->val = arg;
			} catch (const stream::incomplete_read&) {
				return false;
			}

			return true;
		}

	protected:
		stream::input_sptr input;   ///< Input .raw file
		bool first;                 ///< Is this the first time readNextPair() has been called?
		unsigned long lenData;      ///< Length of song data (where song stops and tags start)

		uint8_t codeShortDelay;     ///< DRO code value used for a short delay
		uint8_t codeLongDelay;      ///< DRO code value used for a long delay
		uint8_t codemapLength;      ///< Number of valid entries in codemap array
		uint8_t codemap[128];       ///< Map DRO code values to OPL registers
};


/// Encode OPL register/value pairs into .dro file data.
class OPLWriterCallback_DRO_v2: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_DRO_v2()
			:	usPerTick(DRO_CLOCK),
				buffer(new stream::string()),
				oplType(DRO2_OPLTYPE_OPL2),
				codemapLength(0),
				cachedDelay(0),
				numPairs(0),
				msSongLength(0)
		{
			memset(this->codemap, 0xff, sizeof(this->codemap));
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			// Remember the delay for next time in case we get a register we can't write
			this->cachedDelay += oplEvent->delay;

			assert(oplEvent->chipIndex < 2);
			if (!(
				(oplEvent->reg == 0x01) ||
				(oplEvent->reg == 0x04) ||
				(oplEvent->reg == 0x05) ||
				(oplEvent->reg == 0x08) ||
				(oplEvent->reg == 0xbd) ||
				((oplEvent->reg >= 0x20) && (oplEvent->reg <= 0x35)) ||
				((oplEvent->reg >= 0x40) && (oplEvent->reg <= 0x55)) ||
				((oplEvent->reg >= 0x60) && (oplEvent->reg <= 0x75)) ||
				((oplEvent->reg >= 0x80) && (oplEvent->reg <= 0x95)) ||
				((oplEvent->reg >= 0xe0) && (oplEvent->reg <= 0xf5)) ||
				((oplEvent->reg >= 0xa0) && (oplEvent->reg <= 0xa8)) ||
				((oplEvent->reg >= 0xb0) && (oplEvent->reg <= 0xb8)) ||
				((oplEvent->reg >= 0xc0) && (oplEvent->reg <= 0xc8))
			)) {
				// We have to skip any invalid registers, otherwise there may not be
				// enough space in the codemap table to fit the valid registers.
				std::cerr << "WARNING: Unused OPL register 0x" << std::hex
					<< (unsigned int)oplEvent->reg
					<< " cannot be written to a DROv2 file." << std::endl;
				return;
			}

			// Convert ticks into a DRO delay value (which is actually milliseconds)
			unsigned long delay = this->cachedDelay * this->usPerTick / DRO_CLOCK;
			this->cachedDelay = 0;
			this->msSongLength += delay;
			// Write out the delay in one or more lots of 65535, 255 or less.
			while (delay > 0) {
				if (delay > 256) {
					unsigned long big = (delay >> 8) - 1;
					if (big > 0xff) big = 0xff;
					//uint8_t big = (delay >= 0x10000) ? 0xff : (delay >> 8) - 1;
					// Write out a 'long' delay
					this->buffer
						<< u8(DRO2_CMD_LONGDELAY)
						<< u8(big)
					;
					delay -= (big + 1) << 8;
					this->numPairs++;
					continue;
				}
				assert(delay <= 256);
				this->buffer
					<< u8(DRO2_CMD_SHORTDELAY)
					<< u8(delay - 1) // delay value
				;
				this->numPairs++;
				break; // delay would == 0
			}

			uint8_t code;
			if (this->codemap[oplEvent->reg] == 0xff) {
				code = this->codemapLength++;
				if (this->codemapLength > 128) {
					throw stream::error("Cannot write a DROv2 file that uses more than "
						"127 different OPL registers");
				}
				this->codemap[oplEvent->reg] = code;
			} else {
				code = this->codemap[oplEvent->reg];
			}

			if (oplEvent->chipIndex == 1) {
				code |= 0x80;
				if ((oplEvent->reg == 0x05) && (oplEvent->val & 1)) {
					// Enabled OPL3
					this->oplType = DRO2_OPLTYPE_OPL3;
				} else if (this->oplType == DRO2_OPLTYPE_OPL2) {
					// Haven't enabled OPL3 yet
					this->oplType = DRO2_OPLTYPE_DUALOPL2;
				}
			}
			this->buffer
				<< u8(code)
				<< u8(oplEvent->val)
			;
			this->numPairs++;
			return;
		}

		virtual void writeTempoChange(tempo_t usPerTick)
		{
			assert(usPerTick != 0);
			this->usPerTick = usPerTick;
			return;
		}

		/// Write out all the cached data.
		void write(stream::output_sptr output)
		{
			assert(output->tellp() == 12);

			// Write out the header
			output
				<< u32le(this->numPairs)     // Song length in pairs
				<< u32le(this->msSongLength) // Song length in milliseconds
				<< u8(this->oplType)         // Hardware type (0=OPL2, 1=dual OPL2, 2=OPL3)
				<< u8(0)                     // Format (0 == interleaved)
				<< u8(0)                     // Compression (0 == uncompressed)
				<< u8(DRO2_CMD_SHORTDELAY)   // Short delay code
				<< u8(DRO2_CMD_LONGDELAY)    // Long delay code
				<< u8(this->codemapLength)   // Codemap length
			;
			for (unsigned int c = 0; c < this->codemapLength; c++) {
				for (unsigned int i = 0; i < sizeof(this->codemap); i++) {
					if (this->codemap[i] == c) {
						output << u8(i);
					}
				}
			}
			// Write the actual OPL data from the buffer
			output->write(this->buffer->str().c_str(), this->numPairs << 1);

			return;
		}

	protected:
		tempo_t usPerTick;          ///< Latest microseconds per tick value (tempo)
		stream::string_sptr buffer; ///< Buffer to store output data until finish()
		uint8_t oplType;            ///< OPL hardware type to write into DRO header
		uint8_t codemapLength;      ///< Number of valid entries in codemap array
		uint8_t codemap[256];       ///< Map DRO code values to OPL registers
		unsigned long cachedDelay;  ///< Delay remembered but not yet written
		uint32_t numPairs;          ///< Number of data pairs in file
		uint32_t msSongLength;      ///< Song length in milliseconds
};

std::string MusicType_DRO_v2::getCode() const
{
	return "dro-dosbox-v2";
}

std::string MusicType_DRO_v2::getFriendlyName() const
{
	return "DOSBox Raw OPL version 2";
}

std::vector<std::string> MusicType_DRO_v2::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("dro");
	return vcExtensions;
}

MusicType::Certainty MusicType_DRO_v2::isInstance(stream::input_sptr psMusic) const
{
	// Make sure the signature matches
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c01
	char sig[8];
	psMusic->seekg(0, stream::start);
	psMusic->read(sig, 8);
	if (strncmp(sig, "DBRAWOPL", 8) != 0) return DefinitelyNo;

	// Make sure the header says it's version 2.0
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c02
	uint16_t verMajor, verMinor;
	psMusic >> u16le(verMajor) >> u16le(verMinor);
	if ((verMajor != 2) || (verMinor != 0)) return DefinitelyNo;

	// TESTED BY: mus_dro_dosbox_v2_isinstance_c00
	return DefinitelyYes;
}

MusicPtr MusicType_DRO_v2::read(stream::input_sptr input, SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	input->seekg(0, stream::start);

	OPLReaderCallback_DRO_v2 cb(input);
	MusicPtr music = oplDecode(&cb, DelayIsPreData, OPL_FNUM_DEFAULT);

	// See if there are any tags present
	readMalvMetadata(input, music->metadata);

	return music;
}

void MusicType_DRO_v2::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	output->write("DBRAWOPL\x02\x00\x00\x00", 12);

	// Call the generic OPL writer.
	OPLWriterCallback_DRO_v2 cb;
	oplEncode(&cb, DelayIsPreData, OPL_FNUM_DEFAULT, flags, music);
	cb.write(output);

	// Write out any metadata
	writeMalvMetadata(output, music->metadata);

	// Set final filesize to this
	output->truncate_here();

	return;
}

SuppFilenames MusicType_DRO_v2::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_DRO_v2::getMetadataList() const
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	types.push_back(Metadata::Author);
	types.push_back(Metadata::Description);
	return types;
}
