/**
 * @file  mus-mus-dmx.cpp
 * @brief Support for id Software MIDI (.mus) format.
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
#include <camoto/util.hpp> // make_unique
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include "track-split.hpp"
#include "mus-mus-dmx.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Total channels (even if they're not all used)
#define MUS_CHANNEL_COUNT 16

/// Maximum number of valid MUS instruments (actually 0..127, 135..181)
#define MUS_MAX_INST 256

/// Class taking MIDI events and producing MUS data.
class MUSEncoder: virtual private MIDIEventCallback
{
	public:
		/// Set encoding parameters.
		/**
		 * @param content
		 *   Data stream to write the MIDI data to.
		 *
		 * @param music
		 *   The instance to convert to MIDI data.
		 *
		 * @param musClock
		 *   MUS clock speed in Hertz (e.g. 140).
		 */
		MUSEncoder(stream::output& content, const Music& music,
			unsigned int musClock);

		/// Destructor.
		~MUSEncoder();

		/// Process the events, and write out data in the target format.
		/**
		 * This function will data to the content stream, until all the events in
		 * the song have been written out.
		 *
		 * @throw stream:error
		 *   If the content data could not be written for some reason.
		 *
		 * @throw format_limitation
		 *   If the song could not be converted to MIDI for some reason (e.g. it has
		 *   sampled instruments.)
		 */
		void encode();

		// MIDIEventCallback
		virtual void midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity);
		virtual void midiNoteOn(uint32_t delay, uint8_t channel, uint8_t note,
			uint8_t velocity);
		virtual void midiPatchChange(uint32_t delay, uint8_t channel,
			uint8_t instrument);
		virtual void midiController(uint32_t delay, uint8_t channel,
			uint8_t controller, uint8_t value);
		virtual void midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend);
		virtual void midiSetTempo(uint32_t delay, const Tempo& tempo);
		virtual void endOfTrack();
		virtual void endOfPattern();
		virtual void endOfSong(uint32_t delay);

	protected:
		stream::output& content;        ///< Target stream for SMF MIDI data
		const Music& music;             ///< Song to convert
		unsigned int musClock;          ///< MUS clock speed in Hertz
		unsigned int lastVelocity;      ///< Last velocity value set
		unsigned long usPerTick;        ///< Current tempo
		std::vector<uint8_t> nextEvent; ///< Next event to write

		/// Write delay bytes and the previous event.
		void writeDelay(unsigned int delay);
};


MusicType_MUS_Raptor::MusicType_MUS_Raptor()
{
	this->tempo = 70;
}

std::string MusicType_MUS_Raptor::code() const
{
	return "mus-dmx-raptor";
}

std::string MusicType_MUS_Raptor::friendlyName() const
{
	return "DMX audio library MIDI File (Raptor tempo)";
}


MusicType_MUS::MusicType_MUS()
	:	tempo(140)
{
}

std::string MusicType_MUS::code() const
{
	return "mus-dmx";
}

std::string MusicType_MUS::friendlyName() const
{
	return "DMX audio library MIDI File (normal tempo)";
}

std::vector<std::string> MusicType_MUS::fileExtensions() const
{
	return {
		"mus",
	};
}

MusicType::Caps MusicType_MUS::caps() const
{
	return
		Caps::InstMIDI
		| Caps::HasEvents
	;
}

