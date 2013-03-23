/**
 * @file   mus-mid-type0.cpp
 * @brief  Support for Type-0 (single track) MIDI files.
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
#include <camoto/gamemusic/patch-midi.hpp>
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "mus-mid-type0.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

std::string MusicType_MID_Type0::getCode() const
{
	return "mid-type0";
}

std::string MusicType_MID_Type0::getFriendlyName() const
{
	return "Standard MIDI File (type-0/single track)";
}

std::vector<std::string> MusicType_MID_Type0::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("mid");
	return vcExtensions;
}

MusicType::Certainty MusicType_MID_Type0::isInstance(stream::input_sptr psMusic) const
{
	// Make sure the signature matches
	// TESTED BY: mus_mid_type0_isinstance_c01
	char sig[4];
	psMusic->seekg(0, stream::start);
	psMusic->read(sig, 4);
	if (strncmp(sig, "MThd", 4) != 0) return MusicType::DefinitelyNo;

	// Skip over the length field
	psMusic->seekg(4, stream::cur);

	// Make sure the header says it's a type-0 file
	// TESTED BY: mus_mid_type0_isinstance_c02 (wrong type)
	uint16_t type;
	psMusic >> u16be(type);
	if (type != 0) return MusicType::DefinitelyNo;

	// TESTED BY: mus_mid_type0_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_MID_Type0::read(stream::input_sptr input, SuppData& suppData) const
{
	// Skip MThd header.
	input->seekg(4, stream::start);

	uint32_t len;
	uint16_t type, numTracks, ticksPerQuarter;
	input
		>> u32be(len)
		>> u16be(type)
		>> u16be(numTracks)
		>> u16be(ticksPerQuarter)
	;

	// Skip over any remaining data in the MThd block (should be none)
	input->seekg(len - 6, stream::cur);

	// Read the MTrk header
	input->seekg(4, stream::cur); // skip "MTrk", assume it's fine
	uint32_t lenData;
	input >> u32be(lenData);

	/// @todo Clip input to lenData

	MusicPtr music = midiDecode(input, MIDIFlags::Default, ticksPerQuarter,
		MIDI_DEF_uS_PER_QUARTER_NOTE);

	return music;
}

void MusicType_MID_Type0::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	requirePatches<MIDIPatch>(music->patches);

	output->write(
		"MThd"
		"\x00\x00\x00\x06" // MThd block length (BE)
		"\x00\x00" // type-0
		"\x00\x01" // one track
		"\x00\x00" // placeholder for ticks per quarter note
		"MTrk"
		"\x00\x00\x00\x00" // MTrk block length placeholder
	, 22);

	tempo_t usPerTick;
	midiEncode(output, MIDIFlags::Default, music, &usPerTick, NULL);

	uint32_t mtrkLen = output->tellp();
	mtrkLen -= 22; // 22 == header from start()

	output->seekp(12, stream::start);
	output << u16be(music->ticksPerQuarterNote);

	output->seekp(18, stream::start);
	output << u32be(mtrkLen);
}

SuppFilenames MusicType_MID_Type0::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_MID_Type0::getMetadataList() const
{
	Metadata::MetadataTypes types;
	return types;
}
