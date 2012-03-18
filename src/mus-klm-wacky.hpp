/**
 * @file   mus-klm-wacky.hpp
 * @brief  Support for Wacky Wheels Adlib (.klm) format.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_KLM_WACKY_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_KLM_WACKY_HPP_

#include <camoto/gamemusic/musictype.hpp>

namespace camoto {
namespace gamemusic {

/// MusicType implementation for Wacky Wheels KLM files.
class MusicType_KLM: virtual public MusicType
{
	public:
		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();

		virtual Certainty isInstance(stream::input_sptr psMusic) const
			throw (stream::error);

		virtual MusicPtr read(stream::input_sptr input, SuppData& suppData) const
			throw (stream::error);

		virtual void write(stream::output_sptr output, SuppData& suppData,
			MusicPtr music, unsigned int flags) const
			throw (stream::error, format_limitation);

		virtual SuppFilenames getRequiredSupps(stream::input_sptr input,
			const std::string& filenameMusic) const
			throw ();

		virtual Metadata::MetadataTypes getMetadataList() const
			throw ();
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_KLM_WACKY_HPP_