MusicType::Certainty MusicType_MUS::isInstance(stream::input& content) const
{
	// Too short
	// TESTED BY: mus_mus_dmx_isinstance_c01
	if (content.size() < 4 + 2 * 6) return Certainty::DefinitelyNo;

	content.seekg(0, stream::start);
	uint8_t sig[4];
	try {
		content.read(sig, 4);
	} catch (...) {
		return Certainty::DefinitelyNo;
	}
	if (
		(sig[0] != 'M') ||
		(sig[1] != 'U') ||
		(sig[2] != 'S') ||
		(sig[3] != 0x1A)
	) {
		// TESTED BY: mus_mus_dmx_isinstance_c02
		return Certainty::DefinitelyNo;
	}

	// TESTED BY: mus_mus_dmx_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_MUS::read(stream::input& content,
	SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	content.seekg(0, stream::start);

	auto music = std::make_unique<Music>();
	music->patches = std::make_unique<PatchBank>();
	music->initialTempo.hertz(this->tempo);
	music->initialTempo.ticksPerQuarterNote(64); // some random value in the ballpark

	unsigned long lastDelay[MUS_CHANNEL_COUNT];
	uint8_t musActivePatch[MUS_CHANNEL_COUNT]; ///< Which instruments are in use on which channel
	uint8_t volMap[MUS_CHANNEL_COUNT]; ///< Volume set on each channel
	int instMap[MUS_MAX_INST]; ///< MIDI patch to libgamemusic instrument map

	memset(musActivePatch, 0xFF, sizeof(musActivePatch));
	memset(volMap, 0, sizeof(volMap));
	memset(instMap, 0xFF, sizeof(instMap));

	music->patterns.emplace_back();
	auto& pattern = music->patterns.back();

	for (unsigned int c = 0; c < MUS_CHANNEL_COUNT; c++) {
		music->trackInfo.emplace_back();
		auto& t = music->trackInfo.back();

		t.channelType = TrackInfo::ChannelType::MIDI;
		t.channelIndex = c;

		pattern.emplace_back();
		lastDelay[c] = 0;
	}
	music->patternOrder.push_back(0);

	content.seekg(4, stream::start);

	uint16_t lenSong, offSong, priChan, secChan, numInst, reserved;
	content
		>> u16le(lenSong)
		>> u16le(offSong)
		>> u16le(priChan)
		>> u16le(secChan)
		>> u16le(numInst)
		>> u16le(reserved)
	;

	// Read the instruments
	music->patches->reserve(numInst);
	for (int i = 0; i < numInst; i++) {
		auto patch = std::make_shared<MIDIPatch>();
		uint16_t pnum;
		content >> u16le(pnum);
		if (pnum >= MUS_MAX_INST) {
			std::cout << "mus-dmx: MIDI patch " << pnum
				<< " is out of range, using 0.\n";
			pnum = 0;
		}
		patch->midiPatch = pnum;
		patch->percussion = false;
		music->patches->push_back(patch);
		instMap[patch->midiPatch] = i;
	}

	content.seekg(offSong, stream::start);

	unsigned long totalDelay = 0; // full delay for entire song
	bool eof = false;
	while (!eof) {
		uint8_t code;
		try {
			content >> u8(code);
		} catch (const stream::incomplete_read&) {
			// no point setting eof=true here
			break;
		}

		unsigned int channel = code & 0x0F;
		unsigned int event = (code >> 4) & 0x07;
		bool last = code & 0x80;
		switch (event) {
			case 0x0: { // note off
				unsigned int& track = channel;
				uint8_t n;
				content >> u8(n);

				auto ev = std::make_shared<SpecificNoteOffEvent>();
				ev->milliHertz = midiToFreq(n);

				TrackEvent te;
				te.delay = lastDelay[track];
				lastDelay[track] = 0;
				te.event = ev;
				pattern.at(track).push_back(te);
				break;
			}
			case 0x1: { // note on
				uint8_t n;
				content >> u8(n);
				if (n & 0x80) {
					content >> u8(volMap[channel]);
				} // else reuse last value
				n &= 0x7F;
				unsigned int libPatch;
				if (channel == 15) {
					// percussion
					if (instMap[128 + n] < 0) {
						auto patch = std::make_shared<MIDIPatch>();
						patch->midiPatch = n;
						patch->percussion = true;
						instMap[128 + n] = music->patches->size();
						music->patches->push_back(patch);
					}
					libPatch = instMap[128 + n];
				} else {
					if (instMap[musActivePatch[channel]] < 0) {
						std::cout << "mus-dmx: Instrument " << (int)musActivePatch[channel]
							<< " was selected but not listed in the file's instrument list!"
							<< std::endl;
						auto patch = std::make_shared<MIDIPatch>();
						patch->midiPatch = musActivePatch[channel];
						patch->percussion = false;
						instMap[patch->midiPatch] = music->patches->size();
						music->patches->push_back(patch);
					}
					libPatch = instMap[musActivePatch[channel]];
				}
				unsigned int& track = channel;

				auto ev = std::make_shared<NoteOnEvent>();
				ev->instrument = libPatch;
				ev->milliHertz = midiToFreq(n);
				ev->velocity = (volMap[channel] << 1) | (volMap[channel] >> 6);

				TrackEvent te;
				te.delay = lastDelay[track];
				lastDelay[track] = 0;
				te.event = ev;
				pattern.at(track).push_back(te);
				break;
			}
			case 0x2: { // pitchbend
				uint8_t bend;
				content >> u8(bend);

				double bendSemitones = (bend - 128.0) / 128.0;

				auto ev = std::make_shared<PolyphonicEffectEvent>();
				ev->type = (EffectEvent::Type)PolyphonicEffectEvent::Type::PitchbendChannel;
				ev->data = midiSemitonesToPitchbend(bendSemitones);

				const unsigned int& track = channel;
				TrackEvent te;
				te.delay = lastDelay[track];
				lastDelay[track] = 0;
				te.event = ev;
				pattern.at(track).push_back(te);
				break;
			}
			case 0x3: { // system event
				uint8_t controller;
				content >> u8(controller);
				switch (controller) {
					case 0x0A:
					case 0x0B:
					case 0x0C:
					case 0x0D:
					case 0x0E:
						std::cerr << "mus-dmx: TODO - convert this controller" << std::endl;
						break;
					default:
						std::cerr << "mus-dmx: unknown system event 0x"
							<< std::hex << (int)controller << std::dec << std::endl;
						break;
				}
				break;
			}
			case 0x4: { // controller
				uint8_t controller, value;
				content >> u8(controller) >> u8(value);
				switch (controller) {
					case 0: // patch change
						musActivePatch[channel] = value;
						break;
					case 3: // volume change
						volMap[channel] = ((value & 0xFF) << 1) || (value & 1);
						break;
					default:
						std::cerr << "mus-dmx: unknown controller type 0x"
							<< std::hex << (int)controller << std::dec << std::endl;
						break;
				}
				break;
			}
			case 0x6: // end of song
				eof = true;
				break;
			case 0x7: // unassigned
				uint8_t unused;
				content >> u8(unused);
				break;
			default:
				std::cerr << "mus-dmx: unknown event type 0x" << std::hex << (int)event
					<< std::dec << std::endl;
				throw stream::error("Unknown mus-dmx event type");
		}

		// Process any delay
		if (last) {
			unsigned long finalDelay = 0;
			uint8_t delayVal;
			do {
				content >> u8(delayVal);
				finalDelay <<= 7;
				finalDelay |= delayVal & 0x7F;
			} while (delayVal & 0x80);
			for (unsigned int c = 0; c < MUS_CHANNEL_COUNT; c++) {
				lastDelay[c] += finalDelay;
			}
			totalDelay += finalDelay;
		}
	}

	// Add dummy events for any final delays
	unsigned int trackCount = MUS_CHANNEL_COUNT;
	for (int track = trackCount - 1; track >= 0; track--) {
		if (lastDelay[track] == totalDelay) {
			// This track is unused
			music->trackInfo.erase(music->trackInfo.begin() + track);
			auto& p = music->patterns.at(0);
			p.erase(p.begin() + track);
		} else if (lastDelay[track]) {
			// This track has a trailing delay
			auto ev = std::make_shared<ConfigurationEvent>();
			ev->configType = ConfigurationEvent::Type::EmptyEvent;
			ev->value = 0;

			TrackEvent te;
			te.delay = lastDelay[track];
			lastDelay[track] = 0;
			te.event = ev;
			pattern.at(track).push_back(te);
		}
	}

	music->ticksPerTrack = totalDelay;

	splitPolyphonicTracks(*music);
	return music;
}

