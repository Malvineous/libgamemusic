/**
 * @file   mus-raw-rdos.cpp
 * @brief  Support for Rdos RAW OPL capture (.raw) format.
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
#include "mus-raw-rdos.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

std::string MusicType_RAW::getCode() const
	throw ()
{
	return "raw-rdos";
}

std::string MusicType_RAW::getFriendlyName() const
	throw ()
{
	return "Rdos Raw OPL";
}

std::vector<std::string> MusicType_RAW::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("raw");
	return vcExtensions;
}


MusicType::Certainty MusicType_RAW::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	// Make sure the signature matches
	// TESTED BY: mus_raw_rdos_isinstance_c01
	try {
		char sig[8];
		psMusic->seekg(0, std::ios::beg);
		psMusic->read(sig, 8);
		if (strncmp(sig, "RAWADATA", 8) != 0) return MusicType::DefinitelyNo;
	} catch (std::ios::failure) {
		return MusicType::DefinitelyNo; // EOF
	}

	// TESTED BY: mus_raw_rdos_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicWriterPtr MusicType_RAW::create(ostream_sptr output, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_RAW(output));
}

MusicReaderPtr MusicType_RAW::open(istream_sptr input, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_RAW(input));
}

SuppFilenames MusicType_RAW::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}

MusicReader_RAW::MusicReader_RAW(istream_sptr input)
	throw (std::ios::failure) :
		MusicReader_GenericOPL(DelayIsPreData),
		input(input),
		chipIndex(0)
{
	this->input->seekg(8, std::ios::beg);
	uint16_t clock;
	this->input >> u16le(clock);
	if (clock == 0) clock = 0xffff;
	this->changeSpeed(clock / 1.192180);
}

MusicReader_RAW::~MusicReader_RAW()
	throw ()
{
}

void MusicReader_RAW::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	this->input->seekg(10, std::ios::beg);
	return;
}

bool MusicReader_RAW::nextPair(uint32_t *delay, uint8_t *chipIndex,
	uint8_t *reg, uint8_t *val
)
	throw (std::ios::failure)
{
	*delay = 0;
	try {
nextCode:
		this->input >> u8(*val) >> u8(*reg);
		switch (*reg) {
			case 0x00: // short delay
				*delay += *val;
				goto nextCode;
			case 0x02: { // control
				switch (*val) {
					case 0x00: { // clock change
						uint16_t clock;
						this->input >> u16le(clock);
						if (clock == 0) clock = 0xffff;
						this->changeSpeed(clock / 1.192180);
						break;
					}
					case 0x01:
						this->chipIndex = 0;
						break;
					case 0x02:
						this->chipIndex = 1;
						break;
				}
				goto nextCode;
			}
			case 0xFF:
				if (*val == 0xFF) return false; // EOF
				break;
			default: // normal reg
				break;
		}
	} catch (std::ios::failure) {
		if (this->input->eof()) return false;
		throw;
	}
	*chipIndex = this->chipIndex;
	return !this->input->eof();
}

// TODO: metadata functions


MusicWriter_RAW::MusicWriter_RAW(ostream_sptr output)
	throw () :
		MusicWriter_GenericOPL(DelayIsPreData),
		output(output),
		lastChipIndex(0),
		firstClock(0xFFFF),
		lastClock(0xFFFF)
{
}

MusicWriter_RAW::~MusicWriter_RAW()
	throw ()
{
}

void MusicWriter_RAW::start()
	throw (std::ios::failure)
{
	this->output->write("RAWADATA\xFF\xFF", 10);
	return;
}

void MusicWriter_RAW::finish()
	throw (std::ios::failure)
{
	this->MusicWriter_GenericOPL::finish();

	// Write out the EOF marker
	this->output << u8(0xFF) << u8(0xFF);

	// TODO: write out metadata here

	this->output->seekp(8, std::ios::beg);
	this->output
		<< u16le(this->firstClock)
	;
	return;
}

void MusicWriter_RAW::changeSpeed(uint32_t usPerTick)
	throw ()
{
	uint16_t clock = usPerTick * 1.192180;
	if (this->lastClock != clock) {
		this->output
			<< u8(0x00) // clock change
			<< u8(0x02) // control data
			<< u16le(clock)
		;
		if (this->firstClock == 0xFFFF) this->firstClock = clock;
		this->lastClock = clock;
	}
	return;
}

void MusicWriter_RAW::nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
	throw (std::ios::failure)
{
	// Write out the delay in one or more lots of 255 or less.
	while (delay > 0) {
		uint8_t d = (delay > 255) ? 255 : delay;
		this->output
			<< u8(d) // delay value
			<< u8(0) // delay command
		;
		delay -= d;
	}

	if (chipIndex != this->lastChipIndex) {
		assert(chipIndex < 2);
		this->output
			<< u8(0x01 + chipIndex) // 0x01 = chip 0, 0x02 = chip 1
			<< u8(0x02) // control command
		;
		this->lastChipIndex = chipIndex;
	}
	// Write out the reg/data if it's not one of the control structures.  If it
	// is we have to drop the pair as there's no way of escaping these values.
	if ((reg != 0x00) || (reg != 0x02)) {
		this->output
			<< u8(val)
			<< u8(reg)
		;
	} else {
		std::cout << "Warning: Rdos RAW cannot store writes to OPL register "
			<< reg << " so this value has been lost." << std::endl;
	}
	return;
}

// TODO: metadata functions
