/**
 * @file   mus-mid-type0.cpp
 * @brief  MusicReader and MusicWriter classes for single-track MIDI files.
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

#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/iostream_helpers.hpp>
#include "mus-mid-type0.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

std::string MusicType_MID_Type0::getCode() const
	throw ()
{
	return "mid-type0";
}

std::string MusicType_MID_Type0::getFriendlyName() const
	throw ()
{
	return "Standard MIDI File (type-0/single track)";
}

std::vector<std::string> MusicType_MID_Type0::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("mid");
	return vcExtensions;
}

E_CERTAINTY MusicType_MID_Type0::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	// Make sure the signature matches
	// TESTED BY: mus_mid_type0_isinstance_c01
	char sig[4];
	psMusic->seekg(0, std::ios::beg);
	psMusic->read(sig, 4);
	if (strncmp(sig, "MThd", 4) != 0) return EC_DEFINITELY_NO;

	// Skip over the length field
	psMusic->seekg(4, std::ios::cur);

	// Make sure the header says it's a type-0 file
	// TESTED BY: mus_mid_type0_isinstance_c02 (wrong type)
	uint16_t type;
	psMusic >> u16be(type);
	if (type != 0) return EC_DEFINITELY_NO;

	// TESTED BY: mus_mid_type0_isinstance_c00
	return EC_DEFINITELY_YES;
}

MusicWriterPtr MusicType_MID_Type0::create(ostream_sptr output, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_MID_Type0(output));
}

MusicReaderPtr MusicType_MID_Type0::open(istream_sptr input, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_MID_Type0(input));
}

SuppFilenames MusicType_MID_Type0::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}


MusicReader_MID_Type0::MusicReader_MID_Type0(istream_sptr input)
	throw (std::ios::failure) :
		MusicReader_GenericMIDI(MIDIFlags::Default),
		input(input)
{
	// Skip MThd header.
	this->input->seekg(4, std::ios::beg);

	uint32_t len;
	uint16_t type, numTracks, ticksPerQuarter;
	this->input
		>> u32be(len)
		>> u16be(type)
		>> u16be(numTracks)
		>> u16be(ticksPerQuarter)
	;

	// Skip over any remaining data in the MThd block (should be none)
	this->input->seekg(len - 6, std::ios::cur);

	this->setTicksPerQuarterNote(ticksPerQuarter);
	//this->setusPerQuarterNote(ticksPerQuarter * 1000000 / ticksPerSecond);

	// Read the MTrk header
	this->input->seekg(4, std::ios::cur); // skip "MTrk", assume it's fine
	uint32_t lenData;
	this->input >> u32be(lenData);

	this->offMusic = 16 + len;

	// Pass the MIDI data on to the parent
	assert(this->input->tellg() == this->offMusic);

	///< @todo Clip input to lenData
	this->setMIDIStream(input);
}

MusicReader_MID_Type0::~MusicReader_MID_Type0()
	throw ()
{
}

void MusicReader_MID_Type0::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	this->input->seekg(this->offMusic, std::ios::beg);
	return;
}


MusicWriter_MID_Type0::MusicWriter_MID_Type0(ostream_sptr output)
	throw (std::ios::failure) :
		MusicWriter_GenericMIDI(MIDIFlags::Default),
		output(output)
{
}

MusicWriter_MID_Type0::~MusicWriter_MID_Type0()
	throw ()
{
}

void MusicWriter_MID_Type0::start()
	throw (std::ios::failure)
{
	this->output->write(
		"MThd"
		"\x00\x00\x00\x06" // MThd block length (BE)
		"\x00\x00" // type-0
		"\x00\x01" // one track
		"\x00\x00" // placeholder for ticks per quarter note
		"MTrk"
		"\x00\x00\x00\x00" // MTrk block length placeholder
	, 22);

	this->setMIDIStream(output);
	return;
}

void MusicWriter_MID_Type0::finish()
	throw (std::ios::failure)
{
	this->MusicWriter_GenericMIDI::finish();

	uint32_t mtrkLen = this->output->tellp();
	mtrkLen -= 22; // 22 == header from start()

	uint16_t ticksPerQuarter = this->getTicksPerQuarterNote();
	this->output->seekp(12, std::ios::beg);
	this->output << u16be(ticksPerQuarter);

	this->output->seekp(18, std::ios::beg);
	this->output << u32be(mtrkLen);
	return;
}
