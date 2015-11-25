/**
 * @file  ins-ins-adlib.hpp
 * @brief Support for Ad Lib .INS instruments.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/AdLib_Instrument_Format
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

#include <camoto/util.hpp> // make_unique
#include <camoto/gamemusic/patch-opl.hpp>
#include "patch-adlib.hpp"
#include "ins-ins-adlib.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Length of instrument title, in bytes.
const unsigned int INS_TITLE_LEN = 20;

std::string MusicType_INS_AdLib::code() const
{
	return "ins-adlib";
}

std::string MusicType_INS_AdLib::friendlyName() const
{
	return "Ad Lib INS instrument";
}

std::vector<std::string> MusicType_INS_AdLib::fileExtensions() const
{
	return {
		"ins",
	};
}

MusicType::Caps MusicType_INS_AdLib::caps() const
{
	return
		Caps::InstOPL
	;
}

MusicType::Certainty MusicType_INS_AdLib::isInstance(stream::input& content) const
{
	stream::len lenFile = content.size();
	content.seekg(0, stream::start);

	// Unknown length
	// TESTED BY: ins_ins_adlib_isinstance_c01
	if (
		(lenFile != 54)
		&& (lenFile != 78)
		&& (lenFile != 80)
	) return Certainty::DefinitelyNo;

	for (unsigned int i = 0; i < 14; i++) {
		uint16_t next;
		content >> u16le(next);
		// Out of range value
		// TESTED BY: ins_ins_adlib_isinstance_c02
		if (next > 255) return Certainty::DefinitelyNo;
	}

	// TESTED BY: ins_ins_adlib_isinstance_c00
	return Certainty::PossiblyYes;
}

std::unique_ptr<Music> MusicType_INS_AdLib::read(stream::input& content,
	SuppData& suppData) const
{
	stream::len lenFile = content.size();
	content.seekg(0, stream::start);
	auto music = std::make_unique<Music>();
	music->patches = std::make_shared<PatchBank>();

	int op;
	bool wavesel;
	switch (lenFile) {
		case 54:
			op = 1;
			wavesel = true;
			break;
		case 78:
		case 80:
			op = 2;
			wavesel = true;
			break;
		default:
			std::cout << "[ins-adlib] Unknown format variant: length=" << lenFile
				<< std::endl;
			if (lenFile < 74) op = 1; else op = 2;
			wavesel = false;
			break;
	}

	uint16_t unknown;
	content >> u16le(unknown);

	auto oplPatch = std::make_shared<OPLPatch>();

	content >> adlibOperator<uint16_t>(oplPatch->m, oplPatch->feedback,
		oplPatch->connection);
	if (op > 1) {
		uint8_t dummyFeedback;
		bool dummyConnection;
		content >> adlibOperator<uint16_t>(oplPatch->c, dummyFeedback,
			dummyConnection);
	}

	// Add metadata item for title, and read content into it
	{
		auto& a = music->addAttribute();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = INS_TITLE_LEN;
		content >> nullPadded(a.textValue, INS_TITLE_LEN);
	}

	if (wavesel) {
		content >> u16le(oplPatch->m.waveSelect);
		if (op > 1) {
			content >> u16le(oplPatch->c.waveSelect);
		}
	}

	music->patches->push_back(oplPatch);

	return music;
}

void MusicType_INS_AdLib::write(stream::output& output, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	requirePatches<OPLPatch>(*music.patches);
	if (music.patches->size() != 1) {
		throw bad_patch("AdLib INS files can only have exactly one instrument.");
	}

	auto oplPatch = dynamic_cast<const OPLPatch *>(music.patches->at(0).get());

	uint16_t unknown = 0;
	output << u16le(unknown);

	output
		<< adlibOperator<uint16_t>(oplPatch->m, oplPatch->feedback, oplPatch->connection)
		<< adlibOperator<uint16_t>(oplPatch->c, oplPatch->feedback, oplPatch->connection)
		<< nullPadded(music.attributes().at(0).textValue, INS_TITLE_LEN)
		<< u16le(oplPatch->m.waveSelect)
		<< u16le(oplPatch->c.waveSelect)
	;

	uint16_t unknown2 = 1;
	output << u16le(unknown2);

	// Set final filesize to this
	output.truncate_here();

	return;
}

SuppFilenames MusicType_INS_AdLib::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_INS_AdLib::supportedAttributes() const
{
	std::vector<Attribute> items;
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = INS_TITLE_LEN;
	}
	return items;
}
