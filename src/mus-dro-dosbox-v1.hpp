/**
 * @file   mus-dro-dosbox-v1.hpp
 * @brief  Support for the first version of the DOSBox Raw OPL .DRO format.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_DRO_DOSBOX_V1_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_DRO_DOSBOX_V1_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/mus-generic-opl.hpp>

namespace camoto {
namespace gamemusic {

/// MusicType implementation for DRO.
class MusicType_DRO_v1: virtual public MusicType {

	public:

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();

		virtual MusicType::Certainty isInstance(istream_sptr psMusic) const
			throw (std::ios::failure);

		virtual MusicWriterPtr create(ostream_sptr output, SuppData& suppData) const
			throw (std::ios::failure);

		virtual MusicReaderPtr open(istream_sptr input, SuppData& suppData) const
			throw (std::ios::failure);

		virtual SuppFilenames getRequiredSupps(const std::string& filenameMusic) const
			throw ();

};

/// MusicReader class that understands DRO music files.
class MusicReader_DRO_v1: virtual public MusicReader_GenericOPL {

	protected:
		istream_sptr input;  ///< Stream of data to read
		int lenData;         ///< Length of data to read (must be signed as -1 is possible)
		int chipIndex;       ///< Index of the currently selected OPL chip

	public:

		MusicReader_DRO_v1(istream_sptr input)
			throw (std::ios::failure);

		virtual ~MusicReader_DRO_v1()
			throw ();

		virtual void rewind()
			throw ();

		virtual bool nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg, uint8_t *val)
			throw (std::ios::failure);

		// TODO: metadata functions
};

/// MusicWriter class that can produce DRO music files.
class MusicWriter_DRO_v1: virtual public MusicWriter_GenericOPL {

	protected:
		ostream_sptr output; ///< Stream to write data into
		uint32_t numTicks;   ///< Total number of ticks in the song
		int usPerTick;       ///< Latest microseconds per tick value (tempo)
		int lastChipIndex;   ///< Index of the currently selected OPL chip

	public:

		MusicWriter_DRO_v1(ostream_sptr output)
			throw ();

		virtual ~MusicWriter_DRO_v1()
			throw ();

		virtual void start()
			throw (std::ios::failure);

		virtual void finish()
			throw (std::ios::failure);

		virtual void changeSpeed(uint32_t usPerTick)
			throw ();

		virtual void nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
			throw (std::ios::failure);

		// TODO: metadata functions

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_DRO_DOSBOX_V1_HPP_
