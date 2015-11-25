/**
 * @file  metadata-malv.cpp
 * @brief Read/write functions for file tags in Malvineous' tag format.
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
#include "metadata-malv.hpp"

/// Maximum length of each title/composer/comments field.
#define MM_FIELD_LEN 256

namespace camoto {
namespace gamemusic {

void readMalvMetadata(stream::input& content, Music* music)
{
	uint8_t sig;
	try {
		content >> u8(sig);
	} catch (stream::error) {
		// No (or invalid) tags.  Note that some tags may have been set though.
		return;
	}
	if (sig == 0x1A) {
		auto& attrTitle = music->addAttribute();
		attrTitle.changed = false;
		attrTitle.type = Attribute::Type::Text;
		attrTitle.name = CAMOTO_ATTRIBUTE_TITLE;
		attrTitle.desc = "Song title";
		attrTitle.textMaxLength = MM_FIELD_LEN;
		content >> nullTerminated(attrTitle.textValue, MM_FIELD_LEN);

		auto& attrArtist = music->addAttribute();
		attrArtist.changed = false;
		attrArtist.type = Attribute::Type::Text;
		attrArtist.name = CAMOTO_ATTRIBUTE_AUTHOR;
		attrArtist.desc = "Song composer/arranger/artist";
		attrArtist.textMaxLength = MM_FIELD_LEN;
		content >> nullTerminated(attrArtist.textValue, MM_FIELD_LEN);

		auto& attrComments = music->addAttribute();
		attrComments.changed = false;
		attrComments.type = Attribute::Type::Text;
		attrComments.name = CAMOTO_ATTRIBUTE_COMMENT;
		attrComments.desc = "Comments";
		attrComments.textMaxLength = MM_FIELD_LEN;
		content >> nullTerminated(attrComments.textValue, MM_FIELD_LEN);
	}
	return;
}

void writeMalvMetadata(stream::output& content,
	const std::vector<Attribute>& attributes)
{
	std::string strTitle, strComposer, strComments;
	for (auto& a : attributes) {
		if (a.name.compare(CAMOTO_ATTRIBUTE_TITLE) == 0) {
			strTitle = a.textValue;
		} else if (a.name.compare(CAMOTO_ATTRIBUTE_AUTHOR) == 0) {
			strComposer = a.textValue;
		} else if (a.name.compare(CAMOTO_ATTRIBUTE_COMMENT) == 0) {
			strComments = a.textValue;
		}
	}
	if (!strTitle.empty() || !strComposer.empty() || !strComments.empty()) {
		content
			<< u8(0x1A)
			<< nullTerminated(strTitle, MM_FIELD_LEN)
			<< nullTerminated(strComposer, MM_FIELD_LEN)
			<< nullTerminated(strComments, MM_FIELD_LEN)
			// Program name
			<< nullPadded("Camoto", 9)
		;
	}

	return;
}

std::vector<Attribute> supportedMalvMetadata()
{
	std::vector<Attribute> items;
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = MM_FIELD_LEN;
	}
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_AUTHOR;
		a.desc = "Song composer/arranger/artist";
		a.textMaxLength = MM_FIELD_LEN;
	}
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_COMMENT;
		a.desc = "Comments";
		a.textMaxLength = MM_FIELD_LEN;
	}
	return items;
}

} // namespace gamemusic
} // namespace camoto
