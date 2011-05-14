/**
 * @file   mus-cmf-creativelabs.hpp
 * @brief  MusicReader and MusicWriter classes for Creative Labs' CMF files.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_CMF_CREATIVELABS_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_CMF_CREATIVELABS_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/patchbank-opl.hpp>
#include "mus-generic-midi.hpp"

namespace camoto {
namespace gamemusic {

/// MusicType implementation for CMF files.
class MusicType_CMF: virtual public MusicType {

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

/// MusicReader class that understands CMF files.
class MusicReader_CMF: virtual public MusicReader_GenericMIDI {

	protected:
		istream_sptr input;       ///< CMF file to read
		OPLPatchBankPtr patches;  ///< List of instruments
		uint16_t offMusic;        ///< Offset of music block (for rewind)

	public:

		MusicReader_CMF(istream_sptr input)
			throw (std::ios::failure);

		virtual ~MusicReader_CMF()
			throw ();

		virtual PatchBankPtr getPatchBank()
			throw ();

		virtual void rewind()
			throw ();

};

/// MusicWriter class that can produce CMF files.
class MusicWriter_CMF: virtual public MusicWriter_GenericMIDI {

	protected:
		ostream_sptr output;                  ///< Where to write CMF file
		OPLPatchBankPtr patches;              ///< List of instruments
		uint8_t channelsInUse[MIDI_CHANNELS]; ///< Which MIDI channels were used

	public:
		MusicWriter_CMF(ostream_sptr output)
			throw (std::ios::failure);

		virtual ~MusicWriter_CMF()
			throw ();

		virtual void setPatchBank(const PatchBankPtr& instruments)
			throw (EBadPatchType);

		virtual void start()
			throw (std::ios::failure);

		virtual void finish()
			throw (std::ios::failure);

		virtual void handleEvent(NoteOnEvent *ev)
			throw (std::ios::failure, EChannelMismatch);

};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_CMF_CREATIVELABS_HPP_
