/**
 * @file  mus-got.cpp
 * @brief Support for God of Thunder song files.
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

#include <camoto/iostream_helpers.hpp>
#include "decode-opl.hpp"
#include "encode-opl.hpp"
#include "mus-got.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define GOT_DEFAULT_TEMPO 120 ///< Default tempo, in Hertz

/// Decode data in an .imf file to provide register/value pairs.
class OPLReaderCallback_GOT: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_GOT(stream::input& content)
			:	content(content)
		{
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			assert(oplEvent->valid == 0);

			try {
				uint8_t delay;
				this->content
					>> u8(delay)
					>> u8(oplEvent->reg)
					>> u8(oplEvent->val)
				;
				oplEvent->delay = delay;
			} catch (const stream::incomplete_read&) {
				return false;
			}

			if (
				(oplEvent->delay == 0)
				&& (oplEvent->reg == 0)
				&& (oplEvent->val == 0)
			) {
				// End of song
				return false;
			}
			oplEvent->chipIndex = 0; // This format only supports one OPL2
			oplEvent->valid |= OPLEvent::Delay | OPLEvent::Regs;
			return true;
		}

	protected:
		stream::input& content;   ///< Input file
};


/// Encode OPL register/value pairs into .imf file data.
class OPLWriterCallback_GOT: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_GOT(stream::output& content)
			:	content(content)
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			// Convert ticks into an IMF delay
			unsigned long delay;
			if (oplEvent->valid & OPLEvent::Delay) {
				delay = oplEvent->delay
					* oplEvent->tempo.usPerTick / HERTZ_TO_uS(GOT_DEFAULT_TEMPO);
			} else {
				delay = 0;
			}
			// Write out super long delays as dummy events to an unused port.
			while (delay > 0xFF) {
				this->content
					<< u8(0xFF)
					<< u8(0x00)
					<< u8(0x00)
				;
				delay -= 0xFF;
			}

			if (oplEvent->valid & OPLEvent::Regs) {
				// If this assertion fails, the caller sent OPL3 instructions despite
				// OPLWriteFlags::OPL2Only being supplied in the call to oplEncode().
				assert(oplEvent->chipIndex == 0);

				this->content
					<< u8(delay)
					<< u8(oplEvent->reg)
					<< u8(oplEvent->val)
				;
			} else if (delay) {
				// There is a delay but no regs (e.g. trailing delay)
				this->content
					<< u8(delay)
					<< u8(0)
					<< u8(0)
				;
			}
			return;
		}

	protected:
		stream::output& content; ///< Output file
};


std::string MusicType_GOT::code() const
{
	return "got";
}

std::string MusicType_GOT::friendlyName() const
{
	return "God of Thunder";
}

std::vector<std::string> MusicType_GOT::fileExtensions() const
{
	// No filename extension for this format
	return {};
}

MusicType::Caps MusicType_GOT::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
		| Caps::HardwareOPL2
	;
}

MusicType::Certainty MusicType_GOT::isInstance(stream::input& content) const
{
	stream::len len = content.size();

	// Must be enough room for header + footer
	// TESTED BY: mus_got_isinstance_c01
	if (len < 6) return Certainty::DefinitelyNo;

	// Uneven size.
	// TESTED BY: mus_got_isinstance_c02
	if (len % 3 != 0) return Certainty::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_got_isinstance_c03
	uint16_t sig;
	content.seekg(0, stream::start);
	content >> u16le(sig);
	if (sig != 0x0001) return Certainty::DefinitelyNo;

	// Make sure it ends with a loop-to-start marker
	// TESTED BY: mus_got_isinstance_c04
	uint32_t end;
	content.seekg(-4, stream::end);
	content >> u32le(end);
	if (end != 0) return Certainty::DefinitelyNo;

	// TESTED BY: mus_got_isinstance_c00
	return Certainty::PossiblyYes;
}

std::unique_ptr<Music> MusicType_GOT::read(stream::input& content,
	SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	content.seekg(2, stream::start);

	Tempo initialTempo;
	initialTempo.hertz(GOT_DEFAULT_TEMPO);

	OPLReaderCallback_GOT cb(content);
	auto music = oplDecode(&cb, DelayType::DelayIsPostData, OPL_FNUM_DEFAULT,
		initialTempo);

	return music;
}

void MusicType_GOT::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	content << u16le(1);

	// Call the generic OPL writer.
	OPLWriterCallback_GOT cb(content);

	auto oplFlags = toOPLFlags(flags);
	// Force this format to OPL2 as that's all we can write
	oplFlags |= OPLWriteFlags::OPL2Only;

	oplEncode(&cb, music, DelayType::DelayIsPostData, OPL_FNUM_DEFAULT, oplFlags);

	// Zero event (3 bytes) plus final 0x00
	content << nullPadded("", 4);

	// Set final filesize to this
	content.truncate_here();

	return;
}

SuppFilenames MusicType_GOT::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_GOT::supportedAttributes() const
{
	// No tags
	return {};
}
