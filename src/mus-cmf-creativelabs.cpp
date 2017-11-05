/**
 * @file  mus-cmf-creativelabs.cpp
 * @brief Support for Creative Labs' CMF format.
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

#include <iostream>
#include <camoto/iostream_helpers.hpp>
#include <camoto/stream_string.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/util-opl.hpp>
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "util-sbi.hpp"
#include "mus-cmf-creativelabs.hpp"

/// @todo Handle MIDI controller events for transpose up and down

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of available channels in a CMF file.
const unsigned int CMF_MAX_CHANNELS = 16;

/// Maximum number of bytes in each CMF title/composer/remarks field.
const unsigned int CMF_ATTR_MAXLEN = 32767;

/// Number of preset instruments (repeated from patch 0 to 128)
#define CMF_NUM_DEFAULT_INSTRUMENTS 16

static const char CMF_DEFAULT_PATCHES[] =
"\x01\x11\x4F\x00\xF1\xD2\x53\x74\x00\x00\x06" "\x00\x00\x00\x00\x00"
"\x07\x12\x4F\x00\xF2\xF2\x60\x72\x00\x00\x08" "\x00\x00\x00\x00\x00"
"\x31\xA1\x1C\x80\x51\x54\x03\x67\x00\x00\x0E" "\x00\x00\x00\x00\x00"
"\x31\xA1\x1C\x80\x41\x92\x0B\x3B\x00\x00\x0E" "\x00\x00\x00\x00\x00"
"\x31\x16\x87\x80\xA1\x7D\x11\x43\x00\x00\x08" "\x00\x00\x00\x00\x00"
"\x30\xB1\xC8\x80\xD5\x61\x19\x1B\x00\x00\x0C" "\x00\x00\x00\x00\x00"
"\xF1\x21\x01\x00\x97\xF1\x17\x18\x00\x00\x08" "\x00\x00\x00\x00\x00"
"\x32\x16\x87\x80\xA1\x7D\x10\x33\x00\x00\x08" "\x00\x00\x00\x00\x00"
"\x01\x12\x4F\x00\x71\x52\x53\x7C\x00\x00\x0A" "\x00\x00\x00\x00\x00"
"\x02\x03\x8D\x00\xD7\xF5\x37\x18\x00\x00\x04" "\x00\x00\x00\x00\x00"
"\x21\x21\xD1\x00\xA3\xA4\x46\x25\x00\x00\x0A" "\x00\x00\x00\x00\x00"
"\x22\x22\x0F\x00\xF6\xF6\x95\x36\x00\x00\x0A" "\x00\x00\x00\x00\x00"
"\xE1\xE1\x00\x00\x44\x54\x24\x34\x02\x02\x07" "\x00\x00\x00\x00\x00"
"\xA5\xB1\xD2\x80\x81\xF1\x03\x05\x00\x00\x02" "\x00\x00\x00\x00\x00"
"\x71\x22\xC5\x00\x6E\x8B\x17\x0E\x00\x00\x02" "\x00\x00\x00\x00\x00"
"\x32\x21\x16\x80\x73\x75\x24\x57\x00\x00\x0E" "\x00\x00\x00\x00\x00";

std::string MusicType_CMF::code() const
{
	return "cmf-creativelabs";
}

std::string MusicType_CMF::friendlyName() const
{
	return "Creative Labs Music File";
}

std::vector<std::string> MusicType_CMF::fileExtensions() const
{
	return {
		"cmf",
	};
}

MusicType::Caps MusicType_CMF::caps() const
{
	return
		Caps::InstOPL
		| Caps::InstOPLRhythm
		| Caps::HasEvents
	;
}

MusicType::Certainty MusicType_CMF::isInstance(stream::input& content) const
{
	// File too short
	// TESTED BY: mus_cmf_creativelabs_isinstance_c04
	if (content.size() < 4+2+2+2+2+2+2+2+2+2) return Certainty::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_cmf_creativelabs_isinstance_c01
	char sig[4];
	content.seekg(0, stream::start);
	content.read(sig, 4);
	if (strncmp(sig, "CTMF", 4) != 0) return Certainty::DefinitelyNo;

	// Make sure the header says it's version 1.0 or 1.1
	// TESTED BY: mus_cmf_creativelabs_isinstance_c02 (wrong ver)
	// TESTED BY: mus_cmf_creativelabs_isinstance_c03 (1.0)
	uint16_t ver;
	content >> u16le(ver);
	if ((ver != 0x100) && (ver != 0x101)) return Certainty::DefinitelyNo;

	// TESTED BY: mus_cmf_creativelabs_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_CMF::read(stream::input& content, SuppData& suppData) const
{
	stream::len lenData = content.size();

	// Skip CTMF header.  This is an absolute seek as it will be by far the most
	// common situation and avoids a lot of complexity because the header includes
	// absolute file offsets, which we thus won't have to adjust.
	content.seekg(4, stream::start); // skip CTMF header

	uint16_t ver, offInst, ticksPerQuarter, ticksPerSecond;
	uint16_t offMusic, offTitle, offComposer, offRemarks;
	content
		>> u16le(ver)
		>> u16le(offInst)
		>> u16le(offMusic)
		>> u16le(ticksPerQuarter)
		>> u16le(ticksPerSecond)
		>> u16le(offTitle)
		>> u16le(offComposer)
		>> u16le(offRemarks)
	;

	// Highway Hunter has weird CMF files with invalid metadata offsets (not to
	// mention chunks of random data including MTrk chunks and Microsoft
	// copyright messages!)
	if (offTitle > lenData) {
		std::cerr << "Warning: CMF 'title' field starts past EOF, ignoring.\n";
		offTitle = 0;
	}
	if (offComposer > lenData) {
		std::cerr << "Warning: CMF 'composer' field starts past EOF, ignoring.\n";
		offComposer = 0;
	}
	if (offRemarks > lenData) {
		std::cerr << "Warning: CMF 'remarks' field starts past EOF, ignoring.\n";
		offRemarks = 0;
	}

	// Skip channel-in-use table as we don't need it
	content.seekg(16, stream::cur);

	// Rest of header depends on file version
	uint16_t numInstruments;
	switch (ver) {
		case 0x100: {
			uint8_t temp;
			content
				>> u8(temp)
			;
			numInstruments = temp;
			break;
		}
		default: // do this so you can force-open an unknown version
			std::cerr << "Warning: Unknown CMF version " << (int)(ver >> 8) << '.'
				<< (int)(ver & 0xFF) << ", proceeding as if v1.1" << std::endl;
			// fall through
		case 0x101:
			content
				>> u16le(numInstruments)
			;
			// Skip uint16le tempo value (unknown use)
			content.seekg(2, stream::cur);
			break;
	}

	// Process the MIDI data
	content.seekg(offMusic, stream::start);
	Tempo initialTempo;
	initialTempo.hertz(ticksPerSecond);
	initialTempo.ticksPerQuarterNote(ticksPerQuarter);
	std::unique_ptr<Music> music = midiDecode(content, MIDIFlags::UsePatchIndex
		| MIDIFlags::CMFExtensions, initialTempo);

	unsigned int oplChannel = 0;
	bool opl3 = false;
	for (auto& ti : music->trackInfo) {
		if (ti.channelIndex >= 11) {
			ti.channelType = TrackInfo::ChannelType::OPLPerc;
			ti.channelIndex = 15 - ti.channelIndex;
		} else {
			ti.channelType = TrackInfo::ChannelType::OPL;
			ti.channelIndex = oplChannel++;
			if (oplChannel == 6) {
				oplChannel = 9;
				opl3 = true;
			}
			oplChannel %= 18;
		}
	}

	// Set standard CMF settings
	auto& configTrack = music->patterns.at(0).at(0);
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableDeepTremolo;
		ev->value = 1;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 0, te);
	}
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableDeepVibrato;
		ev->value = 1;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 1, te);
	}
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableWaveSel;
		ev->value = 1;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 2, te);
	}

	if (opl3) {
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableOPL3;
		ev->value = 1;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 0, te);
	}

	// Read the instruments
	auto oplBank = std::make_shared<PatchBank>();
	oplBank->reserve(numInstruments);
	content.seekg(offInst, stream::start);
	for (unsigned int i = 0; i < numInstruments; i++) {
		auto patch = std::make_shared<OPLPatch>();
		content >> instrumentSBI(*patch);
		oplBank->push_back(patch);
	}

	// Load the default instruments
	int genericMapping[CMF_NUM_DEFAULT_INSTRUMENTS];
	auto oplBankDefault = std::make_shared<PatchBank>();
	oplBankDefault->reserve(CMF_NUM_DEFAULT_INSTRUMENTS);
	stream::string ss(std::string(CMF_DEFAULT_PATCHES, sizeof(CMF_DEFAULT_PATCHES) - 1));
	for (unsigned int i = 0; i < CMF_NUM_DEFAULT_INSTRUMENTS; i++) {
		auto patch = std::make_shared<OPLPatch>();
		ss >> instrumentSBI(*patch);
		oplBankDefault->push_back(patch);
		genericMapping[i] = -1; // not yet assigned
	}

	// Run through the song events, and create each patch based on how it is used
	int instMapping[6][128];
	for (unsigned int t = 0; t < 6; t++) {
		for (unsigned int p = 0; p < 128; p++) instMapping[t][p] = -1;
	}

	// Run through each event and change the MIDI-like instrument numbers to
	// optimised values.  This will condense the 127 available standard
	// instruments down into only those which are used, as well as duplicating
	// any instrument used for both percussion and normal rhythm.
	for (auto& patternIndex : music->patternOrder) {
		auto& pattern = music->patterns[patternIndex];

		// For each track
		unsigned int trackIndex = 0;
		for (auto& pt : pattern) {
			auto& ti = music->trackInfo[trackIndex];

			// For each event in the track
			for (auto& te : pt) {
				auto nev = dynamic_cast<NoteOnEvent *>(te.event.get());
				if (!nev) continue;

				// The velocity is loaded directly into the OPL chip, which uses a
				// logarithmic volume scale rather than MIDI's linear one.
				nev->velocity = 255 - log_volume_to_lin_velocity(255 - nev->velocity, 255);

				// Figure out which rhythm instrument (if any) this is
				// If there's a mapping (CMF instrument block # to patchbank #), use that, otherwise:
				int targetRhythm;
				if (ti.channelType == TrackInfo::ChannelType::OPLPerc) {
					targetRhythm = ti.channelIndex + 1;
				} else {
					targetRhythm = 0;
				}

				// Figure out what CMF instrument number to play
				auto midiInst = dynamic_cast<const MIDIPatch*>(
					music->patches->at(nev->instrument).get());
				assert(midiInst);
				unsigned int oplIndex = midiInst->midiPatch;
				int& curTarget = instMapping[targetRhythm][oplIndex];
				if (curTarget == -1) {
					// Don't have a mapping yet

					// See if this is a patch for a default instrument.  We can't use
					// oplBank->size() as it will increase as we add default instruments to
					// the bank, so we use numInstruments which should always be the number of
					// custom instruments only.
					if (oplIndex >= numInstruments) {
						// Using one of the generic instruments
						unsigned int realInst = oplIndex % CMF_NUM_DEFAULT_INSTRUMENTS;
						if (genericMapping[realInst] <= 0) {
							// Need to add this
							oplIndex = genericMapping[realInst] = oplBank->size();
							oplBank->push_back(oplBankDefault->at(realInst));
						} else {
							oplIndex = genericMapping[realInst];
						}
					}

					curTarget = oplIndex;
				}
				// Use the previously defined mapping
				assert(curTarget >= 0);
				nev->instrument = curTarget;
			}
			trackIndex++;
		}
	}

	// Disregard the MIDI patches and use the OPL ones
	music->patches = oplBank;

	// Read metadata
	{
		auto& a = music->addAttribute();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = CMF_ATTR_MAXLEN;
		if (offTitle) {
			content.seekg(offTitle, stream::start);
			content >> nullTerminated(a.textValue, CMF_ATTR_MAXLEN);
		}
	}
	{
		auto& a = music->addAttribute();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_AUTHOR;
		a.desc = "Song composer";
		a.textMaxLength = CMF_ATTR_MAXLEN;
		if (offComposer) {
			content.seekg(offComposer, stream::start);
			content >> nullTerminated(a.textValue, CMF_ATTR_MAXLEN);
		}
	}
	{
		auto& a = music->addAttribute();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_COMMENT;
		a.desc = "Song remarks";
		a.textMaxLength = CMF_ATTR_MAXLEN;
		if (offRemarks) {
			content.seekg(offRemarks, stream::start);
			content >> nullTerminated(a.textValue, CMF_ATTR_MAXLEN);
		}
	}

	// Swap operators for required percussive patches
	oplDenormalisePerc(*music, OPLNormaliseType::CarFromMod);

	return music;
}

void MusicType_CMF::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	requirePatches<OPLPatch>(*music.patches);
	if (music.patches->size() >= MIDI_PATCHES) {
		throw bad_patch("CMF files have a maximum of 128 instruments.");
	}

	// Swap operators for required percussive patches
	auto patches = oplNormalisePerc(music, OPLNormaliseType::CarFromMod);

	content.write(
		"CTMF"
		"\x01\x01" // version 1.1
	, 6);

	uint16_t offNext = 20 + 16 + 4;
	uint16_t lenText[3], offText[3];

	// Get lengths from metadata
	for (unsigned int i = 0; i < 3; i++) {
		lenText[i] = music.attributes().at(i).textValue.length();
	}

	for (int i = 0; i < 3; i++) {
		if (lenText[i]) {
			offText[i] = offNext;
			offNext += lenText[i] + 1;
		} else {
			offText[i] = 0;
		}
	}
	uint16_t offInst = offNext;
	uint16_t numInstruments = patches->size();
	offNext += 16 * numInstruments;
	uint16_t offMusic = offNext;

	content
		<< u16le(offInst)
		<< u16le(offMusic)
		<< u16le(music.initialTempo.ticksPerQuarterNote())
		<< u16le(music.initialTempo.hertz())
	;

	// Write offset of title, composer and remarks
	for (int i = 0; i < 3; i++) {
		content << u16le(offText[i]);
	}

	// Placeholder channel-in-use table (overwritten later)
	for (int i = 0; i < 16; i++) content << u8(1);

	content
		<< u16le(numInstruments)
		<< u16le(music.initialTempo.bpm())
	;

	// Write title, composer and remarks strings here (null terminated)
	for (unsigned int i = 0; i < 3; i++) {
		if (lenText[i] > 0) {
			content << nullTerminated(music.attributes().at(i).textValue,
				CMF_ATTR_MAXLEN);
		}
	}

	// Create a new TrackInfo that moves everything onto the correct MIDI channels
	std::vector<TrackInfo> midiTrackInfo;
	for (auto& ti : music.trackInfo) {
		TrackInfo nti;
		nti.channelType = TrackInfo::ChannelType::MIDI;
		if (ti.channelType == TrackInfo::ChannelType::OPLPerc) {
			nti.channelIndex = 15 - ti.channelIndex;
		} else {
			nti = ti;
		}
		if (nti.channelIndex > 15) {
			throw format_limitation("CMF files can only have up to 16 channels.");
		}
		midiTrackInfo.push_back(nti);
	}
	auto musicMIDI = std::make_shared<Music>();
	*musicMIDI = music;
	musicMIDI->trackInfo = midiTrackInfo;
	// musicMIDI is now the same as music, but with the CMF MIDI track assignment
	// for midiEncode to use to place percussive events on the right channels.

	for (int i = 0; i < numInstruments; i++) {
		auto patch = dynamic_cast<const OPLPatch*>(patches->at(i).get());
		assert(patch);
		content << instrumentSBI(*patch);
	}

	// Call the generic OPL writer.
	bool channelsUsed[MIDI_CHANNEL_COUNT];
	MIDIFlags midiFlags = MIDIFlags::UsePatchIndex | MIDIFlags::CMFExtensions;
	if (flags & MusicType::WriteFlags::IntegerNotesOnly) {
		midiFlags |= MIDIFlags::IntegerNotesOnly;
	}
	midiEncode(content, *musicMIDI, midiFlags, channelsUsed,
		EventHandler::Order_Row_Track, NULL);

	// Set final filesize to this
	content.truncate_here();

	// Update the channel-in-use table
	uint8_t channelsInUse[MIDI_CHANNEL_COUNT];
	for (unsigned int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
		channelsInUse[i] = channelsUsed[i] ? 1 : 0;
	}
	content.seekp(20, stream::start);
	content.write((char *)channelsInUse, 16);

	return;
}

SuppFilenames MusicType_CMF::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_CMF::supportedAttributes() const
{
	std::vector<Attribute> items;
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = CMF_ATTR_MAXLEN;
	}
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_AUTHOR;
		a.desc = "Song composer";
		a.textMaxLength = CMF_ATTR_MAXLEN;
	}
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_COMMENT;
		a.desc = "Song remarks";
		a.textMaxLength = CMF_ATTR_MAXLEN;
	}
	return items;
}