void MusicType_MUS::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	requirePatches<MIDIPatch>(*music.patches);

	// Count the number of unique MIDI channels in use
	std::map<unsigned int, bool> activeChannels;
	unsigned int primaryChannels = 0;
	unsigned int secondaryChannels = 0;
	for (auto& i : music.trackInfo) {
		if (i.channelType != TrackInfo::ChannelType::MIDI) continue;
		activeChannels[i.channelIndex] = true;
		if (i.channelIndex < 9) {
			// Primary channel
			primaryChannels = std::max(primaryChannels, i.channelIndex + 1);
		} else if ((i.channelIndex > 9) && (i.channelIndex < 15)) {
			// Secondary channel
			secondaryChannels = std::max(secondaryChannels, (i.channelIndex - 10) + 1);
		} // else channels 9 or 15, percussion, not counted
	}

	stream::pos offSong = 4 + 2*6 + 2*music.patches->size();
	content
		<< nullPadded("MUS\x1A", 4)
		<< u16le(0xFFFF) // song length placeholder
		<< u16le(offSong)
		<< u16le(primaryChannels) // primary channel count
		<< u16le(secondaryChannels) // secondary channel count
		<< u16le(music.patches->size())
		<< u16le(0) // reserved
	;

	for (auto& i : *music.patches) {
		auto midiPatch = dynamic_cast<const MIDIPatch*>(i.get());
		assert(midiPatch);
		unsigned int patch = midiPatch->midiPatch;
		if (midiPatch->percussion) patch += 128;
		content << u16le(patch);
	}

	MUSEncoder conv(content, music, this->tempo);
	conv.encode();

	// Go back and write the song length
	stream::pos posEnd = content.tellp();
	content.seekp(4, stream::start);
	content << u16le(posEnd - offSong);
	content.seekp(posEnd, stream::start);

	// Set final filesize to this
	content.truncate_here();

	return;
}

SuppFilenames MusicType_MUS::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_MUS::supportedAttributes() const
{
	std::vector<Attribute> types;
	return types;
}


MUSEncoder::MUSEncoder(stream::output& content, const Music& music,
	unsigned int musClock)
	:	content(content),
		music(music),
		musClock(HERTZ_TO_uS(musClock)),
		lastVelocity(-1)
{
	this->usPerTick = this->music.initialTempo.usPerTick;
}

MUSEncoder::~MUSEncoder()
{
}

