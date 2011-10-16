/**
 * @file   mus-raw-rdos.hpp
 * @brief  Support for Rdos RAW OPL capture (.raw) format.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_RAW_RDOS_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_RAW_RDOS_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/mus-generic-opl.hpp>

namespace camoto {
namespace gamemusic {

/// MusicType implementation for Rdos Raw.
class MusicType_RAW: virtual public MusicType {

	public:

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();

		virtual MusicType::Certainty isInstance(stream::input_sptr psMusic) const
			throw (stream::error);

		virtual MusicWriterPtr create(stream::output_sptr output, SuppData& suppData) const
			throw (stream::error);

		virtual MusicReaderPtr open(stream::input_sptr input, SuppData& suppData) const
			throw (stream::error);

		virtual SuppFilenames getRequiredSupps(const std::string& filenameMusic) const
			throw ();

};

/// MusicReader class that understands Rdos RAW music files.
class MusicReader_RAW: virtual public MusicReader_GenericOPL {

	protected:
		stream::input_sptr input;  ///< Stream of data to read
		int chipIndex;       ///< Index of the currently selected OPL chip

	public:

		MusicReader_RAW(stream::input_sptr input)
			throw (stream::error);

		virtual ~MusicReader_RAW()
			throw ();

		virtual void rewind()
			throw ();

		virtual bool nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg,
			uint8_t *val)
			throw (stream::error);

		// TODO: metadata functions
};

/// MusicWriter class that can produce Rdos RAW music files.
class MusicWriter_RAW: virtual public MusicWriter_GenericOPL {

	protected:
		stream::output_sptr output; ///< Stream to write data into
		int lastChipIndex;   ///< Index of the currently selected OPL chip
		uint16_t firstClock; ///< First tempo change saved to write back into header
		uint16_t lastClock;  ///< Last tempo change actioned (to avoid dupes)

	public:

		MusicWriter_RAW(stream::output_sptr output)
			throw ();

		virtual ~MusicWriter_RAW()
			throw ();

		virtual void start()
			throw (stream::error);

		virtual void finish()
			throw (stream::error);

		virtual void changeSpeed(uint32_t usPerTick)
			throw ();

		virtual void nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg,
			uint8_t val)
			throw (stream::error);

		// TODO: metadata functions

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_RAW_RDOS_HPP_
