/**
 * @file   mus-tbsa-doofus.hpp
 * @brief  Support for The Bone Shaker Architect used in Doofus.
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
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

#include <boost/pointer_cast.hpp>
#include <camoto/iostream_helpers.hpp>
#include <camoto/util.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp> // midiToFreq
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>
#include <camoto/gamemusic/opl-util.hpp>
#include "mus-tbsa-doofus.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of storage channels in TBSA file
#define TBSA_CHANNEL_COUNT 11

/// Fixed module tempo/bpm for all songs (but module 'speed' can change)
#define TBSA_TEMPO 66

// Safety limits to avoid infinite loops
#define TBSA_MAX_ORDERS 256
#define TBSA_MAX_INSTS  256
#define TBSA_MAX_PATTS  4096
#define TBSA_MAX_ORD_LEN 256
#define TBSA_MAX_PATSEG_LEN 4096

std::string MusicType_TBSA::getCode() const
{
	return "tbsa-doofus";
}

std::string MusicType_TBSA::getFriendlyName() const
{
	return "The Bone Shaker Architect";
}

std::vector<std::string> MusicType_TBSA::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("bsa");
	return vcExtensions;
}

unsigned long MusicType_TBSA::getCaps() const
{
	return
		InstEmpty
		| InstOPL
		| HasEvents
		| HasPatterns
	;
}

MusicType::Certainty MusicType_TBSA::isInstance(stream::input_sptr input) const
{
	input->seekg(0, stream::start);
	std::string sig;
	input >> fixedLength(sig, 8);

	// Invalid signature
	// TESTED BY: mus_tbsa_doofus_isinstance_c01
	if (sig.compare("TBSA0.01") != 0) return MusicType::DefinitelyNo;

	// TESTED BY: mus_tbsa_doofus_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_TBSA::read(stream::input_sptr input, SuppData& suppData) const
{
	MusicPtr music(new Music());
	PatchBankPtr patches(new PatchBank());
	music->patches = patches;

	// All TBSA files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 2;
	music->ticksPerTrack = 64;

	for (unsigned int c = 0; c < TBSA_CHANNEL_COUNT; c++) {
		TrackInfo t;
		if (c < 6) {
			t.channelType = TrackInfo::OPLChannel;
			t.channelIndex = c;
		} else if (c < 11) {
			t.channelType = TrackInfo::OPLPercChannel;
			t.channelIndex = 4 - (c - 6);
		} else {
			t.channelType = TrackInfo::UnusedChannel;
			t.channelIndex = c - 11;
		}
		music->trackInfo.push_back(t);
	}

	input->seekg(8, stream::start);
	uint16_t offOrderPtrs, offUnknown2, offUnknown3, offUnknown4, offInstPtrs;
	uint16_t offPattPtrs;
	input
		>> u16le(offOrderPtrs)
		>> u16le(offUnknown2)
		>> u16le(offUnknown3)
		>> u16le(offUnknown4)
		>> u16le(offInstPtrs)
		>> u16le(offPattPtrs)
	;
	music->initialTempo.module(6, TBSA_TEMPO);

	// Read the order list pointers
	std::vector<stream::pos> offOrderLists;
	input->seekg(offOrderPtrs, stream::start);
	for (unsigned int i = 0; i < TBSA_MAX_ORDERS; i++) {
		stream::pos p;
		input >> u16le(p);
		if (p == 0xFFFF) break;
		offOrderLists.push_back(p);
	}

	// Read the instrument pointers
	std::vector<stream::pos> offInsts;
	input->seekg(offInstPtrs, stream::start);
	for (unsigned int i = 0; i < TBSA_MAX_INSTS; i++) {
		stream::pos p;
		input >> u16le(p);
		if (p == 0xFFFF) break;
		offInsts.push_back(p);
	}

	// Read the pattern-segment pointers
	std::vector<stream::pos> offPatts;
	input->seekg(offPattPtrs, stream::start);
	for (unsigned int i = 0; i < TBSA_MAX_PATTS; i++) {
		stream::pos p;
		input >> u16le(p);
		if (p == 0xFFFF) break;
		offPatts.push_back(p);
	}

	// Read the order pointers
	std::vector<stream::pos> offOrders;
	for (std::vector<stream::pos>::const_iterator
		i = offOrderLists.begin(); i != offOrderLists.end(); i++
	) {
		input->seekg(*i, stream::start);
		uint8_t orderCount, unknown;
		input
			>> u8(orderCount)
			>> u8(unknown)
		;
		for (unsigned int j = 0; j < orderCount; j++) {
			stream::pos p;
			input >> u16le(p);
			offOrders.push_back(p);
		}
	}

	// Read the instruments
	for (std::vector<stream::pos>::const_iterator
		i = offInsts.begin(); i != offInsts.end(); i++
	) {
		input->seekg(*i, stream::start);
		uint8_t inst[20];
		input->read(inst, 20);

		OPLPatchPtr patch(new OPLPatch());
		patch->defaultVolume = 255;

		patch->m.enableTremolo = inst[9] & 1;
		patch->m.enableVibrato = inst[6] & 1;
		patch->m.enableSustain = inst[2] & 1;
		patch->m.enableKSR     = inst[3] & 1;
		patch->m.freqMult      = inst[4] & 0x0F;
		patch->m.scaleLevel    = inst[8] & 3;
		patch->m.outputLevel   = inst[7] + 2;
		patch->m.attackRate    = inst[0] >> 4;
		patch->m.decayRate     = inst[0] & 0x0F;
		patch->m.sustainRate   = inst[1] >> 4;
		patch->m.releaseRate   = inst[1] & 0x0F;
		patch->m.waveSelect    = inst[11] & 0x07;

		patch->c.enableTremolo = (inst[15] >> 3) & 1; // use overflow from KSR field
		patch->c.enableVibrato = (inst[15] >> 2) & 1; // use overflow from KSR field
		patch->c.enableSustain = inst[14] & 1;
		patch->c.enableKSR     = inst[15] & 1;
		patch->c.freqMult      = inst[16] & 0x0F;
		patch->c.scaleLevel    = inst[18] & 3;
		patch->c.outputLevel   = inst[17] + 2;
		patch->c.attackRate    = inst[12] >> 4;
		patch->c.decayRate     = inst[12] & 0x0F;
		patch->c.sustainRate   = inst[13] >> 4;
		patch->c.releaseRate   = inst[13] & 0x0F;
		patch->c.waveSelect    = inst[19] & 0x07;

		patch->feedback         = inst[5] & 0x07;
		patch->connection       = inst[10] & 1;
		patch->rhythm           = 0;

		patches->push_back(patch);
	}

	// Read the orders and patterns
	std::map<std::string, unsigned int> patternCodes;

	bool finished = false;
	for (unsigned int order = 0; order < TBSA_MAX_ORD_LEN; order++) {
		std::vector<uint8_t> patsegList;
		for (std::vector<stream::pos>::const_iterator
			i = offOrders.begin(); i != offOrders.end(); i++
		) {
			input->seekg(*i + order, stream::start);
			uint8_t patsegIndex;
			input >> u8(patsegIndex);
			if (patsegIndex == 0xFE) {
				finished = true;
				break;
			}
			patsegList.push_back(patsegIndex);
		}
		if (finished) break;

		// See if this combination of pattern segments has been loaded before
		std::string key;
		for (std::vector<uint8_t>::const_iterator
			p = patsegList.begin(); p != patsegList.end(); p++
		) {
			key.append((char)*p, 1);
		}
		std::map<std::string, unsigned int>::const_iterator match = patternCodes.find(key);
		if (match != patternCodes.end()) {
			// Yes it has, so just reference that pattern instead
			music->patternOrder.push_back(match->second);
			continue;
		}
		// No, so add it to the list in case it gets referenced again
		unsigned long patternIndex = music->patterns.size();
		patternCodes[key] = patternIndex;
		music->patternOrder.push_back(patternIndex);

		PatternPtr pattern(new Pattern());
		music->patterns.push_back(pattern);

		// patsegList now contains a list of all the segments, one per track, that
		// make up this pattern.
		unsigned int trackIndex = 0;
		for (std::vector<uint8_t>::const_iterator
			p = patsegList.begin(); p != patsegList.end(); p++, trackIndex++
		) {
			if (*p >= offPatts.size()) {
				std::cerr << "TBSA: Tried to load pattern segment " << *p
					<< " but there are only " << offPatts.size()
					<< " segments in the file!";
				continue;
			}
			TrackPtr t(new Track());
			pattern->push_back(t);

			if ((patternIndex == 0) && (trackIndex == 0)) {
				// Set standard settings
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableOPL3;
					ev->value = 0;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableDeepTremolo;
					ev->value = 0;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableDeepVibrato;
					ev->value = 0;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableWaveSel;
					ev->value = 1;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableRhythm;
					ev->value = 1;
					t->push_back(te);
				}
			}

			// Decode the pattern segment
			input->seekg(offPatts[*p], stream::start);
			int lastVolume = -1, lastInstrument = 0, lastIncrement = 1;
			double lastShift = 0.0;
			int delay = 0;
			for (int row = 0; row < TBSA_MAX_PATSEG_LEN; row++) {
				uint8_t code;
				input >> u8(code);
				if (code == 0xFF) break;
				uint8_t command = code >> 5;
				uint8_t value = code & 0x1F;

				switch (command) {
					case 0:   // 0x,1x
					case 1:   // 2x,3x
					case 2: { // 4x,5x: note
						TrackEvent te;
						te.delay = delay;
						delay = lastIncrement;
						NoteOnEvent *ev = new NoteOnEvent();
						te.event.reset(ev);

						ev->instrument = lastInstrument;
						ev->milliHertz = midiToFreq(code+12+lastShift);
						ev->velocity = lastVolume;

						t->push_back(te);
						break;
					}
					case 3:   // 6x,7x: ?
						break;
					case 4:   // 8x,9x: set instrument
						lastInstrument = value;
						break;
					case 5:   // Ax,Bx: set increment
						lastIncrement = value + 1;
						break;
					case 6:   // Cx,Dx: set increment
						lastIncrement = 32+value + 1;
						break;
					case 7:   // Ex,Fx: ?
						switch (code) {
							case 0xF4: // fine tune down
							case 0xF5:
							case 0xF6:
							case 0xF7:
							case 0xF8:
							case 0xF9:
							case 0xFA:
							case 0xFB:
							case 0xFC:
								lastShift = 0.1 * (code - 0xFD) * 0.25;
								break;
							case 0xFD: // set volume
								input >> u8(value);
								lastVolume = log_volume_to_lin_velocity(value, 127);
								break;
							case 0xFE: // dummy event to take up a row
								delay += lastIncrement;
								break;
							default:
								std::cout << "TBSA: Unrecognised extended command: "
									<< std::hex << (int)code << std::dec << "\n";
								break;
						}
						break;
					default:
						std::cout << "TBSA: Unrecognised command: "
							<< std::hex << (int)code << std::dec << "\n";
						break;
				}
			}

		}
	}
	oplDenormalisePerc(music, OPLPerc_CarFromMod);
	return music;
}

void MusicType_TBSA::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	assert(false);
	return;
}

SuppFilenames MusicType_TBSA::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_TBSA::getMetadataList() const
{
	// No supported metadata
	return Metadata::MetadataTypes();
}
