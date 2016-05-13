/**
 * @file  mus-cdfm-zone66.cpp
 * @brief Support for Renaissance's CDFM format used in Zone 66.
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

class EventConverter_CDFM: virtual public EventHandler
{
	public:
		/// Prepare to convert events into CDFM data sent to a stream.
		/**
		 * @param content
		 *   Output stream the CDFM data will be written to.
		 *
		 * @param music
		 *   Song parameters (patches, initial tempo, etc.)  Events are not read
		 *   from here, the EventHandler is used for that.
		 */
		EventConverter_CDFM(stream::output& content, const Music& music);

		/// Destructor.
		virtual ~EventConverter_CDFM();

		// EventHandler overrides
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const GotoEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

		std::vector<unsigned int> instMapPCM, instMapOPL;
		std::vector<stream::pos> offPattern; ///< Offset of each pattern

	protected:
		stream::output& content;  ///< Where to write data
		const Music& music;       ///< Song being written
		double usPerTick;         ///< Last tempo value
		unsigned int curRow;      ///< Current row in pattern (0-63 inclusive)

		/// Write out the current delay
		void writeDelay(unsigned long delay);

		/// Work out which channel the event should be played on
		int getCDFMChannel(unsigned int trackIndex);
};


std::string MusicType_CDFM::code() const
{
	return "cdfm-zone66";
}

std::string MusicType_CDFM::friendlyName() const
{
	return "Renaissance CDFM";
}

std::vector<std::string> MusicType_CDFM::fileExtensions() const
{
	return {
		"z66",
		"670",
	};
}

MusicType::Caps MusicType_CDFM::caps() const
{
	return
		Caps::InstOPL
		| Caps::InstPCM
		| Caps::HasEvents
		| Caps::HasPatterns
		| Caps::HasLoopDest
		| Caps::HardwareOPL2
	;
}

