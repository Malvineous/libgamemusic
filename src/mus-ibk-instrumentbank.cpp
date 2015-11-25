/**
 * @file  mus-ibk-instrumentbank.cpp
 * @brief Support for .IBK (Instrument Bank) files.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/IBK_Format
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
#include "mus-ibk-instrumentbank.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Length of whole .ibk file
const unsigned int IBK_LENGTH = 4 + 128 * (16 + 9);

/// Number of instruments in IBK
const unsigned int IBK_INST_COUNT = 128;

/// Length of each instrument, in bytes
const unsigned int IBK_INST_LEN = 16;

/// Length of each instrument title, in bytes
const unsigned int IBK_NAME_LEN = 9;

std::string MusicType_IBK::code() const
{
	return "ibk-instrumentbank";
}

std::string MusicType_IBK::friendlyName() const
{
	return "IBK Instrument Bank";
}

std::vector<std::string> MusicType_IBK::fileExtensions() const
{
	return {
		"ibk",
	};
}

MusicType::Caps MusicType_IBK::caps() const
{
	return
		Caps::InstOPL
	;
}

MusicType::Certainty MusicType_IBK::isInstance(stream::input& content) const
{
	// Make sure the signature matches
	// TESTED BY: mus_ibk_isinstance_c01
	char sig[4];
	content.seekg(0, stream::start);
	content.read(sig, 4);
	if (strncmp(sig, "IBK\x1A", 4) != 0) return Certainty::DefinitelyNo;

	// All files are the same size
	// TESTED BY: mus_ibk_isinstance_c02
	if (content.size() != IBK_LENGTH) return Certainty::DefinitelyNo;

	// TESTED BY: mus_ibk_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_IBK::read(stream::input& content,
	SuppData& suppData) const
{
	auto music = std::make_unique<Music>();
	music->patches.reset(new PatchBank());

	uint8_t names[IBK_INST_COUNT * IBK_NAME_LEN];
	content.seekg(4 + IBK_INST_COUNT * IBK_INST_LEN, stream::start);
	content.read(names, IBK_INST_COUNT * IBK_NAME_LEN);

	// Read the instruments
	music->patches->reserve(IBK_INST_COUNT);
	content.seekg(4, stream::start);
	for (unsigned int i = 0; i < IBK_INST_COUNT; i++) {
		auto patch = std::make_shared<OPLPatch>();
		uint8_t inst[IBK_INST_LEN];
		content.read((char *)inst, IBK_INST_LEN);

		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = (inst[0 + op] >> 7) & 1;
			o->enableVibrato = (inst[0 + op] >> 6) & 1;
			o->enableSustain = (inst[0 + op] >> 5) & 1;
			o->enableKSR     = (inst[0 + op] >> 4) & 1;
			o->freqMult      =  inst[0 + op] & 0x0F;
			o->scaleLevel    =  inst[2 + op] >> 6;
			o->outputLevel   =  inst[2 + op] & 0x3F;
			o->attackRate    =  inst[4 + op] >> 4;
			o->decayRate     =  inst[4 + op] & 0x0F;
			o->sustainRate   =  inst[6 + op] >> 4;
			o->releaseRate   =  inst[6 + op] & 0x0F;
			o->waveSelect    =  inst[8 + op] & 0x07;
			o = &patch->c;
		}
		patch->feedback    = (inst[10] >> 1) & 0x07;
		patch->connection  =  inst[10] & 1;
		patch->rhythm      = OPLPatch::Rhythm::Melodic;

		uint8_t *name = &names[i * IBK_NAME_LEN];
		unsigned int namelen = 9;
		for (int l = 0; l < 9; l++) {
			if (name[l] == 0) {
				namelen = l;
				break;
			}
		}
		patch->name = std::string((char *)name, namelen);

		music->patches->push_back(patch);
	}

	return music;
}

void MusicType_IBK::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	requirePatches<OPLPatch>(*music.patches);
	if (music.patches->size() >= IBK_INST_COUNT) {
		throw bad_patch("IBK files have a maximum of 128 instruments.");
	}

	content.write("IBK\x1A", 4);

	uint8_t names[IBK_INST_COUNT * IBK_NAME_LEN];
	memset(names, 0, sizeof(names));

	for (unsigned int i = 0; i < IBK_INST_COUNT; i++) {
		uint8_t inst[16];
		auto patch = dynamic_cast<OPLPatch*>(music.patches->at(i).get());
		assert(patch);
		auto *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			inst[0 + op] =
				((o->enableTremolo & 1) << 7) |
				((o->enableVibrato & 1) << 6) |
				((o->enableSustain & 1) << 5) |
				((o->enableKSR     & 1) << 4) |
				 (o->freqMult      & 0x0F)
			;
			inst[2 + op] = (o->scaleLevel << 6) | (o->outputLevel & 0x3F);
			inst[4 + op] = (o->attackRate << 4) | (o->decayRate & 0x0F);
			inst[6 + op] = (o->sustainRate << 4) | (o->releaseRate & 0x0F);
			inst[8 + op] =  o->waveSelect & 7;

			o = &patch->c;
		}
		inst[10] = ((patch->feedback & 7) << 1) | (patch->connection & 1);
		inst[11] = inst[12] = inst[13] = inst[14] = inst[15] = 0;

		/// @todo handle these
		//patch->deepTremolo = false;
		//patch->deepVibrato = false;

		content.write((char *)inst, 16);

		strncpy((char *)&names[i * IBK_NAME_LEN], patch->name.c_str(), 9);
	}

	content.write(names, IBK_INST_COUNT * IBK_NAME_LEN);

	// Set final filesize to this
	content.truncate_here();

	return;
}

SuppFilenames MusicType_IBK::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_IBK::supportedAttributes() const
{
	return {};
}
