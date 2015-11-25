/**
 * @file  mus-imf-idsoftware.cpp
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

#include <camoto/iostream_helpers.hpp>
#include "decode-opl.hpp"
#include "encode-opl.hpp"
#include "metadata-malv.hpp"
#include "mus-imf-idsoftware.hpp"

/// Any files with delays longer than this value will be reported as not
/// being IMF files.  This could be over cautious, but there are unlikely
/// to be any real songs with delays this long.
/// @todo Check against apogfnf1.imf and seg3.imf
#define IMF_MAX_DELAY  0x4000

using namespace camoto;
using namespace camoto::gamemusic;

/// Value passed to OPLReaderCallback_IMF c'tor if the length is unused
#define IMF_LEN_UNUSED ((unsigned int)-1)

/// Decode data in an .imf file to provide register/value pairs.
class OPLReaderCallback_IMF: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_IMF(stream::input& content, unsigned int lenData)
			:	content(content),
				lenData(lenData)
		{
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			assert(oplEvent->valid == 0);
			if (this->lenData == 0) return false;

			try {
				this->content
					>> u8(oplEvent->reg)
					>> u8(oplEvent->val)
					>> u16le(oplEvent->delay)
				;
			} catch (const stream::incomplete_read&) {
				this->lenData = 0;
				return false;
			}

			if (this->lenData != IMF_LEN_UNUSED) this->lenData -= 4;
			oplEvent->chipIndex = 0; // Only one OPL2 supported
			oplEvent->valid |= OPLEvent::Delay | OPLEvent::Regs;
			return true;
		}

	protected:
		stream::input& content;   ///< Input file
		unsigned long lenData;    ///< Length of song data (where song stops and tags start)
};


/// Encode OPL register/value pairs into .imf file data.
class OPLWriterCallback_IMF: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_IMF(stream::output& content, unsigned int speed)
			:	content(content),
				speed(speed)
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			// Convert ticks into an IMF delay
			unsigned long delay;
			if (oplEvent->valid & OPLEvent::Delay) {
				delay = oplEvent->delay
					* oplEvent->tempo.usPerTick / HERTZ_TO_uS(this->speed);

				// Write out super long delays as dummy events to an unused port.
				while (delay > 0xFFFF) {
					this->content
						<< u8(0x00)
						<< u8(0x00)
						<< u16le(0xFFFF)
					;
					delay -= 0xFFFF;
				}
			} else {
				delay = 0;
			}

			if (oplEvent->valid & OPLEvent::Regs) {
				// If this assertion fails, the caller sent OPL3 instructions despite
				// OPLWriteFlags::OPL2Only being supplied in the call to oplEncode().
				assert(oplEvent->chipIndex == 0);

				this->content
					<< u8(oplEvent->reg)
					<< u8(oplEvent->val)
					<< u16le(delay)
				;
			} else if (delay) {
				// There is a delay but no regs (e.g. trailing delay)
				this->content
					<< u8(0)
					<< u8(0)
					<< u16le(delay)
				;
			}
			return;
		}

	protected:
		stream::output& content; ///< Output file
		unsigned int speed;      ///< IMF clock rate
};


MusicType_IMF_Common::MusicType_IMF_Common(unsigned int imfType,
	unsigned int speed)
	:	imfType(imfType),
		speed(speed)
{
}

MusicType::Caps MusicType_IMF_Common::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
		| Caps::HardwareOPL2
	;
}

MusicType::Certainty MusicType_IMF_Common::isInstance(stream::input& content)
	const
{
	stream::pos len = content.size();

	// TESTED BY: mus_imf_idsoftware_type*_isinstance_c01
	if (len < 4) return Certainty::DefinitelyNo; // too short

	// Read the first two bytes as the data length size and make sure they
	// don't point past the end of the file.
	// TESTED BY: mus_imf_idsoftware_type1_isinstance_c05
	content.seekg(0, stream::start);
	uint16_t dataLen;
	content >> u16le(dataLen);
	if (dataLen > len) return Certainty::DefinitelyNo;

	if (dataLen == 0) { // type-0 format
		// Jump back so these bytes are counted as the first event
		content.seekg(0, stream::start);
		// Only type-0 files start like this
		// TESTED BY: mus_imf_idsoftware_type0_isinstance_c04
		if (this->imfType != 0) return Certainty::DefinitelyNo;
		dataLen = len;
	} else { // type-1 format
		// TESTED BY: mus_imf_idsoftware_type1_isinstance_c04
		if (this->imfType != 1) return Certainty::DefinitelyNo;
	}

	// Make sure files with incomplete data sections aren't picked up
	// TESTED BY: mus_imf_idsoftware_type*_isinstance_c06
	if (dataLen % 4) return Certainty::DefinitelyNo;

	// TODO: Parse file and check for invalid register writes.
	uint16_t delay;
	uint8_t reg, val;
	while (dataLen > 3) {
		// When dataLen < 4, TESTED BY: mus_imf_idsoftware_type1_isinstance_c06

		// Read in the next reg/data pair
		content
			>> u8(reg)
			>> u8(val)
			>> u16le(delay)
		;
		// Make sure it points to a valid OPL register
		if (
			(reg == 0x06)
			|| (reg == 0x07)
			|| ((reg >= 0x09) && (reg <= 0x1F))
			|| ((reg >= 0x36) && (reg <= 0x3F))
			|| ((reg >= 0x56) && (reg <= 0x5F))
			|| ((reg >= 0x76) && (reg <= 0x7F))
			|| ((reg >= 0x96) && (reg <= 0x9F))
			|| ((reg >= 0xA9) && (reg <= 0xAF))
			|| ((reg >= 0xB9) && (reg <= 0xBC))
			|| ((reg >= 0xBE) && (reg <= 0xBF))
			|| ((reg >= 0xC9) && (reg <= 0xDF))
			|| ((reg >= 0xF6))// && (reg <= 0xFF))
		) {
			// This is an invalid OPL register
			// TESTED BY: mus_imf_idsoftware_type*_isinstance_c02
			return Certainty::DefinitelyNo;
		}

		// Very unlikely that a real song would have a lengthy delay in it...
		// TESTED BY: mus_imf_idsoftware_type*_isinstance_c03
		if (delay > IMF_MAX_DELAY) return Certainty::DefinitelyNo;

		dataLen -= 4;
	}

	// TESTED BY: mus_imf_idsoftware_isinstance_c00
	// TESTED BY: mus_imf_idsoftware_isinstance_c05
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_IMF_Common::read(stream::input& content,
	SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	content.seekg(0, stream::start);

	unsigned int lenData;
	if (this->imfType == 1) {
		content >> u16le(lenData);
	} else {
		lenData = IMF_LEN_UNUSED; // read until EOF
	}

	Tempo initialTempo;
	initialTempo.hertz(this->speed);
	initialTempo.ticksPerBeat = this->speed / 4; // still wrong, but the defaults are wildly inaccurate

	OPLReaderCallback_IMF cb(content, lenData);
	auto music = oplDecode(&cb, DelayType::DelayIsPostData, OPL_FNUM_DEFAULT,
		initialTempo);

	if (this->imfType == 1) {
		// See if there are any tags present
		readMalvMetadata(content, music.get());
	}

	return music;
}

void MusicType_IMF_Common::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	if (this->imfType == 1) {
		// Write a placeholder for the song length we'll fill out later when we
		// know what value to use.
		content.write("\x00\x00", 2);
	}

	// Most files seem to start with a dummy event for some reason.  At least it
	// makes it easy to tell between type-0 and type-1 files.
	content << u32le(0);

	// Call the generic OPL writer.
	OPLWriterCallback_IMF cb(content, this->speed);

	auto oplFlags = toOPLFlags(flags);

	// IMF files need the first channel free, as games use this for Adlib SFX.
	oplFlags |= OPLWriteFlags::ReserveFirstChan;

	// Force this format to OPL2 as that's all we can write
	oplFlags |= OPLWriteFlags::OPL2Only;

	oplEncode(&cb, music, DelayType::DelayIsPostData, OPL_FNUM_DEFAULT, oplFlags);

	if (this->imfType == 1) {
		// Update the placeholder we wrote in the constructor with the file size
		int size = content.tellp();
		size -= 2; // don't count the header

		// Write out any metadata
		writeMalvMetadata(content, music.attributes());

		// Set final filesize to this
		content.truncate_here();

		content.seekp(0, stream::start);
		content << u16le(size);
	} else {
		// Set final filesize to this
		content.truncate_here();
	}

	return;
}

SuppFilenames MusicType_IMF_Common::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_IMF_Common::supportedAttributes() const
{
	return supportedMalvMetadata();
}


MusicType_IMF_Type0::MusicType_IMF_Type0()
	:	MusicType_IMF_Common(0, 560)
{
}

std::string MusicType_IMF_Type0::code() const
{
	return "imf-idsoftware-type0";
}

std::string MusicType_IMF_Type0::friendlyName() const
{
	return "id Software Music Format (type-0, 560Hz)";
}

std::vector<std::string> MusicType_IMF_Type0::fileExtensions() const
{
	return {
		"imf",
		"mni",
	};
}


MusicType_IMF_Type1::MusicType_IMF_Type1()
	:	MusicType_IMF_Common(1, 560)
{
}

std::string MusicType_IMF_Type1::code() const
{
	return "imf-idsoftware-type1";
}

std::string MusicType_IMF_Type1::friendlyName() const
{
	return "id Software Music Format (type-1, 560Hz)";
}

std::vector<std::string> MusicType_IMF_Type1::fileExtensions() const
{
	return {
		"imf",
		"mni",
	};
}


MusicType_WLF_Type0::MusicType_WLF_Type0()
	:	MusicType_IMF_Common(0, 700)
{
}

std::string MusicType_WLF_Type0::code() const
{
	return "wlf-idsoftware-type0";
}

std::string MusicType_WLF_Type0::friendlyName() const
{
	return "id Software Music Format (type-0, 700Hz)";
}

std::vector<std::string> MusicType_WLF_Type0::fileExtensions() const
{
	return {
		"wlf",
	};
}


MusicType_WLF_Type1::MusicType_WLF_Type1()
	:	MusicType_IMF_Common(1, 700)
{
}

std::string MusicType_WLF_Type1::code() const
{
	return "wlf-idsoftware-type1";
}

std::string MusicType_WLF_Type1::friendlyName() const
{
	return "id Software Music Format (type-1, 700Hz)";
}

std::vector<std::string> MusicType_WLF_Type1::fileExtensions() const
{
	return {
		"wlf",
	};
}


MusicType_IMF_Duke2::MusicType_IMF_Duke2()
	:	MusicType_IMF_Common(0, 280)
{
}

std::string MusicType_IMF_Duke2::code() const
{
	return "imf-idsoftware-duke2";
}

std::string MusicType_IMF_Duke2::friendlyName() const
{
	return "id Software Music Format (type-0, 280Hz)";
}

std::vector<std::string> MusicType_IMF_Duke2::fileExtensions() const
{
	return {
		"imf",
	};
}