void MUSEncoder::encode()
{
	// Create a dummy shared_ptr to pass to EventConverter_MIDI, which takes the
	// reference to the Music instance and converts it into a fake smart_ptr
	// (which will never try to delete the reference.)
	// This is safe because the EventConverter_MIDI instance holding this fake
	// smart_ptr does not outlive the current function.
	std::shared_ptr<const Music> music_ptr(&this->music, [](const Music *p){ return; });

	EventConverter_MIDI conv(this, music_ptr, MIDIFlags::Default);
	conv.handleAllEvents(EventHandler::Order_Row_Track);
	return;
}

void MUSEncoder::writeDelay(unsigned int delay)
{
	if (delay != 0) {
		this->nextEvent[0] |= 0x80;
	}

	for (auto& i : this->nextEvent) {
		this->content << u8(i);
	}
	this->nextEvent.clear();

	delay = delay * this->usPerTick / this->musClock;

	// Write the three most-significant bytes as 7-bit bytes, with the most
	// significant bit set.
	uint8_t next;
	if (delay >= (1 << 28)) { next = 0x80 | ((delay >> 28) & 0x7F); this->content << u8(next); }
	if (delay >= (1 << 21)) { next = 0x80 | ((delay >> 21) & 0x7F); this->content << u8(next); }
	if (delay >= (1 << 14)) { next = 0x80 | ((delay >> 14) & 0x7F); this->content << u8(next); }
	if (delay >= (1 <<  7)) { next = 0x80 | ((delay >>  7) & 0x7F); this->content << u8(next); }
	if (delay >= (1 <<  0)) { next =        ((delay >>  0) & 0x7F); this->content << u8(next); }

	return;
}

void MUSEncoder::midiNoteOff(uint32_t delay, uint8_t channel, uint8_t note,
	uint8_t velocity)
{
	this->writeDelay(delay);

	this->nextEvent.push_back(0x00 | channel);
	this->nextEvent.push_back(note);
	return;
}

void MUSEncoder::midiNoteOn(uint32_t delay, uint8_t channel, uint8_t note,
	uint8_t velocity)
{
	this->writeDelay(delay);

	this->nextEvent.push_back(0x10 | channel);
	unsigned int velFlag = (velocity != this->lastVelocity) ? 0x80 : 0;
	this->nextEvent.push_back(velFlag | note);
	if (velFlag) {
		// Don't need to scale velocity as it's already scaled to MIDI (0..127)
		this->nextEvent.push_back(velocity);
		this->lastVelocity = velocity;
	}
	return;
}

void MUSEncoder::midiPatchChange(uint32_t delay, uint8_t channel,
	uint8_t instrument)
{
	if (instrument > 127) {
		throw format_limitation("DMX MUS files can only have up to 127 instruments.");
	}
	this->writeDelay(delay);

	this->nextEvent.push_back(0x40 | channel);
	this->nextEvent.push_back(0x00);
	this->nextEvent.push_back(instrument);
	return;
}

void MUSEncoder::midiController(uint32_t delay, uint8_t channel,
	uint8_t controller, uint8_t value)
{
	this->writeDelay(delay);

	unsigned int musController;
	switch (controller) {
		case 0: musController = 1; break;
		case 1: musController = 2; break;
		case 7: musController = 3; break;
		case 10: musController = 4; break;
		case 11: musController = 5; break;
		case 91: musController = 6; break;
		case 93: musController = 7; break;
		case 64: musController = 8; break;
		case 67: musController = 9; break;
		default:
			std::cerr << "mus-dmx: Dropping unsupported MIDI controller "
				<< controller << "\n";
			// Replace with (hopefully) unused controller
			musController = 255;
			value = 0;
			break;
	}
	this->nextEvent.push_back(0x40 | channel);
	this->nextEvent.push_back(musController);
	this->nextEvent.push_back(value);
	return;
}

void MUSEncoder::midiPitchbend(uint32_t delay, uint8_t channel, uint16_t bend)
{
	if ((bend < 4096) || (bend > 8192+4095)) {
		throw format_limitation("MUS cannot pitchbend beyond +/- 1 semitone");
	}
	this->writeDelay(delay);

	// 4096..12288 -> 0..255
	int musBend = (bend >> 5) - 128;
	assert((musBend >= 0) && (musBend < 256));

	this->nextEvent.push_back(0x20 | channel);
	this->nextEvent.push_back(musBend);
	return;
}

void MUSEncoder::midiSetTempo(uint32_t delay, const Tempo& tempo)
{
	this->usPerTick = tempo.usPerTick;
	return;
}

void MUSEncoder::endOfTrack()
{
	return;
}

void MUSEncoder::endOfPattern()
{
	return;
}

void MUSEncoder::endOfSong(uint32_t delay)
{
	// Write an end-of-song event
	this->writeDelay(delay);
	this->content.write("\x60", 1);
	return;
}