MusicType::Certainty MusicType_CDFM::isInstance(stream::input& content) const
{
	stream::len fileSize = content.size();

	// Too short: header truncated
	// TESTED BY: mus_cdfm_zone66_isinstance_c05
	if (fileSize < 1*6+4) return Certainty::DefinitelyNo;

	content.seekg(0, stream::start);

	uint8_t speed, orderCount, patternCount, numDigInst, numOPLInst, loopDest;
	uint32_t sampleOffset;
	content
		>> u8(speed)
		>> u8(orderCount)
		>> u8(patternCount)
		>> u8(numDigInst)
		>> u8(numOPLInst)
		>> u8(loopDest)
		>> u32le(sampleOffset)
	;
	if (numDigInst && (sampleOffset >= fileSize)) {
		// Sample data past EOF
		// TESTED BY: mus_cdfm_zone66_isinstance_c01
		return Certainty::DefinitelyNo;
	}

	if (loopDest >= orderCount) {
		// Loop target is past end of song
		// TESTED BY: mus_cdfm_zone66_isinstance_c02
		return Certainty::DefinitelyNo;
	}

	// Too short: order list truncated
	// TESTED BY: mus_cdfm_zone66_isinstance_c06
	if (fileSize < 1u*6+4+orderCount) return Certainty::DefinitelyNo;

	uint8_t patternOrder[256];
	content.read(patternOrder, orderCount);
	for (int i = 0; i < orderCount; i++) {
		if (patternOrder[i] >= patternCount) {
			// Sequence specifies invalid pattern
			// TESTED BY: mus_cdfm_zone66_isinstance_c03
			return Certainty::DefinitelyNo;
		}
	}

	// Read the song data
	stream::pos patternStart =
		10                  // sizeof fixed-length part of header
		+ 1 * orderCount    // one byte for each pattern in the sequence
		+ 4 * patternCount  // one uint32le for each pattern's offset
		+ 16 * numDigInst   // PCM instruments
		+ 11 * numOPLInst   // OPL instruments
	;

	// Too short, pattern-offset-list truncated
	// TESTED BY: mus_cdfm_zone66_isinstance_c07
	if (fileSize < 1u*6+4+orderCount + 4*patternCount) {
		return Certainty::DefinitelyNo;
	}

	for (int i = 0; i < patternCount; i++) {
		uint32_t patternOffset;
		content >> u32le(patternOffset);
		if (patternStart + patternOffset >= fileSize) {
			// Pattern data offset is past EOF
			// TESTED BY: mus_cdfm_zone66_isinstance_c04
			return Certainty::DefinitelyNo;
		}
	}

	// TESTED BY: mus_cdfm_zone66_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_CDFM::read(stream::input& content,
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
		if (c < 4) {
			t.channelType = TrackInfo::ChannelType::PCM;
			t.channelIndex = c;
		} else if (c < 13) {
			t.channelType = TrackInfo::ChannelType::OPL;
			t.channelIndex = c - 4;
		} else {
			t.channelType = TrackInfo::ChannelType::Unused;
			t.channelIndex = c - 13;
		}
		music->trackInfo.push_back(t);
	}

	content.seekg(0, stream::start);

	uint8_t speed, orderCount, patternCount, numDigInst, numOPLInst, loopDest;
	uint32_t sampleOffset;
	content
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
		content >> u8(order);
		if (order < 0xFE) {
			music->patternOrder.push_back(order);
		} else {
			/// @todo: See if this is part of the CDFM spec, like it is for S3M
			std::cout << "CDFM: Got pattern >= 254, handle this!\n";
		}
	}

	std::vector<uint32_t> ptrPatterns;
	ptrPatterns.reserve(patternCount);
	for (unsigned int i = 0; i < patternCount; i++) {
		uint32_t ptrPattern;
		content >> u32le(ptrPattern);
		ptrPatterns.push_back(ptrPattern);
	}

	// Read instruments
	music->patches->reserve(numDigInst + numOPLInst);

	for (int i = 0; i < numDigInst; i++) {
		auto patch = std::make_shared<PCMPatch>();
		content.seekg(4, stream::cur); // skip address_ptr field
		unsigned long lenData;
		content
			>> u32le(lenData)
			>> u32le(patch->loopStart)
			>> u32le(patch->loopEnd)
		;
		patch->defaultVolume = 255;
		patch->sampleRate = 8287;
		patch->bitDepth = 8;
		patch->numChannels = 1;
		if (patch->loopEnd == 0x00FFFFFF) patch->loopEnd = 0; // no loop
		if (patch->loopStart >= lenData) patch->loopStart = 0;
		if (patch->loopEnd > lenData) patch->loopEnd = lenData;
		patch->data = std::vector<uint8_t>(lenData);
		music->patches->push_back(patch);
	}

	uint8_t inst[11];
	for (int i = 0; i < numOPLInst; i++) {
		auto patch = std::make_shared<OPLPatch>();
		patch->defaultVolume = 255;
		content.read(inst, 11);

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
		patch->rhythm      = OPLPatch::Rhythm::Melodic;

		music->patches->push_back(patch);
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
	assert(content.tellg() == patternStart);

	unsigned int patternIndex = 0;
	const unsigned int& firstOrder = music->patternOrder[0];
	for (auto& i : ptrPatterns) {
		// Jump to the start of the pattern data
		content.seekg(patternStart + i, stream::start);
		music->patterns.emplace_back();
		auto& pattern = music->patterns.back();
		for (unsigned int track = 0; track < CDFM_CHANNEL_COUNT; track++) {
			pattern.emplace_back();
			auto& t = pattern.back();
			if (
				firstPattern
				&& (patternIndex == firstOrder)
				&& (track == 4)
			) { // first OPL track in pattern being played first in order list
				// Set standard settings
				// OPL3 is off.  We don't add an EnableOPL3 event with the value set
				// to zero as that event requires an OPL3 to be present.
				{
					auto ev = std::make_shared<ConfigurationEvent>();
					ev->configType = ConfigurationEvent::Type::EnableDeepTremolo;
					ev->value = 0;

					TrackEvent te;
					te.delay = 0;
					te.event = ev;
					t.push_back(te);
				}
				{
					auto ev = std::make_shared<ConfigurationEvent>();
					ev->configType = ConfigurationEvent::Type::EnableDeepVibrato;
					ev->value = 0;

					TrackEvent te;
					te.delay = 0;
					te.event = ev;
					t.push_back(te);
				}
				{
					auto ev = std::make_shared<ConfigurationEvent>();
					ev->configType = ConfigurationEvent::Type::EnableWaveSel;
					ev->value = 1;

					TrackEvent te;
					te.delay = 0;
					te.event = ev;
					t.push_back(te);
				}

				firstPattern = false;
			}
		}

		// Process the events
		uint8_t cmd;
		unsigned int lastDelay[CDFM_CHANNEL_COUNT];
		for (unsigned int i = 0; i < CDFM_CHANNEL_COUNT; i++) lastDelay[i] = 0;
		do {
			content >> u8(cmd);
			uint8_t channel = cmd & 0x0F;
			if (channel >= CDFM_CHANNEL_COUNT) {
				throw stream::error(createString("CDFM: Channel " << (int)channel
					<< " out of range, max valid channel is " << CDFM_CHANNEL_COUNT - 1));
			}
			auto& track = pattern.at(channel);
			switch (cmd & 0xF0) {
				case 0x00: { // note on
					uint8_t freq, inst_vel;
					content >> u8(freq) >> u8(inst_vel);
					freq %= 128; // notes larger than this wrap

					auto ev = std::make_shared<NoteOnEvent>();

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

					TrackEvent te;
					te.delay = lastDelay[channel];
					lastDelay[channel] = 0;
					te.event = ev;
					track.push_back(te);
					break;
				}
				case 0x20: { // set volume
					uint8_t vol;
					content >> u8(vol);
					vol &= 0x0F;

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
						ev->data = (vol << 4) | vol; // 0..15 -> 0..255

						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						te.event = ev;
						track.push_back(te);
					}
					break;
				}
				case 0x40: { // delay
					uint8_t data;
					content >> u8(data);
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
						<< " at offset " << content.tellg() - 1 << std::endl;
					break;
			}
		} while (cmd != 0x60);

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
		patternIndex++;
	}

	// Load the PCM samples
	content.seekg(sampleOffset, stream::start);
	for (int i = 0; i < numDigInst; i++) {
		auto patch = static_cast<PCMPatch*>(music->patches->at(i).get());
		content.read(patch->data.data(), patch->data.size());
	}

	return music;
}

