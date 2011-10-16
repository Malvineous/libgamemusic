/**
 * @file   mus-klm-wacky.hpp
 * @brief  MusicReader and MusicWriter classes for Wacky Wheels KLM files.
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

#ifndef _CAMOTO_GAMEMUSIC_MUS_KLM_WACKY_HPP_
#define _CAMOTO_GAMEMUSIC_MUS_KLM_WACKY_HPP_

#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/patchbank-opl.hpp>

namespace camoto {
namespace gamemusic {

/// MusicType implementation for KLM files.
class MusicType_KLM: virtual public MusicType {

	public:

		virtual std::string getCode() const
			throw ();

		virtual std::string getFriendlyName() const
			throw ();

		virtual std::vector<std::string> getFileExtensions() const
			throw ();

		virtual Certainty isInstance(stream::input_sptr psMusic) const
			throw (stream::error);

		virtual MusicWriterPtr create(stream::output_sptr output, SuppData& suppData) const
			throw (stream::error);

		virtual MusicReaderPtr open(stream::input_sptr input, SuppData& suppData) const
			throw (stream::error);

		virtual SuppFilenames getRequiredSupps(const std::string& filenameMusic) const
			throw ();

};

#define KLM_CHANNEL_COUNT 14 //11

/// MusicReader class that understands KLM files.
class MusicReader_KLM: virtual public MusicReader {

	public:
		MusicReader_KLM(stream::input_sptr input)
			throw (stream::error);

		virtual ~MusicReader_KLM()
			throw ();

		virtual PatchBankPtr getPatchBank()
			throw ();

		virtual EventPtr readNextEvent()
			throw (stream::error);

	protected:
		stream::input_sptr input;                  ///< KLM file to read
		OPLPatchBankPtr patches;             ///< List of instruments
		uint16_t tempo;                      ///< Tempo from file header
		bool setTempo;                       ///< Has the tempo event been returned yet?
		bool setRhythm;                      ///< Have we enabled rhythm mode yet?
		bool setWaveSel;                     ///< Have we set WSEnable yet?
		unsigned long tick;                  ///< Last event's time
		uint8_t patchMap[KLM_CHANNEL_COUNT]; ///< Which instruments are in use on which channel
		int freqMap[KLM_CHANNEL_COUNT];      ///< Frequency set on each channel
		uint8_t volMap[KLM_CHANNEL_COUNT];   ///< Volume set on each channel
		bool noteOn[KLM_CHANNEL_COUNT];      ///< Is a note playing on this channel?

};

/// MusicWriter class that can produce KLM files.
class MusicWriter_KLM: virtual public MusicWriter {

	public:
		MusicWriter_KLM(stream::output_sptr output)
			throw (stream::error);

		virtual ~MusicWriter_KLM()
			throw ();

		virtual void setPatchBank(const PatchBankPtr& instruments)
			throw (EBadPatchType);

		virtual void start()
			throw (stream::error);

		virtual void finish()
			throw (stream::error);

		virtual void handleEvent(TempoEvent *ev)
			throw (stream::error);

		virtual void handleEvent(NoteOnEvent *ev)
			throw (stream::error, EChannelMismatch);

		virtual void handleEvent(NoteOffEvent *ev)
			throw (stream::error);

		virtual void handleEvent(PitchbendEvent *ev)
			throw (stream::error);

		virtual void handleEvent(ConfigurationEvent *ev)
			throw (stream::error);

	protected:
		stream::output_sptr output;         ///< Where to write KLM file
		OPLPatchBankPtr patches;     ///< List of instruments
		unsigned long lastTick;      ///< Tick value from previous event
		uint8_t patchMap[KLM_CHANNEL_COUNT]; ///< Which instruments are in use on which channel

	/// Write out the current delay, if there is one (curTick != lastTick)
	void writeDelay(unsigned long curTick)
		throw (stream::error);

};


} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUS_KLM_WACKY_HPP_
