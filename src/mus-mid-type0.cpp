/**
 * @file  mus-mid-type0.cpp
 * @brief Support for Type-0 (single track) MIDI files.
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
#include <camoto/gamemusic/patch-midi.hpp>
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "mus-mid-type0.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

std::string MusicType_MID_Type0::code() const
{
	return "mid-type0";
}

std::string MusicType_MID_Type0::friendlyName() const
{
	return "Standard MIDI File (type-0/single track)";
}

std::vector<std::string> MusicType_MID_Type0::fileExtensions() const
{
	return {
		"mdi",
		"mid",
	};
}

MusicType::Caps MusicType_MID_Type0::caps() const
{
	return
		Caps::InstMIDI
		| Caps::HasEvents
	;
}

MusicType::Certainty MusicType_MID_Type0::isInstance(stream::input& content)
	const
{
	stream::len fileSize = content.size();
	if (fileSize < 10) {
		// File too short (header)
		// TESTED BY: mus_mid_type0_isinstance_c03
		return Certainty::DefinitelyNo;
	}

	// Make sure the signature matches
	// TESTED BY: mus_mid_type0_isinstance_c01
	char sig[4];
	content.seekg(0, stream::start);
	content.read(sig, 4);
	if (strncmp(sig, "MThd", 4) != 0) return Certainty::DefinitelyNo;

	// Skip over the length field
	content.seekg(4, stream::cur);

	// Make sure the header says it's a type-0 file
	// TESTED BY: mus_mid_type0_isinstance_c02 (wrong type)
	uint16_t type;
	content >> u16be(type);
	if (type != 0) return Certainty::DefinitelyNo;

	// TESTED BY: mus_mid_type0_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_MID_Type0::read(stream::input& content,
	SuppData& suppData) const
{
	// Skip MThd header.
	content.seekg(4, stream::start);

	uint32_t len;
	uint16_t type, numTracks, ticksPerQuarter;
	content
		>> u32be(len)
		>> u16be(type)
		>> u16be(numTracks)
		>> u16be(ticksPerQuarter)
	;

	// Skip over any remaining data in the MThd block (should be none)
	content.seekg(len - 6, stream::cur);

	// Read the MTrk header
	content.seekg(4, stream::cur); // skip "MTrk", assume it's fine
	uint32_t lenData;
	content >> u32be(lenData);

	/// @todo Clip content to lenData

	Tempo initialTempo;
	initialTempo.ticksPerQuarterNote(ticksPerQuarter);
	initialTempo.usPerQuarterNote(MIDI_DEF_uS_PER_QUARTER_NOTE);
	auto music = midiDecode(content, MIDIFlags::Default, initialTempo);

	return music;
}

void MusicType_MID_Type0::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	requirePatches<MIDIPatch>(*music.patches);

	content.write(
		"MThd"
		"\x00\x00\x00\x06" // MThd block length (BE)
		"\x00\x00" // type-0
		"\x00\x01" // one track
		"\x00\x00" // placeholder for ticks per quarter note
		"MTrk"
		"\x00\x00\x00\x00" // MTrk block length placeholder
	, 22);

	MIDIFlags midiFlags = MIDIFlags::EmbedTempo;
	if (flags & MusicType::WriteFlags::IntegerNotesOnly) {
		midiFlags |= MIDIFlags::IntegerNotesOnly;
	}

	midiEncode(content, music, midiFlags, NULL, EventHandler::Order_Row_Track,
		NULL);

	uint32_t mtrkLen = content.tellp();
	mtrkLen -= 22; // 22 == header from start()

	content.seekp(12, stream::start);
	content << u16be(music.initialTempo.ticksPerQuarterNote());

	content.seekp(18, stream::start);
	content << u32be(mtrkLen);
}

SuppFilenames MusicType_MID_Type0::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_MID_Type0::supportedAttributes() const
{
	return {};
}
