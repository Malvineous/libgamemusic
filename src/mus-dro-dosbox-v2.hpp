/**
 * @file   mus-dro-dosbox-v2.hpp
 * @brief  Support for the second version of the DOSBox Raw OPL .DRO format.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_DRO_DOSBOX_V2_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_DRO_DOSBOX_V2_HPP_

#include <camoto/stream_string.hpp>
#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/mus-generic-opl.hpp>

namespace camoto {
namespace gamemusic {

/// MusicType implementation for DRO.
class MusicType_DRO_v2: virtual public MusicType {

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

/// MusicReader class that understands DRO music files.
class MusicReader_DRO_v2: virtual public MusicReader_GenericOPL {

	protected:
		stream::input_sptr input;  ///< Stream of data to read
		uint32_t lenData;         ///< Length of data to read in reg/val pairs
		uint8_t codeShortDelay; ///< DRO code value used for a short delay
		uint8_t codeLongDelay;  ///< DRO code value used for a long delay
		uint8_t codemapLength;  ///< Number of valid entries in codemap array
		uint8_t codemap[128];   ///< Map DRO code values to OPL registers

	public:

		MusicReader_DRO_v2(stream::input_sptr input)
			throw (stream::error);

		virtual ~MusicReader_DRO_v2()
			throw ();

		virtual void rewind()
			throw ();

		virtual bool nextPair(uint32_t *delay, uint8_t *chipIndex, uint8_t *reg, uint8_t *val)
			throw (stream::error);

		// TODO: metadata functions
};

/// MusicWriter class that can produce DRO music files.
class MusicWriter_DRO_v2: virtual public MusicWriter_GenericOPL {

	protected:
		stream::output_sptr output;      ///< Stream to write data into
		stream::string_sptr buffer; ///< Buffer to store output data until finish()
		int dataLen;              ///< Length of data in buffer, in pairs
		uint32_t numTicks;        ///< Total number of ticks in the song
		int usPerTick;            ///< Latest microseconds per tick value (tempo)
		uint8_t oplType;          ///< OPL hardware type to write into DRO header
		uint8_t codemapLength;    ///< Number of valid entries in codemap array
		uint8_t codemap[256];     ///< Map DRO code values to OPL registers
		uint16_t cachedDelay;     ///< Delay remembered but not yet written

	public:

		MusicWriter_DRO_v2(stream::output_sptr output)
			throw ();

		virtual ~MusicWriter_DRO_v2()
			throw ();

		virtual void start()
			throw (stream::error);

		virtual void finish()
			throw (stream::error);

		virtual void changeSpeed(uint32_t usPerTick)
			throw ();

		virtual void nextPair(uint32_t delay, uint8_t chipIndex, uint8_t reg, uint8_t val)
			throw (stream::error);

		// TODO: metadata functions

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_DRO_DOSBOX_V2_HPP_
