/**
 * @file   mus-mus-dmx.cpp
 * @brief  Support for id Software MIDI (.mus) format.
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

#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include "track-split.hpp"
#include "mus-mus-dmx.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Total channels (even if they're not all used)
#define MUS_CHANNEL_COUNT 16

/// Class taking MIDI events and producing MUS data.
class MUSEncoder: virtual private MIDIEventCallback
{
	public:
		/// Set encoding parameters.
		/**
		 * @param output
		 *   Data stream to write the MIDI data to.
		 *
		 * @param music
		 *   The instance to convert to MIDI data.
		 *
		 * @param musClock
		 *   MUS clock speed in Hertz (e.g. 140).
		 */
		MUSEncoder(stream::output_sptr output, ConstMusicPtr music,
			unsigned int musClock);

		/// Destructor.
		~MUSEncoder();

		/// Process the events, and write out MIDI data.
		/**
		 * This function will write SMF (standard MIDI format) data to the output
		 * stream, until all the events in the song have been written out.
		 *
		 * @param channelsUsed
		 *   Pointer to an array of MIDI_CHANNELS entries of bool.  On return, this
		 *   each entry is set to true where that MIDI channel was used.  Set to
		 *   NULL if this information is not required.
		 *
		 * @throw stream:error
		 *   If the output data could not be written for some reason.
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
		stream::output_sptr output;        ///< Target stream for SMF MIDI data
		ConstMusicPtr music;               ///< Song to convert
		unsigned int musClock;             ///< MUS clock speed in Hertz
		unsigned int lastVelocity;         ///< Last velocity value set
		unsigned long usPerTick;           ///< Current tempo
		std::vector<uint8_t> nextEvent;    ///< Next event to write

		/// Write delay bytes and the previous event.
		void writeDelay(unsigned int delay);
};


MusicType_MUS_Raptor::MusicType_MUS_Raptor()
{
	this->tempo = 70;
}

std::string MusicType_MUS_Raptor::getCode() const
{
	return "mus-dmx-raptor";
}

std::string MusicType_MUS_Raptor::getFriendlyName() const
{
	return "DMX audio library MIDI File (Raptor tempo)";
}


MusicType_MUS::MusicType_MUS()
	:	tempo(140)
{
}

std::string MusicType_MUS::getCode() const
{
	return "mus-dmx";
}

std::string MusicType_MUS::getFriendlyName() const
{
	return "DMX audio library MIDI File (normal tempo)";
}

std::vector<std::string> MusicType_MUS::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("mus");
	return vcExtensions;
}

unsigned long MusicType_MUS::getCaps() const
{
	return
		InstMIDI
		| HasEvents
	;
}

MusicType::Certainty MusicType_MUS::isInstance(stream::input_sptr input) const
{
	// Too short
	// TESTED BY: mus_mus_dmx_isinstance_c01
	if (input->size() < 4 + 2 * 6) return MusicType::DefinitelyNo;

	input->seekg(0, stream::start);
	uint8_t sig[4];
	try {
		input->read(sig, 4);
	} catch (...) {
		return DefinitelyNo;
	}
	if (
		(sig[0] != 'M') ||
		(sig[1] != 'U') ||
		(sig[2] != 'S') ||
		(sig[3] != 0x1A)
	) {
		// TESTED BY: mus_mus_dmx_isinstance_c02
		return DefinitelyNo;
	}

	// TESTED BY: mus_mus_dmx_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_MUS::read(stream::input_sptr input, SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	input->seekg(0, stream::start);

	MusicPtr music(new Music());
	PatchBankPtr patches(new PatchBank());
	music->patches = patches;
	music->initialTempo.hertz(this->tempo);
	music->initialTempo.ticksPerQuarterNote(64); // some random value in the ballpark

	unsigned long lastDelay[MUS_CHANNEL_COUNT];
	uint8_t musActivePatch[MUS_CHANNEL_COUNT]; ///< Which instruments are in use on which channel
	uint8_t volMap[MUS_CHANNEL_COUNT];   ///< Volume set on each channel
	int instMap[256];                    ///< MIDI patch to libgamemusic instrument map

	memset(musActivePatch, 0xFF, sizeof(musActivePatch));
	memset(volMap, 0, sizeof(volMap));
	memset(instMap, 0xFF, sizeof(instMap));

	PatternPtr pattern(new Pattern());
	for (unsigned int c = 0; c < MUS_CHANNEL_COUNT; c++) {
		TrackInfo t;
		t.channelType = TrackInfo::MIDIChannel;
		t.channelIndex = c;
		music->trackInfo.push_back(t);

		TrackPtr track(new Track());
		pattern->push_back(track);
		lastDelay[c] = 0;
	}
	music->patterns.push_back(pattern);
	music->patternOrder.push_back(0);

	input->seekg(4, stream::start);

	uint16_t lenSong, offSong, priChan, secChan, numInst, reserved;
	input
		>> u16le(lenSong)
		>> u16le(offSong)
		>> u16le(priChan)
		>> u16le(secChan)
		>> u16le(numInst)
		>> u16le(reserved)
	;

	// Read the instruments
	patches->reserve(numInst);
	for (int i = 0; i < numInst; i++) {
		MIDIPatchPtr patch(new MIDIPatch());
		input >> u16le(patch->midiPatch);
		patch->percussion = false;
		patches->push_back(patch);
		instMap[patch->midiPatch] = i;
	}

	input->seekg(offSong, stream::start);

	unsigned long totalDelay = 0; // full delay for entire song
	bool eof = false;
	while (!eof) {
		uint8_t code;
		try {
			input >> u8(code);
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
				input >> u8(n);
				TrackEvent te;
				te.delay = lastDelay[track];
				lastDelay[track] = 0;
				SpecificNoteOffEvent *ev = new SpecificNoteOffEvent();
				te.event.reset(ev);
				ev->milliHertz = midiToFreq(n);
				pattern->at(track)->push_back(te);
				break;
			}
			case 0x1: { // note on
				uint8_t n;
				input >> u8(n);
				if (n & 0x80) {
					input >> u8(volMap[channel]);
				} // else reuse last value
				n &= 0x7F;
				unsigned int libPatch;
				if (channel == 15) {
					// percussion
					if (instMap[128 + n] < 0) {
						MIDIPatchPtr patch(new MIDIPatch());
						patch->midiPatch = n;
						patch->percussion = true;
						instMap[128 + n] = patches->size();
						patches->push_back(patch);
					}
					libPatch = instMap[128 + n];
				} else {
					if (instMap[musActivePatch[channel]] < 0) {
						std::cout << "mus-dmx: Instrument " << musActivePatch[channel]
							<< " was selected but not listed in the file's instrument list!"
							<< std::endl;
						MIDIPatchPtr patch(new MIDIPatch());
						patch->midiPatch = musActivePatch[channel];
						patch->percussion = false;
						instMap[patch->midiPatch] = patches->size();
						patches->push_back(patch);
					}
					libPatch = instMap[musActivePatch[channel]];
				}
				unsigned int& track = channel;
				TrackEvent te;
				te.delay = lastDelay[track];
				lastDelay[track] = 0;
				NoteOnEvent *ev = new NoteOnEvent();
				te.event.reset(ev);
				ev->instrument = libPatch;
				ev->milliHertz = midiToFreq(n);
				ev->velocity = (volMap[channel] << 1) | (volMap[channel] >> 6);
				pattern->at(track)->push_back(te);
				break;
			}
			case 0x2: { // pitchbend
				uint8_t bend;
				input >> u8(bend);

				double bendSemitones = (bend - 128.0) / 128.0;

				const unsigned int& track = channel;
				TrackEvent te;
				te.delay = lastDelay[track];
				lastDelay[track] = 0;
				PolyphonicEffectEvent *ev = new PolyphonicEffectEvent();
				te.event.reset(ev);
				ev->type = (EffectEvent::EffectType)PolyphonicEffectEvent::PitchbendChannel;
				ev->data = midiSemitonesToPitchbend(bendSemitones);
				pattern->at(track)->push_back(te);
				break;
			}
			case 0x3: { // system event
				uint8_t controller;
				input >> u8(controller);
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
				input >> u8(controller) >> u8(value);
				switch (controller) {
					case 0: // patch change
						musActivePatch[channel] = value & 0x7F;
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
				input >> u8(delayVal);
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
	for (unsigned int track = 0; track < trackCount; track++) {
		if (lastDelay[track] == totalDelay) {
			// This track is unused
			music->trackInfo.erase(music->trackInfo.begin() + track);
			music->patterns.at(0)->at(track);
			track--;
			trackCount--;
		} else if (lastDelay[track]) {
			// This track has a trailing delay
			TrackEvent te;
			te.delay = lastDelay[track];
			lastDelay[track] = 0;
			ConfigurationEvent *ev = new ConfigurationEvent();
			te.event.reset(ev);
			ev->configType = ConfigurationEvent::None;
			ev->value = 0;
			pattern->at(track)->push_back(te);
		}
	}

	music->ticksPerTrack = totalDelay;

	splitPolyphonicTracks(music);
	return music;
}

void MusicType_MUS::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	requirePatches<MIDIPatch>(music->patches);

	// Count the number of unique MIDI channels in use
	std::map<unsigned int, bool> activeChannels;
	for (TrackInfoVector::const_iterator
		i = music->trackInfo.begin(); i != music->trackInfo.end(); i++
	) {
		if (i->channelType != TrackInfo::MIDIChannel) continue;
		activeChannels[i->channelIndex] = true;
	}
	unsigned int channelCount = 0;
	for (std::map<unsigned int, bool>::const_iterator
		i = activeChannels.begin(); i != activeChannels.end(); i++
	) {
		channelCount++;
	}

	stream::pos offSong = 4 + 2*6 + 2*music->patches->size();
	output
		<< nullPadded("MUS\x1A", 4)
		<< u16le(0xFFFF) // song length placeholder
		<< u16le(offSong)
		<< u16le(channelCount) // primary channel count
		<< u16le(0) // secondary channel count
		<< u16le(music->patches->size())
		<< u16le(0) // reserved
	;

	for (PatchBank::const_iterator
		i = music->patches->begin(); i != music->patches->end(); i++
	) {
		MIDIPatchPtr midiPatch = boost::dynamic_pointer_cast<MIDIPatch>(*i);
		assert(midiPatch);
		unsigned int patch = midiPatch->midiPatch;
		if (midiPatch->percussion) patch += 128;
		output << u16le(patch);
	}

	MUSEncoder conv(output, music, this->tempo);
	conv.encode();

	// Go back and write the song length
	stream::pos posEnd = output->tellp();
	output->seekp(4, stream::start);
	output << u16le(posEnd - offSong);
	output->seekp(posEnd, stream::start);

	// Set final filesize to this
	output->truncate_here();

	return;
}

SuppFilenames MusicType_MUS::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_MUS::getMetadataList() const
{
	Metadata::MetadataTypes types;
	return types;
}


MUSEncoder::MUSEncoder(stream::output_sptr output, ConstMusicPtr music,
	unsigned int musClock)
	:	output(output),
		music(music),
		musClock(Tempo::US_PER_SEC / musClock),
		lastVelocity(-1)
{
	this->usPerTick = music->initialTempo.usPerTick;
}

MUSEncoder::~MUSEncoder()
{
}

void MUSEncoder::encode()
{
	EventConverter_MIDI conv(this, this->music, MIDIFlags::Default);
	conv.handleAllEvents(EventHandler::Order_Row_Track);
	return;
}

void MUSEncoder::writeDelay(unsigned int delay)
{
	if (delay != 0) {
		this->nextEvent[0] |= 0x80;
	}

	for (std::vector<uint8_t>::iterator
		i = this->nextEvent.begin(); i != this->nextEvent.end(); i++
	) {
		this->output << u8(*i);
	}
	this->nextEvent.clear();

	delay = delay * this->usPerTick / this->musClock;

	// Write the three most-significant bytes as 7-bit bytes, with the most
	// significant bit set.
	uint8_t next;
	if (delay >= (1 << 28)) { next = 0x80 | ((delay >> 28) & 0x7F); this->output << u8(next); }
	if (delay >= (1 << 21)) { next = 0x80 | ((delay >> 21) & 0x7F); this->output << u8(next); }
	if (delay >= (1 << 14)) { next = 0x80 | ((delay >> 14) & 0x7F); this->output << u8(next); }
	if (delay >= (1 <<  7)) { next = 0x80 | ((delay >>  7) & 0x7F); this->output << u8(next); }
	if (delay >= (1 <<  0)) { next =        ((delay >>  0) & 0x7F); this->output << u8(next); }

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
			controller = 255;
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
	this->output->write("\x60", 1);
	return;
}