void MusicType_CDFM::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	unsigned long speed = music.initialTempo.module_speed();
	if (speed > 255) {
		throw format_limitation(createString("Tempo is too fast for CDFM file!  "
			"Calculated value is " << speed << " but max permitted value is 255."));
	}

	EventConverter_CDFM conv(content, music);

	unsigned int numDigInst = 0, numOPLInst = 0;
	for (auto& i : *music.patches) {
		auto pcmPatch = dynamic_cast<const PCMPatch *>(i.get());
		if (pcmPatch) {
			if (pcmPatch->bitDepth != 8) {
				throw format_limitation("CDFM files can only store 8-bit samples.");
			}
			if (pcmPatch->numChannels != 1) {
				throw format_limitation("CDFM files can only store mono samples.");
			}
			conv.instMapPCM.push_back(numDigInst);
			conv.instMapOPL.push_back(-1);
			numDigInst++;
		} else if (dynamic_cast<const OPLPatch *>(i.get())) {
			conv.instMapPCM.push_back(-1);
			conv.instMapOPL.push_back(numOPLInst);
			numOPLInst++;
		} else {
			conv.instMapPCM.push_back(-1);
			conv.instMapOPL.push_back(-1);
		}
	}
	if (numDigInst > 255) {
		throw format_limitation(createString(numDigInst << " PCM instruments is "
			"larger than the maximum of 255 possible in a CDFM file."));
	}
	if (numOPLInst > 255) {
		throw format_limitation(createString(numOPLInst << " OPL instruments is "
			"larger than the maximum of 255 possible in a CDFM file."));
	}
	uint8_t loopDest = (music.loopDest < 0) ? 0 : music.loopDest;
	content
		<< u8(speed)
		<< u8(music.patternOrder.size())
		<< u8(music.patterns.size())
		<< u8(numDigInst)
		<< u8(numOPLInst)
		<< u8(loopDest)
		<< u32le(0xFFFFFFFF) // placeholder for sample offset
	;
	for (auto& i : music.patternOrder) {
		content << u8(i);
	}

	// Write placeholders for the offset of each pattern's data
	stream::pos offPatternOffsets = content.tellp();
	content << nullPadded("", music.patterns.size() * 4);

	// Write the PCM instrument parameters
	for (auto& i : *music.patches) {
		auto pcmPatch = dynamic_cast<const PCMPatch *>(i.get());
		if (!pcmPatch) continue;

		uint32_t loopEnd = pcmPatch->loopEnd;
		if (pcmPatch->loopEnd == 0) loopEnd = 0x00FFFFFF;

		content
			<< u32le(0)
			<< u32le(pcmPatch->data.size())
			<< u32le(pcmPatch->loopStart)
			<< u32le(loopEnd)
		;
	}

	// Write the OPL instrument parameters
	std::vector<stream::pos> sampleOffsets;
	for (auto& i : *music.patches) {
		auto oplPatch = dynamic_cast<const OPLPatch *>(i.get());
		if (!oplPatch) continue;

		uint8_t inst[11];
		inst[0] = (oplPatch->feedback << 1) | (oplPatch->connection ? 1 : 0);
		const OPLOperator *o = &oplPatch->m;
		for (int op = 0; op < 2; op++) {
			inst[op * 5 + 1] =
				(o->enableTremolo ? 0x80 : 0) |
				(o->enableVibrato ? 0x40 : 0) |
				(o->enableSustain ? 0x20 : 0) |
				(o->enableKSR ? 0x10 : 0) |
				o->freqMult
			;
			inst[op * 5 + 2] = (o->scaleLevel << 6) | o->outputLevel;
			inst[op * 5 + 3] = (o->attackRate << 4) | o->decayRate;
			inst[op * 5 + 4] = (o->sustainRate << 4) | o->releaseRate;
			inst[op * 5 + 5] = o->waveSelect;
			o = &oplPatch->c;
		}
		content.write(inst, 11);
	}

	// Write the pattern data
	stream::pos offPatternStart = content.tellp();
	conv.handleAllEvents(EventHandler::Pattern_Row_Track, music);

	// Write the PCM sample data
	stream::pos offSample = content.tellp();
	for (auto& i : *music.patches) {
		auto pcmPatch = dynamic_cast<const PCMPatch *>(i.get());
		if (!pcmPatch) continue;
		content.write(pcmPatch->data.data(), pcmPatch->data.size());
	}

	// Go back and write in all the offsets
	stream::pos offEnd = content.tellp();
	content.seekp(6, stream::start);
	content << u32le(offSample);
	// We leave the first pattern offset as zero as this is what it will
	// always be
	content.seekp(offPatternOffsets + 4, stream::start);
	// We don't need the offset of the end of the pattern data
	conv.offPattern.pop_back();
	for (std::vector<stream::pos>::const_iterator
		i = conv.offPattern.begin(); i != conv.offPattern.end(); i++
	) {
		content << u32le(*i - offPatternStart);
	}
	content.seekp(offEnd, stream::start);
	content.truncate_here();
	return;
}

