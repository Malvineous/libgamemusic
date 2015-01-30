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

#include <boost/pointer_cast.hpp>
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

std::string MusicType_CDFM_GUS::getCode() const
{
	return "cdfm-zone66-gus";
}

std::string MusicType_CDFM_GUS::getFriendlyName() const
{
	return "Renaissance CDFM (GUS)";
}

std::vector<std::string> MusicType_CDFM_GUS::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("z66");
	vcExtensions.push_back("670");
	return vcExtensions;
}

unsigned long MusicType_CDFM_GUS::getCaps() const
{
	return
		InstEmpty
		| InstPCM
		| HasEvents
		| HasPatterns
		| HasLoopDest
	;
}

MusicType::Certainty MusicType_CDFM_GUS::isInstance(stream::input_sptr input) const
{
	stream::len fileSize = input->size();
	input->seekg(0, stream::start);

	uint8_t speed, orderCount, patternCount, numDigInst, loopDest;
	uint32_t sampleOffset;
	input
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
		return MusicType::DefinitelyNo;
	}

	if (loopDest >= orderCount) {
		// Loop target is past end of song
		// TESTED BY: mus_cdfm_zone66_gus_isinstance_c02
		return MusicType::DefinitelyNo;
	}

	uint8_t patternOrder[256];
	input->read(patternOrder, orderCount);
	for (int i = 0; i < orderCount; i++) {
		if (patternOrder[i] >= patternCount) {
			// Sequence specifies invalid pattern
			// TESTED BY: mus_cdfm_zone66_gus_isinstance_c03
			return MusicType::DefinitelyNo;
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
		input >> u32le(patternOffset);
		if (patternStart + patternOffset >= fileSize) {
			// Pattern data offset is past EOF
			// TESTED BY: mus_cdfm_zone66_gus_isinstance_c04
			return MusicType::DefinitelyNo;
		}
	}

	// TESTED BY: mus_cdfm_zone66_gus_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_CDFM_GUS::read(stream::input_sptr input, SuppData& suppData) const
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
		t.channelType = TrackInfo::PCMChannel;
		t.channelIndex = c;
		music->trackInfo.push_back(t);
	}

	input->seekg(0, stream::start);

	uint8_t unknown, orderCount, patternCount, numDigInst;
	uint8_t loopDest;
	uint32_t sampleOffset;
	input
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
	patches->reserve(numDigInst);

	for (int i = 0; i < numDigInst; i++) {
		uint8_t flags;
		PCMPatchPtr patch(new PCMPatch());
		input
			>> u8(flags)
			>> u16le(patch->sampleRate)
			>> u32le(patch->loopStart)
			>> u32le(patch->lenData)
		;
		patch->defaultVolume = 255;
		patch->bitDepth = 8;
		patch->numChannels = (flags & 1) ? 2 : 1;
		if (flags & 2) {
			patch->loopEnd = patch->lenData;
		} else {
			patch->loopStart = 0;
			patch->loopEnd = 0;
		}
		patches->push_back(patch);
	}

	// Read the song data
	stream::pos patternStart =
		9                      // sizeof fixed-length part of header
		+ 1 * orderCount  // one byte for each pattern in the sequence
		+ 4 * patternCount      // one uint32le for each pattern's offset
		+ 11 * numDigInst      // GUS instruments
	;
	assert(input->tellg() == patternStart);

	for (std::vector<uint16_t>::const_iterator
		i = ptrPatterns.begin(); i != ptrPatterns.end(); i++
	) {
		// Jump to the start of the pattern data
		input->seekg(patternStart + *i, stream::start);
		PatternPtr pattern(new Pattern());
		for (unsigned int track = 0; track < CDFM_CHANNEL_COUNT; track++) {
			TrackPtr t(new Track());
			pattern->push_back(t);
		}

		// Process the events
		uint8_t cmd;
		unsigned int lastDelay[CDFM_CHANNEL_COUNT];
		for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) lastDelay[i] = 0;
		bool end = false;
		do {
			input >> u8(cmd);
			uint8_t channel = cmd & 0x0F;
			TrackPtr& track = pattern->at(channel);
			switch (cmd & 0xF0) {

				case 0x00: { // command
					switch (cmd) {
						case 0x00: { // delay
							uint8_t data;
							input >> u8(data);
							for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) {
								lastDelay[i] += data;
							}
							break;
						}
						case 0x01: { // speed change
							uint8_t data;
							input >> u8(data);
							TrackEvent te;
							te.delay = lastDelay[channel];
							lastDelay[channel] = 0;
							TempoEvent *ev = new TempoEvent();
							te.event.reset(ev);
							ev->tempo.module(data, CDFM_TEMPO);
							track->push_back(te);
							break;
						}
						case 0x02:
							end = true;
							break;
						default:
							std::cerr << "mus-cdfm-zone66: Unknown cmd " << std::hex << (int)cmd
								<< " at offset " << std::dec << input->tellg() - 1 << std::endl;
							break;
					}
					break;
				}

				case 0x40: { // note on
					uint8_t freq, panvol, inst;
					input
						>> u8(freq)
						>> u8(panvol)
						>> u8(inst)
					;
					unsigned int vol = panvol & 0x0F;
					unsigned int pan = panvol >> 4;

					TrackEvent te;
					te.delay = lastDelay[channel];
					lastDelay[channel] = 0;
					NoteOnEvent *ev = new NoteOnEvent();
					te.event.reset(ev);

					ev->instrument = inst;
					ev->milliHertz = midiToFreq(freq + 25);
					ev->velocity = z66_volume_to_velocity(vol);
					if (ev->velocity < 0) std::cerr << "vol: " << vol << std::endl;
					assert(ev->velocity >= 0);

					track->push_back(te);
					break;
				}

				case 0x80: { // Set volume
					uint8_t data;
					input >> u8(data);
					unsigned int vol = data & 0x0F;
					unsigned int pan = data >> 4;

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
						ev->data = z66_volume_to_velocity(vol);
						track->push_back(te);
					}
					break;
				}
				default:
					std::cerr << "mus-cdfm-zone66: Unknown event " << std::hex << (int)(cmd & 0xF0)
						<< " at offset " << std::dec << input->tellg() - 1 << std::endl;
					break;
			}
		} while (!end);

		// Write out any trailing delays
		for (unsigned int t = 0; t < CDFM_CHANNEL_COUNT; t++) {
			if (lastDelay[t]) {
				TrackPtr& track = pattern->at(t);
				TrackEvent te;
				te.delay = lastDelay[t];
				ConfigurationEvent *ev = new ConfigurationEvent();
				te.event.reset(ev);
				ev->configType = ConfigurationEvent::EmptyEvent;
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
		uint8_t *data = new uint8_t[patch->lenData];
		patch->data.reset(data);
		input->read(patch->data.get(), patch->lenData);

		// Convert 8-bit GUS samples from signed to unsigned
		for (unsigned long i = 0; i < patch->lenData; i++) {
			*data++ += 128;
		}
	}

	return music;
}

void MusicType_CDFM_GUS::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	assert(false);
	return;
}

SuppFilenames MusicType_CDFM_GUS::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_CDFM_GUS::getMetadataList() const
{
	// No supported metadata
	return Metadata::MetadataTypes();
}
