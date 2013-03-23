/**
 * @file   mus-dro-dosbox-v1.cpp
 * @brief  Support for the first version of the DOSBox Raw OPL .DRO format.
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
		OPLReaderCallback_DRO_v1(stream::input_sptr input)
			:	input(input),
				first(true),
				chipIndex(0)
		{
			this->input->seekg(16, stream::start);
			this->input >> u32le(this->lenData);
			// Skip to start of OPL data
			this->input->seekg(24, stream::start);
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
				uint8_t code;
nextCode:
				if (this->lenData == 0) return false;
				this->input >> u8(code);
				this->lenData--;
				switch (code) {
					case 0x00: // short delay
						this->input >> u8(code);
						this->lenData--;
						oplEvent->delay += code + 1;
						goto nextCode;
					case 0x01: { // long delay
						uint16_t amt;
						this->input >> u16le(amt);
						this->lenData--;
						oplEvent->delay += amt + 1;
						goto nextCode;
					}
					case 0x02:
						this->chipIndex = 0;
						goto nextCode;
					case 0x03:
						this->chipIndex = 1;
						goto nextCode;
					case 0x04: // escape
						this->input
							>> u8(oplEvent->reg)
							>> u8(oplEvent->val)
						;
						this->lenData -= 2;
						break;
					default: // normal reg
						oplEvent->reg = code;
						this->input >> u8(oplEvent->val);
						this->lenData--;
						break;
				}
			} catch (const stream::incomplete_read&) {
				return false;
			}
			oplEvent->chipIndex = this->chipIndex;
			return true;
		}

	protected:
		stream::input_sptr input;   ///< Input file
		bool first;                 ///< Is this the first time readNextPair() has been called?
		unsigned int chipIndex;     ///< Index of the currently selected OPL chip
		unsigned long lenData;      ///< Length of song data (where song stops and tags start)
};


/// Encode OPL register/value pairs into .dro file data.
class OPLWriterCallback_DRO_v1: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_DRO_v1(stream::output_sptr output)
			:	output(output),
				lastChipIndex(0),
				usPerTick(DRO_CLOCK),
				msSongLength(0),
				oplType(DRO_OPLTYPE_OPL2)
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			// Convert ticks into a DRO delay value (which is actually milliseconds)
			unsigned long delay = oplEvent->delay * this->usPerTick / DRO_CLOCK;
			// Write out the delay in one or more lots of 65535, 255 or less.
			while (delay > 0) {
				if (delay > 256) {
					uint16_t ld = (delay > 65536) ? 65535 : delay - 1;
					// Write out a 'long' delay
					this->output
						<< u8(1)
						<< u16le(ld)
					;
					delay -= ld + 1;
					this->msSongLength += ld + 1;
					continue;
				}
				assert(delay <= 256);
				this->output
					<< u8(0) // delay command
					<< u8(delay - 1) // delay value
				;
				this->msSongLength += delay;
				break; // delay would == 0
			}

			if (oplEvent->chipIndex != this->lastChipIndex) {
				assert(oplEvent->chipIndex < 2);
				this->output << u8(0x02 + oplEvent->chipIndex);
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
				this->output
					<< u8(4)
				;
				// Now the following byte will be treated as a register
				// regardless of its value.
			}
			this->output
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
		unsigned int lastChipIndex; ///< Index of the currently selected OPL chip
		tempo_t usPerTick;          ///< Latest microseconds per tick value (tempo)

	public:
		uint32_t msSongLength;      ///< Song length in milliseconds
		uint8_t oplType;            ///< OPL hardware type to write into DRO header
};

std::string MusicType_DRO_v1::getCode() const
{
	return "dro-dosbox-v1";
}

std::string MusicType_DRO_v1::getFriendlyName() const
{
	return "DOSBox Raw OPL version 1";
}

std::vector<std::string> MusicType_DRO_v1::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("dro");
	return vcExtensions;
}

MusicType::Certainty MusicType_DRO_v1::isInstance(stream::input_sptr input) const
{
	// Make sure the signature matches
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c01
	char sig[8];
	input->seekg(0, stream::start);
	input->read(sig, 8);
	if (strncmp(sig, "DBRAWOPL", 8) != 0) return DefinitelyNo;

	// Make sure the header says it's version 0.1
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c02
	uint16_t verMajor, verMinor;
	input >> u16le(verMajor) >> u16le(verMinor);
	if ((verMajor != 0) || (verMinor != 1)) return DefinitelyNo;

	// TESTED BY: mus_dro_dosbox_v1_isinstance_c00
	return DefinitelyYes;
}

MusicPtr MusicType_DRO_v1::read(stream::input_sptr input, SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	input->seekg(0, stream::start);

	OPLReaderCallback_DRO_v1 cb(input);
	MusicPtr music = oplDecode(&cb, DelayIsPreData, OPL_FNUM_DEFAULT);

	// See if there are any tags present
	readMalvMetadata(input, music->metadata);

	return music;
}

void MusicType_DRO_v1::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	output->write("DBRAWOPL\x00\x00\x01\x00", 12);

	// Write out some placeholders, which will be overwritten later
	output
		<< u32le(0) // Song length in milliseconds
		<< u32le(0) // Song length in bytes
		<< u32le(0) // Hardware type (0=OPL2, 1=OPL3, 2=dual OPL2)
	;

	// Call the generic OPL writer.
	OPLWriterCallback_DRO_v1 cb(output);
	oplEncode(&cb, DelayIsPreData, OPL_FNUM_DEFAULT, flags, music);

	// Update the placeholder we wrote in the constructor with the file size
	int size = output->tellp();
	size -= 24; // don't count the header

	// Write out any metadata
	writeMalvMetadata(output, music->metadata);

	// Set final filesize to this
	output->truncate_here();

	output->seekp(12, stream::start);
	output
		<< u32le(cb.msSongLength) // Song length in milliseconds (one tick == 1ms)
		<< u32le(size)            // Song length in bytes
		<< u32le(cb.oplType)      // Hardware type (0=OPL2, 1=OPL3, 2=dual OPL2)
	;

	return;
}

SuppFilenames MusicType_DRO_v1::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_DRO_v1::getMetadataList() const
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	types.push_back(Metadata::Author);
	types.push_back(Metadata::Description);
	return types;
}