SuppFilenames MusicType_CDFM::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_CDFM::supportedAttributes() const
{
	// No supported metadata
	return {};
}


EventConverter_CDFM::EventConverter_CDFM(stream::output& content,
	const Music& music)
	:	content(content),
		music(music),
		curRow(0)
{
}

EventConverter_CDFM::~EventConverter_CDFM()
{
}

void EventConverter_CDFM::endOfTrack(unsigned long delay)
{
	return;
}

void EventConverter_CDFM::endOfPattern(unsigned long delay)
{
	this->writeDelay(delay + 64 - (this->curRow + delay));
	this->content << u8(0x60);
	this->offPattern.push_back(this->content.tellp());
	this->curRow = 0;
	return;
}

bool EventConverter_CDFM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->writeDelay(delay);
	std::cerr << "CDFM: Does not support tempo changes\n";
	return true;
}

bool EventConverter_CDFM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	this->writeDelay(delay);
	int channel = this->getCDFMChannel(trackIndex);
	if (channel < 0) return true;

	unsigned int midiNote = round(freqToMIDI(ev->milliHertz));

	uint8_t note = midiNote % 12;
	uint8_t oct = midiNote / 12;

	int inst;
	if (channel > 3) {
		inst = this->instMapOPL[ev->instrument];
		if (oct < 1) {
			std::cerr << "CDFM: Dropping OPL note in octave < 1\n";
			return true;
		}
		oct -= 1;
	} else {
		inst = this->instMapPCM[ev->instrument];
		if (oct < 3) {
			std::cerr << "CDFM: Dropping PCM note in octave < 3\n";
			return true;
		}
		oct -= 3;
	}
	if (inst < 0) return true; // non-PCM/OPL instrument

	assert(ev->velocity < 256);
	unsigned int velocity;
	if (ev->velocity < 0) {
		// Default velocity from instrument
		velocity = this->music.patches->at(ev->instrument)->defaultVolume;
	} else {
		velocity = ev->velocity;
	}
	this->content
		<< u8(channel)
		<< u8((oct << 4) | note)
		<< u8((inst << 4) | (velocity >> 4))
	;
	return true;
}

