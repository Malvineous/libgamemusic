/**
 * @file  mus-raw-rdos.cpp
 * @brief Support for Rdos RAW OPL capture (.raw) format.
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

#include <camoto/util.hpp>
#include <camoto/iostream_helpers.hpp>
#include "decode-opl.hpp"
#include "encode-opl.hpp"
#include "metadata-malv.hpp"
#include "mus-raw-rdos.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define uS_TO_RAWCLOCK(x) (round((x) * 1.192180))
#define RAWCLOCK_TO_uS(x) ((x) / 1.192180)

/// Decode data in a .raw file to provide register/value pairs.
class OPLReaderCallback_RAW: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_RAW(stream::input& content)
			:	content(content),
				chipIndex(0)
		{
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			assert(oplEvent->valid == 0);
			oplEvent->delay = 0;

			try {
nextCode:
				this->content
					>> u8(oplEvent->val)
					>> u8(oplEvent->reg)
				;
				switch (oplEvent->reg) {
					case 0x00: // short delay
						oplEvent->valid |= OPLEvent::Delay;
						oplEvent->delay += oplEvent->val;
						goto nextCode;
					case 0x02: { // control
						switch (oplEvent->val) {
							case 0x00: { // clock change
								uint16_t clock;
								this->content >> u16le(clock);
								if (clock == 0) clock = 0xffff;
								oplEvent->valid |= OPLEvent::Tempo;
								oplEvent->tempo.usPerTick = RAWCLOCK_TO_uS(clock);
								break;
							}
							case 0x01:
								this->chipIndex = 0;
								goto nextCode;
							case 0x02:
								this->chipIndex = 1;
								goto nextCode;
						}
						break;
					}
					case 0xFF:
						if (oplEvent->val == 0xFF) { // EOF
							// oplEvent->delay is populated with any final delay
							return false;
						}
						break;
					default: // normal reg
						oplEvent->valid |= OPLEvent::Regs;
						oplEvent->chipIndex = this->chipIndex;
						break;
				}
			} catch (const stream::incomplete_read&) {
				// oplEvent->delay is populated with any final delay
				return false;
			}

			return true;
		}

	protected:
		stream::input& content;  ///< Input file
		unsigned int chipIndex;  ///< Index of the currently selected OPL chip
};


/// Encode OPL register/value pairs into .raw file data.
class OPLWriterCallback_RAW: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_RAW(stream::output& content)
			:	content(content),
				lastChipIndex(0)
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			if (oplEvent->valid & OPLEvent::Tempo) {
				uint16_t clock = uS_TO_RAWCLOCK(oplEvent->tempo.usPerTick);
				this->content
					<< u8(0x00) // clock change
					<< u8(0x02) // control data
					<< u16le(clock)
				;
			}

			if (oplEvent->valid & OPLEvent::Delay) {
				// Write out the delay in one or more lots of 255 or less.
				unsigned long delay = oplEvent->delay;
				while (delay > 0) {
					uint8_t d = (delay > 255) ? 255 : delay;
					this->content
						<< u8(d) // delay value
						<< u8(0) // delay command
					;
					delay -= d;
				}
			}

			if (oplEvent->valid & OPLEvent::Regs) {
				// Switch OPL chips if necessary
				if (oplEvent->chipIndex != this->lastChipIndex) {
					assert(oplEvent->chipIndex < 2);
					this->content
						<< u8(0x01 + oplEvent->chipIndex) // 0x01 = chip 0, 0x02 = chip 1
						<< u8(0x02) // control command
					;
					this->lastChipIndex = oplEvent->chipIndex;
				}

				// Write out the reg/data if it's not one of the control structures.  If
				// it is we have to drop the pair as there's no way of escaping these
				// values.
				if ((oplEvent->reg != 0x00) || (oplEvent->reg != 0x02)) {
					this->content
						<< u8(oplEvent->val)
						<< u8(oplEvent->reg)
					;
				} else {
					std::cout << "Warning: Rdos RAW cannot store writes to OPL register "
						<< oplEvent->reg << " so this value has been lost." << std::endl;
				}
			}
			return;
		}

	protected:
		stream::output& content;    ///< Output file
		unsigned int lastChipIndex; ///< Index of the currently selected OPL chip
};

std::string MusicType_RAW::code() const
{
	return "raw-rdos";
}

std::string MusicType_RAW::friendlyName() const
{
	return "Rdos raw OPL capture";
}

std::vector<std::string> MusicType_RAW::fileExtensions() const
{
	return {
		"raw",
	};
}

MusicType::Caps MusicType_RAW::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
	;
}

MusicType::Certainty MusicType_RAW::isInstance(stream::input& content) const
{
	// Too short
	// TESTED BY: mus_raw_rdos_isinstance_c02
	if (content.size() < 10) return Certainty::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_raw_rdos_isinstance_c01
	char sig[8];
	content.seekg(0, stream::start);
	content.read(sig, 8);
	if (strncmp(sig, "RAWADATA", 8) != 0) return Certainty::DefinitelyNo;

	// TESTED BY: mus_raw_rdos_isinstance_c00
	// TESTED BY: mus_raw_rdos_isinstance_c03
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_RAW::read(stream::input& content,
	SuppData& suppData) const
{
	content.seekg(8, stream::start);
	uint16_t clock;
	content >> u16le(clock);
	if (clock == 0) clock = 0xffff;
	Tempo initialTempo;
	initialTempo.usPerTick = RAWCLOCK_TO_uS(clock);

	OPLReaderCallback_RAW cb(content);
	auto music = oplDecode(&cb, DelayType::DelayIsPreData, OPL_FNUM_DEFAULT,
		initialTempo);

	// See if there are any tags present
	readMalvMetadata(content, music.get());

	return music;
}

void MusicType_RAW::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	unsigned long tempo = uS_TO_RAWCLOCK(music.initialTempo.usPerTick);
	if (tempo > 65534) {
		throw format_limitation(createString(
			"The tempo is too slow for this format (tempo is " << tempo
			<< ", max is 65534)"
		));
	}
	content
		<< nullPadded("RAWADATA", 8)
		<< u16le(uS_TO_RAWCLOCK(music.initialTempo.usPerTick))
	;

	// Call the generic OPL writer.
	OPLWriterCallback_RAW cb(content);
	auto oplFlags = toOPLFlags(flags);
	oplEncode(&cb, music, DelayType::DelayIsPreData, OPL_FNUM_DEFAULT, oplFlags);

	// Write out the EOF marker
	content << u8(0xFF) << u8(0xFF);

	// Write out any metadata
	writeMalvMetadata(content, music.attributes());

	// Set final filesize to this
	content.truncate_here();

	return;
}

SuppFilenames MusicType_RAW::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_RAW::supportedAttributes() const
{
	return supportedMalvMetadata();
}
