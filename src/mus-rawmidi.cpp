/**
 * @file   mus-rawmidi.cpp
 * @brief  Support for standard MIDI data without any header.
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
#include "mus-rawmidi.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

std::string MusicType_RawMIDI::getCode() const
	throw ()
{
	return "rawmidi";
}

std::string MusicType_RawMIDI::getFriendlyName() const
	throw ()
{
	return "Raw MIDI data";
}

std::vector<std::string> MusicType_RawMIDI::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	return vcExtensions;
}


MusicType::Certainty MusicType_RawMIDI::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	/// @todo Try to read the data

	// TESTED BY: mus_raw_rdos_isinstance_c00
	return MusicType::DefinitelyNo;
}

MusicWriterPtr MusicType_RawMIDI::create(ostream_sptr output, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_RawMIDI(output));
}

MusicReaderPtr MusicType_RawMIDI::open(istream_sptr input, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_RawMIDI(input));
}

SuppFilenames MusicType_RawMIDI::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}

MusicReader_RawMIDI::MusicReader_RawMIDI(istream_sptr input)
	throw (std::ios::failure) :
		MusicReader_GenericMIDI(MIDIFlags::ShortAftertouch), // TEMP for Vinyl
		input(input)
{
	this->setMIDIStream(input);
}

MusicReader_RawMIDI::~MusicReader_RawMIDI()
	throw ()
{
}

void MusicReader_RawMIDI::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	this->input->seekg(0, std::ios::beg);
	return;
}



MusicWriter_RawMIDI::MusicWriter_RawMIDI(ostream_sptr output)
	throw () :
		MusicWriter_GenericMIDI(MIDIFlags::Default)
{
	this->setMIDIStream(output);
}

MusicWriter_RawMIDI::~MusicWriter_RawMIDI()
	throw ()
{
}

void MusicWriter_RawMIDI::start()
	throw (std::ios::failure)
{
	return;
}
