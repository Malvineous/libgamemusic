/**
 * @file   mus-imf-idsoftware.cpp
 * @brief  Support for id Software's .IMF format.
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
#include "mus-imf-idsoftware.hpp"

// Any files with delays longer than this value will be reported as not
// being IMF files.  This could be over cautious, but there are unlikely
// to be any real songs with delays this long.
#define IMF_MAX_DELAY  0x4000

using namespace camoto::gamemusic;

std::vector<std::string> MusicType_IMF_Common::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("imf");
	vcExtensions.push_back("wlf");
	vcExtensions.push_back("mni");
	return vcExtensions;
}

E_CERTAINTY MusicType_IMF_Common::genericIsInstance(istream_sptr psMusic, int imfType) const
	throw (std::ios::failure)
{
	psMusic->seekg(0, std::ios::end);
	io::stream_offset len = psMusic->tellg();

	// TESTED BY: mus_imf_idsoftware_type*_isinstance_c01
	if (len < 2) return EC_DEFINITELY_NO; // too short

	// Read the first two bytes as the data length size and make sure they
	// don't point past the end of the file.
	// TESTED BY: mus_imf_idsoftware_type1_isinstance_c05
	psMusic->seekg(0, std::ios::beg);
	uint16_t dataLen;
	psMusic >> u16le(dataLen);
	if (dataLen > len) return EC_DEFINITELY_NO;

	if (dataLen == 0) { // type-0 format
		// Jump back so these bytes are counted as the first event
		psMusic->seekg(0, std::ios::beg);
		// Only type-0 files start like this
		// TESTED BY: mus_imf_idsoftware_type0_isinstance_c04
		if (imfType != 0) return EC_DEFINITELY_NO;
		dataLen = len;
	} else { // type-1 format
		// TESTED BY: mus_imf_idsoftware_type1_isinstance_c04
		if (imfType != 1) return EC_DEFINITELY_NO;
	}

	// TODO: Parse file and check for invalid register writes.
	uint16_t delay; uint8_t reg, val;
	while (dataLen > 3) {
		// When dataLen < 4, TESTED BY: mus_imf_idsoftware_type1_isinstance_c06

		// Read in the next reg/data pair
		psMusic
			>> u8(reg)
			>> u8(val)
			>> u16le(delay)
		;
		// Make sure it points to a valid OPL register
		if (
			(reg == 0x06)
			|| (reg == 0x07)
			|| ((reg >= 0x09) && (reg <= 0x1F))
			|| ((reg >= 0x36) && (reg <= 0x3F))
			|| ((reg >= 0x56) && (reg <= 0x5F))
			|| ((reg >= 0x76) && (reg <= 0x7F))
			|| ((reg >= 0x96) && (reg <= 0x9F))
			|| ((reg >= 0xA9) && (reg <= 0xAF))
			|| ((reg >= 0xB9) && (reg <= 0xBC))
			|| ((reg >= 0xBE) && (reg <= 0xBF))
			|| ((reg >= 0xC9) && (reg <= 0xDF))
			|| ((reg >= 0xF6) && (reg <= 0xFF))
		) {
			// This is an invalid OPL register
			// TESTED BY: mus_imf_idsoftware_type*_isinstance_c02
			return EC_DEFINITELY_NO;
		}

		// Very unlikely that a real song would have a lengthy delay in it...
		// TESTED BY: mus_imf_idsoftware_type*_isinstance_c03
		if (delay > IMF_MAX_DELAY) return EC_DEFINITELY_NO;

		dataLen -= 4;
	}

	// TESTED BY: mus_imf_idsoftware_isinstance_c00
	return EC_DEFINITELY_YES;
}

MP_SUPPLIST MusicType_IMF_Common::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return MP_SUPPLIST();
}


std::string MusicType_IMF_Type0::getCode() const
	throw ()
{
	return "imf-idsoftware-type0";
}

std::string MusicType_IMF_Type0::getFriendlyName() const
	throw ()
{
	return "id Software Music Format (type-0)";
}

E_CERTAINTY MusicType_IMF_Type0::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	return this->genericIsInstance(psMusic, 0);
}

MusicWriterPtr MusicType_IMF_Type0::create(ostream_sptr output, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_IMF(output, 0, 560)); // TODO: 280/560/700
}

MusicReaderPtr MusicType_IMF_Type0::open(istream_sptr input, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_IMF(input, 0));
}

std::string MusicType_IMF_Type1::getCode() const
	throw ()
{
	return "imf-idsoftware-type1";
}

std::string MusicType_IMF_Type1::getFriendlyName() const
	throw ()
{
	return "id Software Music Format (type-1)";
}

E_CERTAINTY MusicType_IMF_Type1::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	return this->genericIsInstance(psMusic, 1);
}

MusicWriterPtr MusicType_IMF_Type1::create(ostream_sptr output, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_IMF(output, 1, 560)); // TODO: 280/560/700
}

MusicReaderPtr MusicType_IMF_Type1::open(istream_sptr input, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_IMF(input, 1));
}

MusicReader_IMF::MusicReader_IMF(istream_sptr input, int imfType)
	throw (std::ios::failure) :
		MusicReader_GenericOPL(DelayIsPostData),
		input(input),
		imfType(imfType)
{
	this->input->seekg(0, std::ios::beg);
	if (imfType == 1) {
		this->input >> u16le(this->lenData);
	} else {
		this->lenData = 0; // read until EOF
	}
	this->changeSpeed(1000000/560); // 560Hz (TODO: 700Hz)
}

MusicReader_IMF::~MusicReader_IMF()
	throw ()
{
}

void MusicReader_IMF::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	if (imfType == 1) {
		this->input->seekg(2, std::ios::beg);
	} else {
		this->input->seekg(0, std::ios::beg);
	}
	return;
}

bool MusicReader_IMF::nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg, uint8_t *val)
	throw (std::ios::failure)
{
	try {
		*chipIndex = 0; // IMF only supports one OPL2
		this->input
			>> u8(*reg)
			>> u8(*val)
			>> u16le(*delay)
		;
	} catch (std::ios::failure) {
		if (this->input->eof()) return false;
		throw;
	}
	return !this->input->eof();
}

// TODO: metadata functions


MusicWriter_IMF::MusicWriter_IMF(ostream_sptr output, int imfType, int hzSpeed)
	throw () :
		MusicWriter_GenericOPL(DelayIsPostData),
		output(output),
		imfType(imfType),
		usPerTick(0),
		hzSpeed(hzSpeed)
{
}

MusicWriter_IMF::~MusicWriter_IMF()
	throw ()
{
}

void MusicWriter_IMF::start()
	throw (std::ios::failure)
{
	if (this->imfType == 1) {
		// Write a placeholder for the file size, which will be overwritten later
		this->output << u16le(0);
	}
	this->output << u32le(0);
	return;
}

void MusicWriter_IMF::finish()
	throw (std::ios::failure)
{
	this->MusicWriter_GenericOPL::finish();

	if (this->imfType == 1) {
		// Update the placeholder we wrote in the constructor with the file size
		int size = this->output->tellp();
		size -= 2; // don't count the field itself
		// TODO: write out metadata here
		this->output->seekp(0, std::ios::beg);
		this->output << u16le(size);
	}
	return;
}

void MusicWriter_IMF::changeSpeed(uint32_t usPerTick)
	throw ()
{
	this->usPerTick = usPerTick;
	return;
}

void MusicWriter_IMF::nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
	throw (std::ios::failure)
{
	assert(chipIndex == 0);  // make sure the format capabilities are being honoured
	assert(this->usPerTick > 0); // make sure this has been set already

	// Convert ticks into an IMF delay
	delay = delay * this->usPerTick / (1000000/this->hzSpeed);

	this->output
		<< u8(reg)
		<< u8(val)
		<< u16le(delay)
	;
	return;
}

// TODO: metadata functions
