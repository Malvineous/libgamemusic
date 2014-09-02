/**
 * @file   mus-cdfm-zone66.cpp
 * @brief  Support for Renaissance's CDFM format used in Zone 66.
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
#include "mus-cdfm-zone66.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of storage channels in CDFM file
#define CDFM_CHANNEL_COUNT (4+9)

/// Fixed module tempo/bpm for all songs (but module 'speed' can change)
#define CDFM_TEMPO 144

std::string MusicType_CDFM::getCode() const
{
	return "cdfm-zone66";
}

std::string MusicType_CDFM::getFriendlyName() const
{
	return "Renaissance CDFM";
}

std::vector<std::string> MusicType_CDFM::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("z66");
	vcExtensions.push_back("670");
	return vcExtensions;
}

unsigned long MusicType_CDFM::getCaps() const
{
	return
		InstEmpty
		| InstOPL
		| InstPCM
		| HasEvents
		| HasPatterns
		| HasLoopDest
	;
}

MusicType::Certainty MusicType_CDFM::isInstance(stream::input_sptr input) const
{
	stream::len fileSize = input->size();
	input->seekg(0, stream::start);

	uint8_t speed, orderCount, patternCount, numDigInst, numOPLInst, loopDest;
	uint32_t sampleOffset;
	input
		>> u8(speed)
		>> u8(orderCount)
		>> u8(patternCount)
		>> u8(numDigInst)
		>> u8(numOPLInst)
		>> u8(loopDest)
		>> u32le(sampleOffset)
	;
	if (loopDest >= orderCount) {
		// Loop target is past end of song
		// TESTED BY: mus_cdfm_zone66_isinstance_c01
		return MusicType::DefinitelyNo;
	}

	uint8_t patternOrder[256];
	input->read(patternOrder, orderCount);
	for (int i = 0; i < orderCount; i++) {
		if (patternOrder[i] >= patternCount) {
			// Sequence specifies invalid pattern
			// TESTED BY: mus_cdfm_zone66_isinstance_c02
			return MusicType::DefinitelyNo;
		}
	}

	// Read the song data
	stream::pos patternStart =
		10                     // sizeof fixed-length part of header
		+ 1 * orderCount  // one byte for each pattern in the sequence
		+ 4 * patternCount      // one uint32le for each pattern's offset
		+ 16 * numDigInst      // PCM instruments
		+ 11 * numOPLInst      // OPL instruments
	;

	for (int i = 0; i < patternCount; i++) {
		uint32_t patternOffset;
		input >> u32le(patternOffset);
		if (patternStart + patternOffset >= fileSize) {
			// Pattern data offset is past EOF
			// TESTED BY: mus_cdfm_zone66_isinstance_c03
			return MusicType::DefinitelyNo;
		}
	}

	// TESTED BY: mus_cdfm_zone66_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_CDFM::read(stream::input_sptr input, SuppData& suppData) const
{
	MusicPtr music(new Music());
	PatchBankPtr patches(new PatchBank());
	music->patches = patches;

	// All CDFM files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 4;
	music->ticksPerTrack = 64;

	for (unsigned int c = 0; c < CDFM_CHANNEL_COUNT; c++) {
		TrackInfo t;
		if (c < 4) {
			t.channelType = TrackInfo::PCMChannel;
			t.channelIndex = c;
		} else if (c < 13) {
			t.channelType = TrackInfo::OPLChannel;
			t.channelIndex = c - 4;
		} else {
			t.channelType = TrackInfo::UnusedChannel;
			t.channelIndex = c - 13;
		}
		music->trackInfo.push_back(t);
	}

	input->seekg(0, stream::start);

	uint8_t speed, orderCount, patternCount, numDigInst, numOPLInst, loopDest;
	uint32_t sampleOffset;
	input
		>> u8(speed)
		>> u8(orderCount)
		>> u8(patternCount)
		>> u8(numDigInst)
		>> u8(numOPLInst)
		>> u8(loopDest)
		>> u32le(sampleOffset)
	;
	music->loopDest = loopDest;
	music->initialTempo.module(speed, CDFM_TEMPO);

	for (unsigned int i = 0; i < orderCount; i++) {
		uint8_t order;
		input >> u8(order);
		if (order < 0xFE) {
			music->patternOrder.push_back(order);
		} else {
			/// @todo: See if this is part of the CDFM spec, like it is for S3M
			std::cout << "CDFM: Got pattern >= 254, handle this!\n";
		}
	}

	std::vector<uint16_t> ptrPatterns;
	ptrPatterns.reserve(patternCount);
	for (unsigned int i = 0; i < patternCount; i++) {
		uint16_t ptrPattern;
		input >> u32le(ptrPattern);
		ptrPatterns.push_back(ptrPattern);
	}

	// Read instruments
	patches->reserve(numDigInst + numOPLInst);

	for (int i = 0; i < numDigInst; i++) {
		uint32_t unknown;
		PCMPatchPtr patch(new PCMPatch());
		input
			>> u32le(unknown)
			>> u32le(patch->lenData)
			>> u32le(patch->loopStart)
			>> u32le(patch->loopEnd)
		;
		patch->defaultVolume = 255;
		patch->sampleRate = 8287;
		patch->bitDepth = 8;
		patch->numChannels = 1;
		if (patch->loopEnd == 0x00FFFFFF) patch->loopEnd = 0; // no loop
		patches->push_back(patch);
		if (unknown != 0) {
			std::cout << "CDFM: Got file with unknown value as " << (int)unknown << "\n";
		}
	}

	uint8_t inst[11];
	for (int i = 0; i < numOPLInst; i++) {
		OPLPatchPtr patch(new OPLPatch());
		patch->defaultVolume = 255;
		input->read(inst, 11);

		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = (inst[op * 5 + 1] >> 7) & 1;
			o->enableVibrato = (inst[op * 5 + 1] >> 6) & 1;
			o->enableSustain = (inst[op * 5 + 1] >> 5) & 1;
			o->enableKSR     = (inst[op * 5 + 1] >> 4) & 1;
			o->freqMult      =  inst[op * 5 + 1] & 0x0F;
			o->scaleLevel    =  inst[op * 5 + 2] >> 6;
			o->outputLevel   =  inst[op * 5 + 2] & 0x3F;
			o->attackRate    =  inst[op * 5 + 3] >> 4;
			o->decayRate     =  inst[op * 5 + 3] & 0x0F;
			o->sustainRate   =  inst[op * 5 + 4] >> 4;
			o->releaseRate   =  inst[op * 5 + 4] & 0x0F;
			o->waveSelect    =  inst[op * 5 + 5] & 0x07;
			o = &patch->c;
		}
		patch->feedback    = (inst[0] >> 1) & 0x07;
		patch->connection  =  inst[0] & 1;
		patch->rhythm      = 0;

		patches->push_back(patch);
	}

	// Read the song data
	music->patterns.reserve(patternCount);
	bool firstPattern = true;
	stream::pos patternStart =
		10                     // sizeof fixed-length part of header
		+ 1 * orderCount  // one byte for each pattern in the sequence
		+ 4 * patternCount      // one uint32le for each pattern's offset
		+ 16 * numDigInst      // PCM instruments
		+ 11 * numOPLInst      // OPL instruments
	;
	assert(input->tellg() == patternStart);

	unsigned int patternIndex = 0;
	const unsigned int& firstOrder = music->patternOrder[0];
	for (std::vector<uint16_t>::const_iterator
		i = ptrPatterns.begin(); i != ptrPatterns.end(); i++, patternIndex++
	) {
		// Jump to the start of the pattern data
		input->seekg(patternStart + *i, stream::start);
		PatternPtr pattern(new Pattern());
		for (unsigned int track = 0; track < CDFM_CHANNEL_COUNT; track++) {
			TrackPtr t(new Track());
			pattern->push_back(t);
			if (
				firstPattern
				&& (patternIndex == firstOrder)
				&& (track == 4)
			) { // first OPL track in pattern being played first in order list
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

				firstPattern = false;
			}
		}

		// Process the events
		uint8_t cmd;
		unsigned int lastDelay[CDFM_CHANNEL_COUNT];
		for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) lastDelay[i] = 0;
		do {
			input >> u8(cmd);
			uint8_t channel = cmd & 0x0F;
			if (channel >= CDFM_CHANNEL_COUNT) {
				throw stream::error(createString("CDFM: Channel " << (int)channel
					<< " out of range, max valid channel is " << CDFM_CHANNEL_COUNT - 1));
			}
			TrackPtr& track = pattern->at(channel);
			switch (cmd & 0xF0) {
				case 0x00: { // note on
					uint8_t freq, inst_vel;
					input >> u8(freq) >> u8(inst_vel);
					freq %= 128; // notes larger than this wrap

					TrackEvent te;
					te.delay = lastDelay[channel];
					lastDelay[channel] = 0;
					NoteOnEvent *ev = new NoteOnEvent();
					te.event.reset(ev);

					unsigned int oct = (freq >> 4);
					unsigned int note = freq & 0x0F;

					ev->instrument = inst_vel >> 4;
					if (channel > 3) {
						// This is an OPL channel so increment instrument index
						ev->instrument += numDigInst;
					} else {
						// PCM instruments use C-2 not C-4 so transpose them
						oct += 2;
					}
					ev->milliHertz = midiToFreq((oct + 1) * 12 + note);
					ev->velocity = ((inst_vel & 0x0F) << 4) | (inst_vel & 0x0F);

					track->push_back(te);
					break;
				}
				case 0x20: { // set volume
					uint8_t vol;
					input >> u8(vol);
					vol &= 0x0F;

					if (vol == 0) {
						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						NoteOffEvent *ev = new NoteOffEvent();
						te.event.reset(ev);
						track->push_back(te);
					} else {
						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						EffectEvent *ev = new EffectEvent();
						te.event.reset(ev);
						ev->type = EffectEvent::Volume;
						ev->data = (vol << 4) | vol; // 0-15 -> 0-255
						track->push_back(te);
					}
					break;
				}
				case 0x40: { // delay
					uint8_t data;
					input >> u8(data);
					for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) {
						lastDelay[i] += data;
					}
					break;
				}
				case 0x60: // end of pattern
					// Value will cause while loop to exit below
					break;
				default:
					std::cerr << "mus-cdfm-zone66: Unknown event " << (int)(cmd & 0xF0)
						<< " at offset " << input->tellg() - 1 << std::endl;
					break;
			}
		} while (cmd != 0x60);

		// Write out any trailing delays
		for (unsigned int t = 0; t < CDFM_CHANNEL_COUNT; t++) {
			if (lastDelay[t]) {
				TrackPtr& track = pattern->at(t);
				TrackEvent te;
				te.delay = lastDelay[t];
				ConfigurationEvent *ev = new ConfigurationEvent();
				te.event.reset(ev);
				ev->configType = ConfigurationEvent::None;
				ev->value = 0;
				track->push_back(te);
			}
		}
		music->patterns.push_back(pattern);
	}

	// Load the PCM samples
	input->seekg(sampleOffset, stream::start);
	for (int i = 0; i < numDigInst; i++) {
		PCMPatchPtr patch = boost::static_pointer_cast<PCMPatch>(patches->at(i));
		patch->data.reset(new uint8_t[patch->lenData]);
		input->read(patch->data.get(), patch->lenData);
	}

	return music;
}

void MusicType_CDFM::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	assert(false);
	return;
}

SuppFilenames MusicType_CDFM::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_CDFM::getMetadataList() const
{
	// No supported metadata
	return Metadata::MetadataTypes();
}
