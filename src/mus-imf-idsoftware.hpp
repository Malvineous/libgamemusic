/**
 * @file  mus-imf-idsoftware.hpp
 * @brief Support for id Software's .IMF format.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_IMF_IDSOFTWARE_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_IMF_IDSOFTWARE_HPP_

#include <camoto/gamemusic/musictype.hpp>

namespace camoto {
namespace gamemusic {

/// Common IMF functions for all versions
class MusicType_IMF_Common: virtual public MusicType
{
	public:
		MusicType_IMF_Common(unsigned int imfType, unsigned int speed);
		virtual Caps caps() const;
		virtual Certainty isInstance(stream::input& content) const;
		virtual std::unique_ptr<Music> read(stream::input& content, SuppData& suppData) const;
		virtual void write(stream::output& content, SuppData& suppData,
			const Music& music, WriteFlags flags) const;
		virtual SuppFilenames getRequiredSupps(stream::input& content,
			const std::string& filename) const;
		virtual std::vector<Attribute> supportedAttributes() const;

	protected:
		unsigned int imfType;  ///< IMF format type; 0 or 1
		unsigned int speed;    ///< IMF speed in Hertz
};

/// IMF 560Hz Type-0 handler
class MusicType_IMF_Type0: virtual public MusicType_IMF_Common
{
	public:
		MusicType_IMF_Type0();

		virtual std::string code() const;
		virtual std::string friendlyName() const;
		virtual std::vector<std::string> fileExtensions() const;
};

/// IMF 560Hz Type-1 handler
class MusicType_IMF_Type1: virtual public MusicType_IMF_Common
{
	public:
		MusicType_IMF_Type1();

		virtual std::string code() const;
		virtual std::string friendlyName() const;
		virtual std::vector<std::string> fileExtensions() const;
};

/// WLF 700Hz Type-0 handler
class MusicType_WLF_Type0: virtual public MusicType_IMF_Common
{
	public:
		MusicType_WLF_Type0();

		virtual std::string code() const;
		virtual std::string friendlyName() const;
		virtual std::vector<std::string> fileExtensions() const;
};

/// WLF 700Hz Type-1 handler (Wolf3D variant)
class MusicType_WLF_Type1: virtual public MusicType_IMF_Common
{
	public:
		MusicType_WLF_Type1();

		virtual std::string code() const;
		virtual std::string friendlyName() const;
		virtual std::vector<std::string> fileExtensions() const;
};

/// IMF 280Hz Type-0 handler (Duke Nukem II variant)
class MusicType_IMF_Duke2: virtual public MusicType_IMF_Common
{
	public:
		MusicType_IMF_Duke2();

		virtual std::string code() const;
		virtual std::string friendlyName() const;
		virtual std::vector<std::string> fileExtensions() const;
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_IMF_IDSOFTWARE_HPP_
