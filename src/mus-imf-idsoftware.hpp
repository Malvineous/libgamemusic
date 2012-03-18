/**
 * @file   mus-imf-idsoftware.hpp
 * @brief  Support for id Software's .IMF format.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_IMF_IDSOFTWARE_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_IMF_IDSOFTWARE_HPP_

#include <camoto/gamemusic/musictype.hpp>

namespace camoto {
namespace gamemusic {

/// Common IMF functions for all versions
class MusicType_IMF_Common: virtual public MusicType
{
	public:
		MusicType_IMF_Common(unsigned int imfType, unsigned int speed)
			throw ();

		virtual Certainty isInstance(stream::input_sptr input) const
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

	protected:
		unsigned int imfType;  ///< IMF format type; 0 or 1
		unsigned int speed;    ///< IMF speed in Hertz
};

/// IMF 560Hz Type-0 handler
class MusicType_IMF_Type0: virtual public MusicType_IMF_Common
{
	public:
		MusicType_IMF_Type0()
			throw ();

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();
};

/// IMF 560Hz Type-1 handler
class MusicType_IMF_Type1: virtual public MusicType_IMF_Common
{
	public:
		MusicType_IMF_Type1()
			throw ();

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();
};

/// WLF 700Hz Type-0 handler
class MusicType_WLF_Type0: virtual public MusicType_IMF_Common
{
	public:
		MusicType_WLF_Type0()
			throw ();

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();
};

/// WLF 700Hz Type-1 handler (Wolf3D variant)
class MusicType_WLF_Type1: virtual public MusicType_IMF_Common
{
	public:
		MusicType_WLF_Type1()
			throw ();

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();
};

/// IMF 280Hz Type-0 handler (Duke Nukem II variant)
class MusicType_IMF_Duke2: virtual public MusicType_IMF_Common
{
	public:
		MusicType_IMF_Duke2()
			throw ();

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_IMF_IDSOFTWARE_HPP_