bool EventConverter_CDFM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->writeDelay(delay);
	int channel = this->getCDFMChannel(trackIndex);
	if (channel < 0) return true;

	// Fake a note-off by setting the volume to zero
	this->content
		<< u8(0x20 | channel)
		<< u8(0)
	;
	return true;
}

bool EventConverter_CDFM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	this->writeDelay(delay);
	int channel = this->getCDFMChannel(trackIndex);
	if (channel < 0) return true;

	switch (ev->type) {
		case EffectEvent::Type::PitchbendNote:
			std::cerr << "CDFM: Pitch bends not supported, ignoring event\n";
			break;
		case EffectEvent::Type::Volume:
			this->content
				<< u8(0x20 | channel)
				<< u8(ev->data >> 4) // 0..255 -> 0..15
			;
			break;
	}
	return true;
}

bool EventConverter_CDFM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const GotoEvent *ev)
{
	this->writeDelay(delay);
	// Not supported by this format?
	return true;
}

bool EventConverter_CDFM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->configType) {
		case ConfigurationEvent::Type::EmptyEvent:
			break;
		case ConfigurationEvent::Type::EnableOPL3:
			if (ev->value != 0) std::cerr << "CDFM: OPL3 cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableDeepTremolo:
			if (ev->value != 0) std::cerr << "CDFM: Deep tremolo cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableDeepVibrato:
			if (ev->value != 0) std::cerr << "CDFM: Deep vibrato cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableRhythm:
			if (ev->value != 0) std::cerr << "CDFM: Rhythm mode cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableWaveSel:
			if (ev->value != 1) std::cerr << "CDFM: Wave selection registers cannot be disabled, ignoring event.\n";
			break;
	}
	return true;
}

void EventConverter_CDFM::writeDelay(unsigned long delay)
{
	if (this->curRow + delay > 64) {
		throw format_limitation(createString("CDFM: Tried to write pattern with more "
			"than 64 rows (next row is " << this->curRow + delay << ")."));
	}

	this->curRow += delay;
	while (delay) {
		uint8_t d = (delay > 255) ? 255 : delay;
		this->content
			<< u8(0x40)
			<< u8(d)
		;
		delay -= d;
	}
	return;
}

int EventConverter_CDFM::getCDFMChannel(unsigned int trackIndex)
{
	const TrackInfo& ti = this->music.trackInfo[trackIndex];
	if (ti.channelType == TrackInfo::ChannelType::PCM) {
		if (ti.channelIndex > 3) {
			throw format_limitation("CDFM files only support four PCM channels.");
		}
		return ti.channelIndex;
	} else if (ti.channelType == TrackInfo::ChannelType::OPL) {
		if (ti.channelIndex > 8) {
			throw format_limitation("CDFM files only support nine OPL channels.");
		}
		return 4 + ti.channelIndex;
	}
	return -1;
}
