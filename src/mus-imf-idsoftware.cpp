/**
 * @file   mus-imf-idsoftware.cpp
 * @brief  Support for id Software's .IMF format.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
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

#define HERTZ_TO_uS(x) (1000000 / (x))

/// Value passed to OPLReaderCallback_IMF c'tor if the length is unused
#define IMF_LEN_UNUSED ((unsigned int)-1)

/// Decode data in an .imf file to provide register/value pairs.
class OPLReaderCallback_IMF: virtual public OPLReaderCallback
{
	public:
		OPLReaderCallback_IMF(stream::input_sptr input, unsigned int speed, unsigned int lenData)
			:	input(input),
				first(true),
				speed(speed),
				lenData(lenData)
		{
		}

		virtual bool readNextPair(OPLEvent *oplEvent)
		{
			if (this->lenData == 0) return false;
			if (this->first) {
				// First call, set tempo
				this->first = false;
				oplEvent->tempo = HERTZ_TO_uS(this->speed);
				oplEvent->delay = 0;
				oplEvent->reg = 0;
				oplEvent->val = 0;
				oplEvent->chipIndex = 0;
				return true; // empty event w/ initial tempo
			}

			try {
				oplEvent->chipIndex = 0; // IMF only supports one OPL2
				this->input
					>> u8(oplEvent->reg)
					>> u8(oplEvent->val)
					>> u16le(oplEvent->delay)
				;
				if (this->lenData != IMF_LEN_UNUSED) this->lenData -= 4;
			} catch (const stream::incomplete_read&) {
				return false;
			}
			return true;
		}

	protected:
		stream::input_sptr input;   ///< Input file
		bool first;                 ///< Is this the first time readNextPair() has been called?
		unsigned int speed;         ///< IMF clock rate
		unsigned long lenData;      ///< Length of song data (where song stops and tags start)
};


/// Encode OPL register/value pairs into .imf file data.
class OPLWriterCallback_IMF: virtual public OPLWriterCallback
{
	public:
		OPLWriterCallback_IMF(stream::output_sptr output, unsigned int speed)
			:	output(output),
				speed(speed),
				usPerTick(HERTZ_TO_uS(speed)) // default to speed ticks per second
		{
		}

		virtual void writeNextPair(const OPLEvent *oplEvent)
		{
			// Convert ticks into an IMF delay
			unsigned long delay = oplEvent->delay * this->usPerTick / (HERTZ_TO_uS(this->speed));
			// Write out super long delays as dummy events to an unused port.
			while (delay > 0xFFFF) {
				this->output
					<< u8(0x00)
					<< u8(0x00)
					<< u16le(0xFFFF)
				;
				delay -= 0xFFFF;
			}

			/// @todo Put this as a format capability
			if (oplEvent->chipIndex != 0) {
				throw stream::error("IMF files are single-OPL2 only");
			}
			assert(oplEvent->chipIndex == 0);  // make sure the format capabilities are being honoured

			this->output
				<< u8(oplEvent->reg)
				<< u8(oplEvent->val)
				<< u16le(delay)
			;
			return;
		}

		virtual void writeTempoChange(tempo_t usPerTick)
		{
			assert(usPerTick != 0);
			this->usPerTick = usPerTick;
			return;
		}

	protected:
		stream::output_sptr output; ///< Output file
		unsigned int speed;         ///< IMF clock rate
		tempo_t usPerTick;          ///< Latest microseconds per tick value (tempo)
};


MusicType_IMF_Common::MusicType_IMF_Common(unsigned int imfType, unsigned int speed)
	:	imfType(imfType),
		speed(speed)
{
}

MusicType::Certainty MusicType_IMF_Common::isInstance(stream::input_sptr input) const
{
	stream::pos len = input->size();

	// TESTED BY: mus_imf_idsoftware_type*_isinstance_c01
	if (len < 2) return DefinitelyNo; // too short

	// Read the first two bytes as the data length size and make sure they
	// don't point past the end of the file.
	// TESTED BY: mus_imf_idsoftware_type1_isinstance_c05
	input->seekg(0, stream::start);
	uint16_t dataLen;
	input >> u16le(dataLen);
	if (dataLen > len) return DefinitelyNo;

	if (dataLen == 0) { // type-0 format
		// Jump back so these bytes are counted as the first event
		input->seekg(0, stream::start);
		// Only type-0 files start like this
		// TESTED BY: mus_imf_idsoftware_type0_isinstance_c04
		if (this->imfType != 0) return DefinitelyNo;
		dataLen = len;
	} else { // type-1 format
		// TESTED BY: mus_imf_idsoftware_type1_isinstance_c04
		if (this->imfType != 1) return DefinitelyNo;

		// Make sure files with incomplete data sections aren't picked up
		// TESTED BY: mus_imf_idsoftware_type1_isinstance_c06
		if (dataLen % 4) return DefinitelyNo;
	}

	// TODO: Parse file and check for invalid register writes.
	uint16_t delay; uint8_t reg, val;
	while (dataLen > 3) {
		// When dataLen < 4, TESTED BY: mus_imf_idsoftware_type1_isinstance_c06

		// Read in the next reg/data pair
		input
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
			return DefinitelyNo;
		}

		// Very unlikely that a real song would have a lengthy delay in it...
		// TESTED BY: mus_imf_idsoftware_type*_isinstance_c03
		if (delay > IMF_MAX_DELAY) return DefinitelyNo;

		dataLen -= 4;
	}

	// TESTED BY: mus_imf_idsoftware_isinstance_c00
	return DefinitelyYes;
}

MusicPtr MusicType_IMF_Common::read(stream::input_sptr input, SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	input->seekg(0, stream::start);

	unsigned int lenData;
	if (this->imfType == 1) {
		input >> u16le(lenData);
	} else {
		lenData = IMF_LEN_UNUSED; // read until EOF
	}

	OPLReaderCallback_IMF cb(input, this->speed, lenData);
	MusicPtr music = oplDecode(&cb, DelayIsPostData, OPL_FNUM_DEFAULT);

	if (this->imfType == 1) {
		// See if there are any tags present
		readMalvMetadata(input, music->metadata);
	}

	return music;
}

void MusicType_IMF_Common::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	if (this->imfType == 1) {
		// Write a placeholder for the song length we'll fill out later when we
		// know what value to use.
		output->write("\x00\x00", 2);
	}

	// Most files seem to start with a dummy event for some reason.  At least it
	// makes it easy to tell between type-0 and type-1 files.
	output << u32le(0);

	// Call the generic OPL writer.
	OPLWriterCallback_IMF cb(output, this->speed);

	// IMF files need the first channel free, as games use this for Adlib SFX.
	flags |= ReserveFirstChan;

	oplEncode(&cb, DelayIsPostData, OPL_FNUM_DEFAULT, flags, music);

	if (this->imfType == 1) {
		// Update the placeholder we wrote in the constructor with the file size
		int size = output->tellp();
		size -= 2; // don't count the header

		// Write out any metadata
		writeMalvMetadata(output, music->metadata);

		// Set final filesize to this
		output->truncate_here();

		output->seekp(0, stream::start);
		output << u16le(size);
	} else {
		// Set final filesize to this
		output->truncate_here();
	}

	return;
}

SuppFilenames MusicType_IMF_Common::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_IMF_Common::getMetadataList() const
{
	Metadata::MetadataTypes types;
	if (this->imfType == 1) {
		types.push_back(Metadata::Title);
		types.push_back(Metadata::Author);
		types.push_back(Metadata::Description);
	}
	return types;
}


MusicType_IMF_Type0::MusicType_IMF_Type0()
	:	MusicType_IMF_Common(0, 560)
{
}

std::string MusicType_IMF_Type0::getCode() const
{
	return "imf-idsoftware-type0";
}

std::string MusicType_IMF_Type0::getFriendlyName() const
{
	return "id Software Music Format (type-0, 560Hz)";
}

std::vector<std::string> MusicType_IMF_Type0::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("imf");
	vcExtensions.push_back("mni");
	return vcExtensions;
}


MusicType_IMF_Type1::MusicType_IMF_Type1()
	:	MusicType_IMF_Common(1, 560)
{
}

std::string MusicType_IMF_Type1::getCode() const
{
	return "imf-idsoftware-type1";
}

std::string MusicType_IMF_Type1::getFriendlyName() const
{
	return "id Software Music Format (type-1, 560Hz)";
}

std::vector<std::string> MusicType_IMF_Type1::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("imf");
	vcExtensions.push_back("mni");
	return vcExtensions;
}


MusicType_WLF_Type0::MusicType_WLF_Type0()
	:	MusicType_IMF_Common(0, 700)
{
}

std::string MusicType_WLF_Type0::getCode() const
{
	return "wlf-idsoftware-type0";
}

std::string MusicType_WLF_Type0::getFriendlyName() const
{
	return "id Software Music Format (type-0, 700Hz)";
}

std::vector<std::string> MusicType_WLF_Type0::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("wlf");
	return vcExtensions;
}


MusicType_WLF_Type1::MusicType_WLF_Type1()
	:	MusicType_IMF_Common(1, 700)
{
}

std::string MusicType_WLF_Type1::getCode() const
{
	return "wlf-idsoftware-type1";
}

std::string MusicType_WLF_Type1::getFriendlyName() const
{
	return "id Software Music Format (type-1, 700Hz)";
}

std::vector<std::string> MusicType_WLF_Type1::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("wlf");
	return vcExtensions;
}


MusicType_IMF_Duke2::MusicType_IMF_Duke2()
	:	MusicType_IMF_Common(0, 280)
{
}

std::string MusicType_IMF_Duke2::getCode() const
{
	return "imf-idsoftware-duke2";
}

std::string MusicType_IMF_Duke2::getFriendlyName() const
{
	return "id Software Music Format (type-0, 280Hz)";
}

std::vector<std::string> MusicType_IMF_Duke2::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("imf");
	return vcExtensions;
}
