/**
 * @file   mus-dro-dosbox-v1.cpp
 * @brief  Support for the first version of the DOSBox Raw OPL .DRO format.
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
#include "mus-dro-dosbox-v1.hpp"

using namespace camoto::gamemusic;

std::string MusicType_DRO_v1::getCode() const
	throw ()
{
	return "dro-dosbox-v1";
}

std::string MusicType_DRO_v1::getFriendlyName() const
	throw ()
{
	return "DOSBox Raw OPL version 1";
}

std::vector<std::string> MusicType_DRO_v1::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("dro");
	return vcExtensions;
}

E_CERTAINTY MusicType_DRO_v1::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	// Make sure the signature matches
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c01
	char sig[8];
	psMusic->seekg(0, std::ios::beg);
	psMusic->read(sig, 8);
	if (strncmp(sig, "DBRAWOPL", 8) != 0) return EC_DEFINITELY_NO;

	// Make sure the header says it's version 0.1
	// TESTED BY: mus_dro_dosbox_v1_isinstance_c02
	uint16_t verMajor, verMinor;
	psMusic >> u16le(verMajor) >> u16le(verMinor);
	if ((verMajor != 0) || (verMinor != 1)) return EC_DEFINITELY_NO;

	// TESTED BY: mus_dro_dosbox_v1_isinstance_c00
	return EC_DEFINITELY_YES;
}

MusicWriterPtr MusicType_DRO_v1::create(ostream_sptr output, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_DRO_v1(output));
}

MusicReaderPtr MusicType_DRO_v1::open(istream_sptr input, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_DRO_v1(input));
}

MP_SUPPLIST MusicType_DRO_v1::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return MP_SUPPLIST();
}

MusicReader_DRO_v1::MusicReader_DRO_v1(istream_sptr input)
	throw (std::ios::failure) :
		MusicReader_GenericOPL(DelayIsPreData),
		input(input),
		chipIndex(0)
{
	this->rewind();
	this->changeSpeed(1000); // 1000us (1ms) per tick
}

MusicReader_DRO_v1::~MusicReader_DRO_v1()
	throw ()
{
}

void MusicReader_DRO_v1::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	this->input->seekg(16, std::ios::beg);
	this->input >> u32le(this->lenData);
	this->input->seekg(24, std::ios::beg);
	return;
}

bool MusicReader_DRO_v1::nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg, uint8_t *val)
	throw (std::ios::failure)
{
	*delay = 0;
	*reg = 0; *val = 0; // in case of trailing delay
	try {
		uint8_t code;
nextCode:
		if (this->lenData == 0) return false; // end of song
		this->input >> u8(code);
		this->lenData--;
		switch (code) {
			case 0x00: // short delay
				this->input >> u8(code);
				this->lenData--;
				*delay += code + 1;
				goto nextCode;
			case 0x01: { // long delay
				uint16_t amt;
				this->input >> u16le(amt);
				this->lenData -= 2;
				*delay += amt + 1;
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
					>> u8(*reg)
					>> u8(*val)
				;
				this->lenData -= 2;
				break;
			default: // normal reg
				*reg = code;
				this->input >> u8(*val);
				this->lenData--;
				break;
		}
	} catch (std::ios::failure) {
		if (this->input->eof()) return false;
		throw;
	}
	*chipIndex = this->chipIndex;
	return true; // read some data
	//return this->lenData > 0;//!this->input->eof();
}

// TODO: metadata functions


MusicWriter_DRO_v1::MusicWriter_DRO_v1(ostream_sptr output)
	throw () :
		MusicWriter_GenericOPL(DelayIsPreData),
		output(output),
		numTicks(0),
		usPerTick(0),
		lastChipIndex(0)
{
}

MusicWriter_DRO_v1::~MusicWriter_DRO_v1()
	throw ()
{
}

void MusicWriter_DRO_v1::start()
	throw (std::ios::failure)
{
	this->output->write("DBRAWOPL\x00\x00\x01\x00", 12);

	// Write out some placeholders, which will be overwritten later
	this->output
		<< u32le(0) // Song length in milliseconds
		<< u32le(0) // Song length in bytes
		<< u32le(0) // Hardware type (0=OPL2, 1=OPL3, 2=dual OPL2)
	;
	return;
}

void MusicWriter_DRO_v1::finish()
	throw (std::ios::failure)
{
	this->MusicWriter_GenericOPL::finish();

	// Update the placeholder we wrote in the constructor with the file size
	int size = this->output->tellp();
	size -= 24; // don't count the header

	// TODO: write out metadata here

	this->output->seekp(12, std::ios::beg);
	this->output
		<< u32le(this->numTicks) // Song length in milliseconds (one tick == 1ms)
		<< u32le(size) // Song length in bytes
//		<< u32le(0) // Hardware type (0=OPL2, 1=OPL3, 2=dual OPL2)
	;
	return;
}

void MusicWriter_DRO_v1::changeSpeed(uint32_t usPerTick)
	throw ()
{
	this->usPerTick = usPerTick;
	return;
}

void MusicWriter_DRO_v1::nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
	throw (std::ios::failure)
{
	assert(this->usPerTick > 0); // make sure this has been set already

	// Convert ticks into a DRO delay value (which is actually milliseconds)
	delay = delay * this->usPerTick / 1000;

	if (delay > 256) {
		// Write out a 'long' delay
		this->output
			<< u8(1)
			<< u16le(delay - 1)
		;
	} else if (delay > 0) {
		// Write out a 'short' delay
		this->output
			<< u8(0)
			<< u8(delay - 1)
		;
	}
	if (chipIndex != this->lastChipIndex) {
		assert(chipIndex < 2);
		this->output << u8(0x02 + chipIndex);
		this->lastChipIndex = chipIndex;
	}
	if (reg < 0x05) {
		// Need to escape this reg
		this->output
			<< u8(4)
		;
		// Now the following byte will be treated as a register
		// regardless of its value.
	}
	this->output
		<< u8(reg)
		<< u8(val)
	;
	this->numTicks += delay;
	return;
}

// TODO: metadata functions
