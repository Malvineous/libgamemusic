/**
 * @file   mus-mid-type0.hpp
 * @brief  MusicReader and MusicWriter classes for single-track MIDI files.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_MID_TYPE0_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_MID_TYPE0_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/patchbank-opl.hpp>
#include "mus-generic-midi.hpp"

namespace camoto {
namespace gamemusic {

/// MusicType implementation for MIDI files.
class MusicType_MID_Type0: virtual public MusicType {

	public:

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();

		virtual E_CERTAINTY isInstance(istream_sptr psMusic) const
			throw (std::ios::failure);

		virtual MusicWriterPtr create(ostream_sptr output, SuppData& suppData) const
			throw (std::ios::failure);

		virtual MusicReaderPtr open(istream_sptr input, SuppData& suppData) const
			throw (std::ios::failure);

		virtual SuppFilenames getRequiredSupps(const std::string& filenameMusic) const
			throw ();

};

/// MusicReader class that understands MIDI files.
class MusicReader_MID_Type0: virtual public MusicReader_GenericMIDI {

	protected:
		istream_sptr input;       ///< MIDI file to read
		int offMusic; /// @todo temp (use array of tracks)

	public:

		MusicReader_MID_Type0(istream_sptr input)
			throw (std::ios::failure);

		virtual ~MusicReader_MID_Type0()
			throw ();

		virtual void rewind()
			throw ();

};

/// MusicWriter class that can produce MIDI files.
class MusicWriter_MID_Type0: virtual public MusicWriter_GenericMIDI {

	protected:
		ostream_sptr output;                  ///< Where to write MIDI file

	public:
		MusicWriter_MID_Type0(ostream_sptr output)
			throw (std::ios::failure);

		virtual ~MusicWriter_MID_Type0()
			throw ();

		virtual void start()
			throw (std::ios::failure);

		virtual void finish()
			throw (std::ios::failure);

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_MID_TYPE0_HPP_
