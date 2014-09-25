/**
 * @file   mus-cdfm-zone66-gus.hpp
 * @brief  Support for Renaissance's CDFM format used in Zone 66 (GUS variant).
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_CDFM_GUS_ZONE66_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_CDFM_GUS_ZONE66_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/util-opl.hpp>

namespace camoto {
namespace gamemusic {

/// MusicType implementation for CDFM files.
class MusicType_CDFM_GUS: virtual public MusicType
{
	public:
		virtual std::string getCode() const;
		virtual std::string getFriendlyName() const;
		virtual std::vector<std::string> getFileExtensions() const;
		virtual unsigned long getCaps() const;
		virtual Certainty isInstance(stream::input_sptr input) const;
		virtual MusicPtr read(stream::input_sptr input, SuppData& suppData) const;
		virtual void write(stream::output_sptr output, SuppData& suppData,
			MusicPtr music, unsigned int flags) const;
		virtual SuppFilenames getRequiredSupps(stream::input_sptr input,
			const std::string& filenameMusic) const;
		virtual Metadata::MetadataTypes getMetadataList() const;
};

// This function is here so the test code can get to it
/// Convert a volume value (0..15) to a libgamemusic linear velocity (0..255)
inline unsigned int z66_volume_to_velocity(unsigned int vol)
{
	return log_volume_to_lin_velocity(vol, 15);
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_CDFM_GUS_ZONE66_HPP_
