/**
 * @file   music.cpp
 * @brief  Declaration of top-level Music class, for accessing files
 *         storing music data.
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

#include <camoto/gamemusic/music.hpp>

using namespace camoto::gamemusic;

MusicReader::MusicReader()
	throw ()
{
}

MusicReader::~MusicReader()
	throw ()
{
}

VC_METADATA_ITEMS MusicReader::getMetadataList() const
	throw ()
{
	return VC_METADATA_ITEMS();
}

std::string MusicReader::getMetadata(E_METADATA item) const
	throw (std::ios::failure)
{
	// This should never be called because getMetadataList() returned an empty
	// list.
	assert(false);
	throw std::ios::failure("unsupported metadata item");
}

MusicWriter::MusicWriter()
	throw () :
		flags(Default)
{
}

MusicWriter::~MusicWriter()
	throw ()
{
}

void MusicWriter::setFlags(Flags f)
	throw ()
{
	this->flags = f;
	return;
}

void MusicWriter::setMetadata(E_METADATA item, const std::string& value)
	throw (std::ios::failure)
{
	// This should never be called because getMetadataList() returned an empty
	// list.
	assert(false);
	throw std::ios::failure("unsupported metadata item");
}
