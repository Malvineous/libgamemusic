/**
 * @file  metadata-malv.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_METADATA_MALV_HPP_
#define _CAMOTO_GAMEMUSIC_METADATA_MALV_HPP_

#include <camoto/attribute.hpp>
#include <camoto/stream.hpp>
#include <camoto/gamemusic/musictype.hpp>

namespace camoto {
namespace gamemusic {

/// Read tags from the stream in Malvineous' tag format.
/**
 * @param input
 *   Stream to read tags from.
 *
 * @param music
 *   Music instance to append tags to.
 *
 * @throw stream::error
 *   If there was an error reading from the stream, such as invalid or truncated
 *   tags.
 */
void readMalvMetadata(stream::input& content, Music* music);

/// Write tags to the stream in Malvineous' tag format.
/**
 * @param output
 *   Stream to write tags to.
 *
 * @param attributes
 *   Vector to get tags from.  Tags are looked up by name rather than index.
 *
 * @throw stream::error
 *   If there was an error writing to the stream, such as insufficient disk
 *   space.
 *
 * @throw format_limitation
 *   If the tags were too long to be written.
 */
void writeMalvMetadata(stream::output& content,
	const std::vector<Attribute>& attributes);

/// Return available fields for Malvineous' tag format.
std::vector<Attribute> supportedMalvMetadata();

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_METADATA_MALV_HPP_
