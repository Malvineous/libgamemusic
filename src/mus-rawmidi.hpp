/**
 * @file   mus-rawmidi.hpp
 * @brief  Support for standard MIDI data without any header.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_RAWMIDI_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_RAWMIDI_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include "mus-generic-midi.hpp"

namespace camoto {
namespace gamemusic {

/// MusicType implementation for raw MIDI.
class MusicType_RawMIDI: virtual public MusicType {

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

/// MusicReader class that understands raw MIDI data.
class MusicReader_RawMIDI: virtual public MusicReader_GenericMIDI {

	protected:
		stream::input_sptr input;  ///< Stream of data to read

	public:

		MusicReader_RawMIDI(stream::input_sptr input)
			throw (stream::error);

		virtual ~MusicReader_RawMIDI()
			throw ();

		virtual void rewind()
			throw ();

};

/// MusicWriter class that can produce MIDI data without any header.
class MusicWriter_RawMIDI: virtual public MusicWriter_GenericMIDI {

	public:

		MusicWriter_RawMIDI(stream::output_sptr output)
			throw ();

		virtual ~MusicWriter_RawMIDI()
			throw ();

		virtual void start()
			throw (stream::error);

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_RAWMIDI_HPP_
