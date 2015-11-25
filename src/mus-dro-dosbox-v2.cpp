/**
 * @file  mus-dro-dosbox-v2.cpp
 * @brief Support for the second version of the DOSBox Raw OPL .DRO format.
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
		OPLReaderCallback_DRO_v2(stream::input& content)
			:	content(content)
		{
			this->content.seekg(12, stream::start);
			this->content >> u32le(this->lenData);
			this->content.seekg(6, stream::cur);
			uint8_t compression;
			this->content
				>> u8(compression)
				>> u8(this->codeShortDelay)
				>> u8(this->codeLongDelay)
				>> u8(this->codemapLength)
			;
			if (compression != 0) throw stream::error("Compressed DRO files are not implemented (didn't even know they existed)");
			if (this->codemapLength > 127) throw stream::error("DRO code map too large");
			memset(this->codemap, 0xFF, sizeof(this->codemap));
			this->content.read(this->codemap, this->codemapLength);
			// Seek pointer is now at start of OPL data
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			oplEvent->delay = 0;
			if (this->lenData == 0) return false;

			try {
				uint8_t code, arg;
nextCode:
				this->content >> u8(code) >> u8(arg);
				this->lenData--;
				if (code == this->codeShortDelay) {
					oplEvent->delay += arg + 1;
					oplEvent->valid |= OPLEvent::Delay;
					goto nextCode;
				} else if (code == this->codeLongDelay) {
					oplEvent->delay += (arg + 1) << 8;
					oplEvent->valid |= OPLEvent::Delay;
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
				oplEvent->valid |= OPLEvent::Regs;
			} catch (const stream::incomplete_read&) {
				// oplEvent->delay is populated with any final delay
				return false;
			}

			return true;
		}

	protected:
		stream::input& content;   ///< Input .raw file
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
			:	oplType(DRO2_OPLTYPE_OPL2),
				codemapLength(0),
				cachedDelay(0),
				numPairs(0),
				msSongLength(0)
		{
			memset(this->codemap, 0xff, sizeof(this->codemap));
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			if (oplEvent->valid & OPLEvent::Delay) {
				// Remember the delay for next time in case we get a register we can't write
				this->cachedDelay += oplEvent->delay;

				// Convert ticks into a DRO delay value (which is actually milliseconds)
				unsigned long delay = oplEvent->delay
					* oplEvent->tempo.usPerTick / HERTZ_TO_uS(DRO_CLOCK);
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
			}

			if (oplEvent->valid & OPLEvent::Regs) {
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
			}
			return;
		}

		/// Write out all the cached data.
		void write(stream::output& content)
		{
			assert(content.tellp() == 12);

			// Write out the header
			content
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
						content << u8(i);
					}
				}
			}
			// Write the actual OPL data from the buffer
			content.write(this->buffer.data.c_str(), this->numPairs << 1);

			return;
		}

	protected:
		stream::string buffer;      ///< Buffer to store content data until finish()
		uint8_t oplType;            ///< OPL hardware type to write into DRO header
		uint8_t codemapLength;      ///< Number of valid entries in codemap array
		uint8_t codemap[256];       ///< Map DRO code values to OPL registers
		unsigned long cachedDelay;  ///< Delay remembered but not yet written
		uint32_t numPairs;          ///< Number of data pairs in file
		uint32_t msSongLength;      ///< Song length in milliseconds
};

std::string MusicType_DRO_v2::code() const
{
	return "dro-dosbox-v2";
}

std::string MusicType_DRO_v2::friendlyName() const
{
	return "DOSBox Raw OPL version 2";
}

std::vector<std::string> MusicType_DRO_v2::fileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("dro");
	return vcExtensions;
}

MusicType::Caps MusicType_DRO_v2::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
		| Caps::HardwareOPL3
	;
}

MusicType::Certainty MusicType_DRO_v2::isInstance(stream::input& content) const
{
	// Too short
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c03
	if (content.size() < 12) return Certainty::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c01
	char sig[8];
	content.seekg(0, stream::start);
	content.read(sig, 8);
	if (strncmp(sig, "DBRAWOPL", 8) != 0) return Certainty::DefinitelyNo;

	// Make sure the header says it's version 2.0
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c02
	uint16_t verMajor, verMinor;
	content >> u16le(verMajor) >> u16le(verMinor);
	if ((verMajor != 2) || (verMinor != 0)) return Certainty::DefinitelyNo;

	// TESTED BY: mus_dro_dosbox_v2_isinstance_c00
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c04
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_DRO_v2::read(stream::input& content,
	SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	content.seekg(0, stream::start);

	Tempo initialTempo;
	initialTempo.usPerTick = DRO_CLOCK;

	OPLReaderCallback_DRO_v2 cb(content);
	auto music = oplDecode(&cb, DelayType::DelayIsPreData,
		OPL_FNUM_DEFAULT, initialTempo);

	// See if there are any tags present
	readMalvMetadata(content, music.get());

	return music;
}

void MusicType_DRO_v2::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	content.write("DBRAWOPL\x02\x00\x00\x00", 12);

	// Call the generic OPL writer.
	OPLWriterCallback_DRO_v2 cb;
	auto oplFlags = toOPLFlags(flags);
	oplEncode(&cb, music, DelayType::DelayIsPreData, OPL_FNUM_DEFAULT, oplFlags);
	cb.write(content);

	// Write out any metadata
	writeMalvMetadata(content, music.attributes());

	// Set final filesize to this
	content.truncate_here();

	return;
}

SuppFilenames MusicType_DRO_v2::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_DRO_v2::supportedAttributes() const
{
	return supportedMalvMetadata();
}
