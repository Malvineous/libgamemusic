/**
 * @file  mus-dro-dosbox-v1.cpp
 * @brief Support for the first version of the DOSBox Raw OPL .DRO format.
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

#include <camoto/iostream_helpers.hpp>
#include "decode-opl.hpp"
#include "encode-opl.hpp"
#include "metadata-malv.hpp"
#include "mus-dro-dosbox-v1.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Length of each .dro tick in microseconds
#define DRO_CLOCK  1000

/// Header value for single OPL2 chip
#define DRO_OPLTYPE_OPL2 0

/// Header value for two OPL2 chips
#define DRO_OPLTYPE_DUALOPL2 2

/// Header value for single OPL3 chip
#define DRO_OPLTYPE_OPL3 1

/// Decode data in a .dro file to provide register/value pairs.
class OPLReaderCallback_DRO_v1: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_DRO_v1(stream::input& content)
			:	content(content),
				chipIndex(0)
		{
			this->content.seekg(16, stream::start);
			this->content >> u32le(this->lenData);
			// Skip to start of OPL data
			this->content.seekg(24, stream::start);
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			assert(oplEvent->valid == 0);

			if (this->lenData == 0) return false;
			oplEvent->delay = 0;

			try {
				uint8_t code;
nextCode:
				if (this->lenData == 0) return false;
				this->content >> u8(code);
				this->lenData--;
				switch (code) {
					case 0x00: // short delay
						this->content >> u8(code);
						this->lenData--;
						oplEvent->delay += code + 1;
						oplEvent->valid |= OPLEvent::Delay;
						goto nextCode;
					case 0x01: { // long delay
						uint16_t amt;
						this->content >> u16le(amt);
						this->lenData--;
						oplEvent->delay += amt + 1;
						oplEvent->valid |= OPLEvent::Delay;
						goto nextCode;
					}
					case 0x02:
						this->chipIndex = 0;
						goto nextCode;
					case 0x03:
						this->chipIndex = 1;
						goto nextCode;
					case 0x04: // escape
						this->content
							>> u8(oplEvent->reg)
							>> u8(oplEvent->val)
						;
						this->lenData -= 2;
						oplEvent->valid |= OPLEvent::Regs;
						break;
					default: // normal reg
						oplEvent->chipIndex = this->chipIndex;
						oplEvent->reg = code;
						this->content >> u8(oplEvent->val);
						this->lenData--;
						oplEvent->valid |= OPLEvent::Regs;
						break;
				}
			} catch (const stream::incomplete_read&) {
				return false;
			}

			return true;
		}

	protected:
		stream::input& content;     ///< Input file
		unsigned int chipIndex;     ///< Index of the currently selected OPL chip
		unsigned long lenData;      ///< Length of song data (where song stops and tags start)
};


/// Encode OPL register/value pairs into .dro file data.
class OPLWriterCallback_DRO_v1: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_DRO_v1(stream::output& content)
			:	content(content),
				lastChipIndex(0),
				msSongLength(0),
				oplType(DRO_OPLTYPE_OPL2)
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			if (oplEvent->valid & OPLEvent::Delay) {
				// Convert ticks into a DRO delay value (which is actually milliseconds)
				unsigned long delay = oplEvent->delay
					* oplEvent->tempo.usPerTick / HERTZ_TO_uS(DRO_CLOCK);
				// Write out the delay in one or more lots of 65535, 255 or less.
				while (delay > 0) {
					if (delay > 256) {
						uint16_t ld = (delay > 65536) ? 65535 : delay - 1;
						// Write out a 'long' delay
						this->content
							<< u8(1)
							<< u16le(ld)
						;
						delay -= ld + 1;
						this->msSongLength += ld + 1;
						continue;
					}
					assert(delay <= 256);
					this->content
						<< u8(0) // delay command
						<< u8(delay - 1) // delay value
					;
					this->msSongLength += delay;
					break; // delay would == 0
				}
			}

			if (oplEvent->valid & OPLEvent::Regs) {
				if (oplEvent->chipIndex != this->lastChipIndex) {
					assert(oplEvent->chipIndex < 2);
					this->content << u8(0x02 + oplEvent->chipIndex);
					this->lastChipIndex = oplEvent->chipIndex;
				}
				if (oplEvent->chipIndex == 1) {
					if ((oplEvent->reg == 0x05) && (oplEvent->val & 1)) {
						// Enabled OPL3
						this->oplType = DRO_OPLTYPE_OPL3;
					} else if (this->oplType == DRO_OPLTYPE_OPL2) {
						// Haven't enabled OPL3 yet
						this->oplType = DRO_OPLTYPE_DUALOPL2;
					}
				}
				if (oplEvent->reg < 0x05) {
					// Need to escape this reg
					this->content
						<< u8(4)
					;
					// Now the following byte will be treated as a register
					// regardless of its value.
				}
				this->content
					<< u8(oplEvent->reg)
					<< u8(oplEvent->val)
				;
			}
			return;
		}

	protected:
		stream::output& content;    ///< Output file
		unsigned int lastChipIndex; ///< Index of the currently selected OPL chip

	public:
		uint32_t msSongLength;      ///< Song length in milliseconds
		uint8_t oplType;            ///< OPL hardware type to write into DRO header
};

std::string MusicType_DRO_v1::code() const
{
	return "dro-dosbox-v1";
}

std::string MusicType_DRO_v1::friendlyName() const
{
	return "DOSBox Raw OPL version 1";
}

std::vector<std::string> MusicType_DRO_v1::fileExtensions() const
{
	return {
		"dro",
	};
}

MusicType::Caps MusicType_DRO_v1::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
		| Caps::HardwareOPL3
	;
}

MusicType::Certainty MusicType_DRO_v1::isInstance(stream::input& content) const
{
	// Too short
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c03
	if (content.size() < 12) return Certainty::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c01
	char sig[8];
	content.seekg(0, stream::start);
	content.read(sig, 8);
	if (strncmp(sig, "DBRAWOPL", 8) != 0) return Certainty::DefinitelyNo;

	// Make sure the header says it's version 0.1
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c02
	uint16_t verMajor, verMinor;
	content >> u16le(verMajor) >> u16le(verMinor);
	if ((verMajor != 0) || (verMinor != 1)) return Certainty::DefinitelyNo;

	// TESTED BY: mus_dro_dosbox_v1_isinstance_c00
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c04
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_DRO_v1::read(stream::input& content,
	SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	content.seekg(0, stream::start);

	Tempo initialTempo;
	initialTempo.usPerTick = DRO_CLOCK;

	OPLReaderCallback_DRO_v1 cb(content);
	auto music = oplDecode(&cb, DelayType::DelayIsPreData, OPL_FNUM_DEFAULT, initialTempo);

	// See if there are any tags present
	readMalvMetadata(content, music.get());

	return music;
}

void MusicType_DRO_v1::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	content.write("DBRAWOPL\x00\x00\x01\x00", 12);

	// Write out some placeholders, which will be overwritten later
	content
		<< u32le(0) // Song length in milliseconds
		<< u32le(0) // Song length in bytes
		<< u32le(0) // Hardware type (0=OPL2, 1=OPL3, 2=dual OPL2)
	;

	// Call the generic OPL writer.
	OPLWriterCallback_DRO_v1 cb(content);
	auto oplFlags = toOPLFlags(flags);
	oplEncode(&cb, music, DelayType::DelayIsPreData, OPL_FNUM_DEFAULT, oplFlags);

	// Update the placeholder we wrote in the constructor with the file size
	int size = content.tellp();
	size -= 24; // don't count the header

	// Write out any metadata
	writeMalvMetadata(content, music.attributes());

	// Set final filesize to this
	content.truncate_here();

	content.seekp(12, stream::start);
	content
		<< u32le(cb.msSongLength) // Song length in milliseconds (one tick == 1ms)
		<< u32le(size)            // Song length in bytes
		<< u32le(cb.oplType)      // Hardware type (0=OPL2, 1=OPL3, 2=dual OPL2)
	;

	return;
}

SuppFilenames MusicType_DRO_v1::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_DRO_v1::supportedAttributes() const
{
	return supportedMalvMetadata();
}
