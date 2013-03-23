/**
 * @file   metadata-malv.cpp
 * @brief  Read/write functions for file tags in Malvineous' tag format.
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
#include "metadata-malv.hpp"

namespace camoto {
namespace gamemusic {

void readMalvMetadata(stream::input_sptr& input, Metadata::TypeMap& metadata)
{
	uint8_t sig;
	try {
		input >> u8(sig);
	} catch (stream::error) {
		// No (or invalid) tags.  Note that some tags may have been set though.
		return;
	}
	if (sig == 0x1A) {
		input >> nullTerminated(metadata[Metadata::Title], 256);
		input >> nullTerminated(metadata[Metadata::Author], 256);
		input >> nullTerminated(metadata[Metadata::Description], 256);
	}
	return;
}

void writeMalvMetadata(stream::output_sptr& output, const Metadata::TypeMap& metadata)
{
	Metadata::TypeMap::const_iterator itTitle = metadata.find(Metadata::Title);
	Metadata::TypeMap::const_iterator itComposer = metadata.find(Metadata::Author);
	Metadata::TypeMap::const_iterator itDesc = metadata.find(Metadata::Description);
	if (
		(itTitle != metadata.end()) ||
		(itComposer != metadata.end()) ||
		(itDesc != metadata.end())
	) {
		output << u8(0x1A);
		if (itTitle != metadata.end()) {
			if (itTitle->second.length() > 255) {
				throw format_limitation("The metadata element 'Title' is longer than "
					"the maximum 255 character length.");
			}
			output << nullTerminated(itTitle->second, 256);
		} else {
			output->write("\0", 1); // empty field
		}

		if (itComposer != metadata.end()) {
			if (itComposer->second.length() > 255) {
				throw format_limitation("The metadata element 'Author' is longer than "
					"the maximum 255 character length.");
			}
			output << nullTerminated(itComposer->second, 256);
		} else {
			output->write("\0", 1); // empty field
		}

		if (itDesc != metadata.end()) {
			if (itDesc->second.length() > 255) {
				throw format_limitation("The metadata element 'Desc' is longer than "
					"the maximum 255 character length.");
			}
			output << nullTerminated(itDesc->second, 256);
		} else {
			output->write("\0", 1); // empty field
		}

		// Program name
		output->write("Camoto\0\0\0", 9);
	}

	return;
}

} // namespace gamemusic
} // namespace camoto
