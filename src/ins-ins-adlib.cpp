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

#include <camoto/gamemusic/patch-opl.hpp>
#include "patch-adlib.hpp"
#include "ins-ins-adlib.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Length of instrument title, in bytes
const unsigned int INS_TITLE_LEN = 20;

std::string MusicType_INS_AdLib::getCode() const
{
	return "ins-adlib";
}

std::string MusicType_INS_AdLib::getFriendlyName() const
{
	return "Ad Lib INS instrument";
}

std::vector<std::string> MusicType_INS_AdLib::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("ins");
	return vcExtensions;
}

unsigned long MusicType_INS_AdLib::getCaps() const
{
	return
		InstOPL
	;
}

MusicType::Certainty MusicType_INS_AdLib::isInstance(stream::input_sptr psMusic) const
{
	stream::len lenFile = psMusic->size();
	psMusic->seekg(0, stream::start);

	// Unknown length
	// TESTED BY: ins_ins_adlib_isinstance_c01
	if (
		(lenFile != 54)
		&& (lenFile != 78)
		&& (lenFile != 80)
	) return MusicType::DefinitelyNo;

	for (unsigned int i = 0; i < 14; i++) {
		uint16_t next;
		psMusic >> u16le(next);
		// Out of range value
		// TESTED BY: ins_ins_adlib_isinstance_c02
		if (next > 255) return MusicType::DefinitelyNo;
	}

	// TESTED BY: ins_ins_adlib_isinstance_c00
	return MusicType::PossiblyYes;
}

MusicPtr MusicType_INS_AdLib::read(stream::input_sptr input, SuppData& suppData) const
{
	stream::len lenFile = input->size();
	input->seekg(0, stream::start);
	MusicPtr music(new Music());
	music->patches.reset(new PatchBank());

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
	input >> u16le(unknown);

	OPLPatchPtr oplPatch(new OPLPatch());
	readAdLibOperator<uint16_t>(input, &oplPatch->m, &oplPatch->feedback, &oplPatch->connection);
	if (op > 1) {
		readAdLibOperator<uint16_t>(input, &oplPatch->c, NULL, NULL);
	}

	std::string title;
	input >> nullPadded(title, INS_TITLE_LEN);
	if (!title.empty()) music->metadata[Metadata::Title] = title;

	if (wavesel) {
		readAdLibWaveSel<uint16_t>(input, &oplPatch->m);
		if (op > 1) {
			readAdLibWaveSel<uint16_t>(input, &oplPatch->c);
		}
	}
	music->patches->push_back(oplPatch);

	return music;
}

void MusicType_INS_AdLib::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	requirePatches<OPLPatch>(music->patches);
	if (music->patches->size() != 1) {
		throw bad_patch("AdLib INS files can only have exactly one instrument.");
	}

	OPLPatchPtr oplPatch = boost::dynamic_pointer_cast<OPLPatch>(music->patches->at(0));

	uint16_t unknown = 0;
	output << u16le(unknown);

	writeAdLibOperator<uint16_t>(output, oplPatch->m, oplPatch->feedback, oplPatch->connection);
	writeAdLibOperator<uint16_t>(output, oplPatch->c, oplPatch->feedback, oplPatch->connection);

	output << nullPadded(music->metadata[Metadata::Title], INS_TITLE_LEN);

	writeAdLibWaveSel<uint16_t>(output, oplPatch->m);
	writeAdLibWaveSel<uint16_t>(output, oplPatch->c);

	uint16_t unknown2 = 1;
	output << u16le(unknown2);

	// Set final filesize to this
	output->truncate_here();

	return;
}

SuppFilenames MusicType_INS_AdLib::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_INS_AdLib::getMetadataList() const
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	return types;
}
