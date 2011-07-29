/**
 * @file   mus-dro-dosbox-v2.cpp
 * @brief  Support for the second version of the DOSBox Raw OPL .DRO format.
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

#include <camoto/iostream_helpers.hpp>
#include "mus-dro-dosbox-v2.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

std::string MusicType_DRO_v2::getCode() const
	throw ()
{
	return "dro-dosbox-v2";
}

std::string MusicType_DRO_v2::getFriendlyName() const
	throw ()
{
	return "DOSBox Raw OPL version 2";
}

std::vector<std::string> MusicType_DRO_v2::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("dro");
	return vcExtensions;
}

MusicType::Certainty MusicType_DRO_v2::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	// Make sure the signature matches
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c01
	char sig[8];
	psMusic->seekg(0, std::ios::beg);
	psMusic->read(sig, 8);
	if (strncmp(sig, "DBRAWOPL", 8) != 0) return MusicType::DefinitelyNo;

	// Make sure the header says it's version 2.0
	// TESTED BY: mus_dro_dosbox_v2_isinstance_c02
	uint16_t verMajor, verMinor;
	psMusic >> u16le(verMajor) >> u16le(verMinor);
	if ((verMajor != 2) || (verMinor != 0)) return MusicType::DefinitelyNo;

	// TESTED BY: mus_dro_dosbox_v2_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicWriterPtr MusicType_DRO_v2::create(ostream_sptr output, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_DRO_v2(output));
}

MusicReaderPtr MusicType_DRO_v2::open(istream_sptr input, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_DRO_v2(input));
}

SuppFilenames MusicType_DRO_v2::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}

MusicReader_DRO_v2::MusicReader_DRO_v2(istream_sptr input)
	throw (std::ios::failure) :
		MusicReader_GenericOPL(DelayIsPreData),
		input(input)
{
	this->input->seekg(12, std::ios::beg);
	this->input >> u32le(this->lenData);
	this->input->seekg(6, std::ios::cur);
	uint8_t compression, codemapLength;
	this->input
		>> u8(compression)
		>> u8(this->codeShortDelay)
		>> u8(this->codeLongDelay)
		>> u8(this->codemapLength)
	;
	if (compression != 0) throw std::ios::failure("Compressed DRO files are not implemented (didn't even know they existed)");
	if (codemapLength > 127) throw std::ios::failure("DRO code map too large");
	memset(this->codemap, 0xFF, sizeof(this->codemap));
	this->input->read((char *)this->codemap, this->codemapLength);
	this->changeSpeed(1000); // 1000us (1ms) per tick
}

MusicReader_DRO_v2::~MusicReader_DRO_v2()
	throw ()
{
}

void MusicReader_DRO_v2::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	this->input->seekg(12, std::ios::beg);
	this->input >> u32le(this->lenData);
	this->input->seekg(26 + this->codemapLength, std::ios::beg);
	return;
}

bool MusicReader_DRO_v2::nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg, uint8_t *val)
	throw (std::ios::failure)
{
	*delay = 0;
	*reg = 0; *val = 0; // in case of trailing delay
	try {
		uint8_t code, arg;
nextCode:
		if (this->lenData == 0) return false; // end of song
		this->input >> u8(code) >> u8(arg);
		this->lenData--;
		if (code == this->codeShortDelay) {
			*delay += arg + 1;
			goto nextCode;
		} else if (code == this->codeLongDelay) {
			*delay += (arg + 1) << 8;
			goto nextCode;
		}

		// High bit indicates which chip to use
		*chipIndex = code >> 7;

		if ((code & 0x7F) >= this->codemapLength) {
			std::cerr << "WARNING: DRO file is using codes past the end of the code "
				"map!" << std::endl;
			// But continue as we will just use register 0xFF in these cases.
		}
		*reg = this->codemap[code & 0x7F];
		*val = arg;
	} catch (std::ios::failure) {
		if (this->input->eof()) return false;
		throw;
	}
	return true; // read some data
}

// TODO: metadata functions


MusicWriter_DRO_v2::MusicWriter_DRO_v2(ostream_sptr output)
	throw () :
		MusicWriter_GenericOPL(DelayIsPreData),
		output(output),
		dataLen(0),
		numTicks(0),
		usPerTick(0),
		oplType(0),
		codemapLength(0),
		cachedDelay(0)
{
	memset(this->codemap, 0xff, sizeof(this->codemap));
}

MusicWriter_DRO_v2::~MusicWriter_DRO_v2()
	throw ()
{
}

void MusicWriter_DRO_v2::start()
	throw (std::ios::failure)
{
	this->output->write("DBRAWOPL\x02\x00\x00\x00", 12);

	return;
}

void MusicWriter_DRO_v2::finish()
	throw (std::ios::failure)
{
	this->MusicWriter_GenericOPL::finish();

	// Write out some placeholders, which will be overwritten later
	this->output
		<< u32le(this->dataLen)    // Song length in pairs
		<< u32le(this->numTicks)   // Song length in milliseconds
		<< u8(this->oplType)       // Hardware type (0=OPL2, 1=dual OPL2, 2=OPL3)
		<< u8(0)                   // Format (0 == interleaved)
		<< u8(0)                   // Compression (0 == uncompressed)
		<< u8(0xfe)                // Short delay code
		<< u8(0xff)                // Long delay code
		<< u8(this->codemapLength) // Codemap length
	;
	for (int c = 0; c < this->codemapLength; c++) {
		for (int i = 0; i < sizeof(this->codemap); i++) {
			if (this->codemap[i] == c) {
				this->output << u8(i);
			}
		}
	}
	this->output->write((char *)this->buffer.str().c_str(), this->dataLen << 1);

	return;
}

void MusicWriter_DRO_v2::changeSpeed(uint32_t usPerTick)
	throw ()
{
	this->usPerTick = usPerTick;
	return;
}

void MusicWriter_DRO_v2::nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
	throw (std::ios::failure)
{
	assert(this->usPerTick > 0); // make sure this has been set already
	assert(chipIndex < 2);

	// Remember the delay for next time in case we get a register we can't write
	this->cachedDelay += delay;

	if (!(
		(reg == 0x01) ||
		(reg == 0x04) ||
		(reg == 0x05) ||
		(reg == 0x08) ||
		(reg == 0xbd) ||
		((reg >= 0x20) && (reg <= 0x35)) ||
		((reg >= 0x40) && (reg <= 0x55)) ||
		((reg >= 0x60) && (reg <= 0x75)) ||
		((reg >= 0x80) && (reg <= 0x95)) ||
		((reg >= 0xe0) && (reg <= 0xf5)) ||
		((reg >= 0xa0) && (reg <= 0xa8)) ||
		((reg >= 0xb0) && (reg <= 0xb8)) ||
		((reg >= 0xc0) && (reg <= 0xc8))
	)) {
		std::cerr << "WARNING: Unused OPL register 0x" << std::hex << reg
			<< " cannot be written to a DROv2 file." << std::endl;
		return;
	}

	// Convert ticks into a DRO delay value (which is actually milliseconds)
	delay = this->cachedDelay * this->usPerTick / 1000;
	this->cachedDelay = 0;

	if (delay > 256) {
		// Write out a 'long' delay
		uint16_t big = (delay >> 8) - 1;
		delay = big & 0xff;
		this->buffer
			<< u8(0xff)
			<< u8(big)
		;
		this->dataLen++;
	}
	if (delay > 0) {
		// Write out a 'short' delay
		this->buffer
			<< u8(0xfe)
			<< u8(delay - 1)
		;
		this->dataLen++;
	}
	uint8_t code;
	if (this->codemap[reg] == 0xff) {
		code = this->codemapLength++;
		if (this->codemapLength > 128) {
			throw std::ios::failure("Cannot write a DROv2 file that uses more than "
				"127 different OPL registers");
		}
		this->codemap[reg] = code;
	} else {
		code = this->codemap[reg];
	}
	// TODO: ignore invalid registers (keeping the delay) or there won't be enough codes
	if (chipIndex == 1) {
		code |= 0x80;
		this->oplType = 1; // OPL3 - could also be dual OPL2 though!
	}
	this->buffer
		<< u8(code)
		<< u8(val)
	;

	this->dataLen++;
	this->numTicks += delay;
	return;
}
