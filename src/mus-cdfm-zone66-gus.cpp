/**
 * @file  mus-cdfm-zone66-gus.cpp
 * @brief Support for Renaissance's CDFM format used in Zone 66 (GUS variant).
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

#include <camoto/util.hpp> // make_unique
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp> // midiToFreq
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>
#include "mus-cdfm-zone66-gus.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of storage channels in CDFM file
#define CDFM_CHANNEL_COUNT 16

/// Fixed module tempo/bpm for all songs (but module 'speed' can change)
#define CDFM_TEMPO 144

std::string MusicType_CDFM_GUS::code() const
{
	return "cdfm-zone66-gus";
}

std::string MusicType_CDFM_GUS::friendlyName() const
{
	return "Renaissance CDFM (GUS)";
}

std::vector<std::string> MusicType_CDFM_GUS::fileExtensions() const
{
	return {
		"z66",
		"670",
	};
}

MusicType::Caps MusicType_CDFM_GUS::caps() const
{
	return
		Caps::InstPCM
		| Caps::HasEvents
		| Caps::HasPatterns
		| Caps::HasLoopDest
	;
}

MusicType::Certainty MusicType_CDFM_GUS::isInstance(stream::input& content) const
{
	stream::len fileSize = content.size();
	content.seekg(0, stream::start);

	uint8_t speed, orderCount, patternCount, numDigInst, loopDest;
	uint32_t sampleOffset;
	content
		>> u32le(sampleOffset)
		>> u8(speed)
		>> u8(orderCount)
		>> u8(patternCount)
		>> u8(numDigInst)
		>> u8(loopDest)
	;
	if (sampleOffset >= fileSize) {
		// Sample data past EOF
		// TESTED BY: mus_cdfm_zone66_gus_isinstance_c01
		return Certainty::DefinitelyNo;
	}

	if (loopDest >= orderCount) {
		// Loop target is past end of song
		// TESTED BY: mus_cdfm_zone66_gus_isinstance_c02
		return Certainty::DefinitelyNo;
	}

	uint8_t patternOrder[256];
	content.read(patternOrder, orderCount);
	for (int i = 0; i < orderCount; i++) {
		if (patternOrder[i] >= patternCount) {
			// Sequence specifies invalid pattern
			// TESTED BY: mus_cdfm_zone66_gus_isinstance_c03
			return Certainty::DefinitelyNo;
		}
	}

	// Read the song data
	stream::pos patternStart =
		9                      // sizeof fixed-length part of header
		+ 1 * orderCount  // one byte for each pattern in the sequence
		+ 4 * patternCount      // one uint32le for each pattern's offset
		+ 11 * numDigInst      // PCM instruments
	;

	for (int i = 0; i < patternCount; i++) {
		uint32_t patternOffset;
		content >> u32le(patternOffset);
		if (patternStart + patternOffset >= fileSize) {
			// Pattern data offset is past EOF
			// TESTED BY: mus_cdfm_zone66_gus_isinstance_c04
			return Certainty::DefinitelyNo;
		}
	}

	// TESTED BY: mus_cdfm_zone66_gus_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_CDFM_GUS::read(stream::input& content,
	SuppData& suppData) const
{
	auto music = std::make_unique<Music>();
	music->patches = std::make_shared<PatchBank>();

	// All CDFM files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 4;
	music->ticksPerTrack = 64;

	for (unsigned int c = 0; c < CDFM_CHANNEL_COUNT; c++) {
		TrackInfo t;
		t.channelType = TrackInfo::ChannelType::PCM;
		t.channelIndex = c;
		music->trackInfo.push_back(t);
	}

	content.seekg(0, stream::start);

	uint8_t unknown, orderCount, patternCount, numDigInst;
	uint8_t loopDest;
	uint32_t sampleOffset;
	content
		>> u32le(sampleOffset)
		>> u8(unknown)
		>> u8(orderCount)
		>> u8(patternCount)
		>> u8(numDigInst)
		>> u8(loopDest)
	;
	music->loopDest = loopDest;
	music->initialTempo.module(6, CDFM_TEMPO);

	for (unsigned int i = 0; i < orderCount; i++) {
		uint8_t order;
		content >> u8(order);
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
		content >> u32le(ptrPattern);
		ptrPatterns.push_back(ptrPattern);
	}

	// Read instruments
	music->patches->reserve(numDigInst);

	for (int i = 0; i < numDigInst; i++) {
		uint8_t flags;
		auto patch = std::make_shared<PCMPatch>();
		unsigned long lenData;
		content
			>> u8(flags)
			>> u16le(patch->sampleRate)
			>> u32le(patch->loopStart)
			>> u32le(lenData)
		;
		patch->defaultVolume = 255;
		patch->bitDepth = 8;
		patch->numChannels = (flags & 1) ? 2 : 1;
		if (flags & 2) {
			patch->loopEnd = lenData;
		} else {
			patch->loopStart = 0;
			patch->loopEnd = 0;
		}
		patch->data = std::vector<uint8_t>(lenData);
		music->patches->push_back(patch);
	}

	// Read the song data
	stream::pos patternStart =
		9                      // sizeof fixed-length part of header
		+ 1 * orderCount  // one byte for each pattern in the sequence
		+ 4 * patternCount      // one uint32le for each pattern's offset
		+ 11 * numDigInst      // GUS instruments
	;
	assert(content.tellg() == patternStart);

	for (auto& ptrPattern : ptrPatterns) {
		// Jump to the start of the pattern data
		content.seekg(patternStart + ptrPattern, stream::start);
		music->patterns.emplace_back();
		auto& pattern = music->patterns.back();
		for (unsigned int track = 0; track < CDFM_CHANNEL_COUNT; track++) {
			pattern.emplace_back();
		}

		// Process the events
		uint8_t cmd;
		unsigned int lastDelay[CDFM_CHANNEL_COUNT];
		for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) lastDelay[i] = 0;
		bool end = false;
		do {
			content >> u8(cmd);
			uint8_t channel = cmd & 0x0F;
			auto& track = pattern.at(channel);
			switch (cmd & 0xF0) {

				case 0x00: { // command
					switch (cmd) {
						case 0x00: { // delay
							uint8_t data;
							content >> u8(data);
							for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) {
								lastDelay[i] += data;
							}
							break;
						}
						case 0x01: { // speed change
							uint8_t data;
							content >> u8(data);
							auto ev = std::make_shared<TempoEvent>();
							ev->tempo.module(data, CDFM_TEMPO);

							TrackEvent te;
							te.delay = lastDelay[channel];
							lastDelay[channel] = 0;
							te.event = ev;
							track.push_back(te);
							break;
						}
						case 0x02:
							end = true;
							break;
						default:
							std::cerr << "mus-cdfm-zone66: Unknown cmd " << std::hex << (int)cmd
								<< " at offset " << std::dec << content.tellg() - 1 << std::endl;
							break;
					}
					break;
				}

				case 0x40: { // note on
					uint8_t freq, panvol, inst;
					content
						>> u8(freq)
						>> u8(panvol)
						>> u8(inst)
					;
					unsigned int vol = panvol & 0x0F;
					unsigned int pan = panvol >> 4;

					auto ev = std::make_shared<NoteOnEvent>();
					ev->instrument = inst;
					ev->milliHertz = midiToFreq(freq + 25);
					ev->velocity = z66_volume_to_velocity(vol);
					if (ev->velocity < 0) std::cerr << "vol: " << vol << std::endl;
					assert(ev->velocity >= 0);

					TrackEvent te;
					te.delay = lastDelay[channel];
					lastDelay[channel] = 0;
					te.event = ev;

					track.push_back(te);
					break;
				}

				case 0x80: { // Set volume
					uint8_t data;
					content >> u8(data);
					unsigned int vol = data & 0x0F;
					unsigned int pan = data >> 4;

					if (vol == 0) {
						auto ev = std::make_shared<NoteOffEvent>();

						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						te.event = ev;
						track.push_back(te);
					} else {
						auto ev = std::make_shared<EffectEvent>();
						ev->type = EffectEvent::Type::Volume;
						ev->data = z66_volume_to_velocity(vol);

						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						te.event = ev;
						track.push_back(te);
					}
					break;
				}
				default:
					std::cerr << "mus-cdfm-zone66: Unknown event " << std::hex << (int)(cmd & 0xF0)
						<< " at offset " << std::dec << content.tellg() - 1 << std::endl;
					break;
			}
		} while (!end);

		// Write out any trailing delays
		for (unsigned int t = 0; t < CDFM_CHANNEL_COUNT; t++) {
			if (lastDelay[t]) {
				auto ev = std::make_shared<ConfigurationEvent>();
				ev->configType = ConfigurationEvent::Type::EmptyEvent;
				ev->value = 0;

				auto& track = pattern.at(t);
				TrackEvent te;
				te.delay = lastDelay[t];
				te.event = ev;
				track.push_back(te);
			}
		}
	}

	// Load the PCM samples
	content.seekg(sampleOffset, stream::start);
	for (int i = 0; i < numDigInst; i++) {
		auto patch = static_cast<PCMPatch*>(music->patches->at(i).get());
		content.read(patch->data.data(), patch->data.size());

		// Convert 8-bit GUS samples from signed to unsigned
		for (auto& s : patch->data) s += 128;
	}

	return music;
}

void MusicType_CDFM_GUS::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	#warning Zone 66 GUS writing still needs to be implemented
	throw camoto::error("Writing the GUS variant of Zone 66 CDFM files is not "
		"yet implemented.  Please ask for it if you need it.");
}

SuppFilenames MusicType_CDFM_GUS::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_CDFM_GUS::supportedAttributes() const
{
	// No supported metadata
	return {};
}
