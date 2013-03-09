/**
 * @file   mus-cdfm-zone66.cpp
 * @brief  Support for Renaissance's CDFM format used in Zone 66.
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

#include <boost/pointer_cast.hpp>
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp> // midiToFreq
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>
#include "mus-cdfm-zone66.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

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

MusicType::Certainty MusicType_CDFM::isInstance(stream::input_sptr input) const
{
	stream::len fileSize = input->size();
	input->seekg(0, stream::start);

	uint8_t tempo, patternListSize, numPatterns, numDigInst, numOPLInst, loopDest;
	uint32_t sampleOffset;
	uint8_t patternOrder[256];
	uint32_t patternOffset[256];
	input
		>> u8(tempo)
		>> u8(patternListSize)
		>> u8(numPatterns)
		>> u8(numDigInst)
		>> u8(numOPLInst)
		>> u8(loopDest)
		>> u32le(sampleOffset)
	;
	if (loopDest >= patternListSize) {
		// Loop target is past end of song
		// TESTED BY: mus_cdfm_zone66_isinstance_c01
		return MusicType::DefinitelyNo;
	}

	input->read(patternOrder, patternListSize);
	for (int i = 0; i < patternListSize; i++) {
		if (patternOrder[i] >= numPatterns) {
			// Sequence specifies invalid pattern
			// TESTED BY: mus_cdfm_zone66_isinstance_c02
			return MusicType::DefinitelyNo;
		}
	}

	// Read the song data
	stream::pos patternStart =
		10                     // sizeof fixed-length part of header
		+ 1 * patternListSize  // one byte for each pattern in the sequence
		+ 4 * numPatterns      // one uint32le for each pattern's offset
		+ 16 * numDigInst      // PCM instruments
		+ 11 * numOPLInst      // OPL instruments
	;

	for (int i = 0; i < numPatterns; i++) {
		input >> u32le(patternOffset[i]);
		if (patternStart + patternOffset[i] >= fileSize) {
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
	stream::len fileSize = input->size();
	input->seekg(0, stream::start);

	uint8_t tempo, patternListSize, numPatterns, numDigInst, numOPLInst, loopDest;
	uint32_t sampleOffset;
	uint8_t patternOrder[256];
	uint32_t patternOffset[256];
	input
		>> u8(tempo)
		>> u8(patternListSize)
		>> u8(numPatterns)
		>> u8(numDigInst)
		>> u8(numOPLInst)
		>> u8(loopDest)
		>> u32le(sampleOffset)
	;
	input->read(patternOrder, patternListSize);
	for (int i = 0; i < numPatterns; i++) {
		input >> u32le(patternOffset[i]);
	}

	MusicPtr music(new Music());
	PatchBankPtr patches(new PatchBank());
	music->patches = patches;
	music->events.reset(new EventVector());
	music->ticksPerQuarterNote = 192; ///< @todo see if there's one value for this format

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
		patch->sampleRate = 33075;
		patch->bitDepth = 8;
		patch->numChannels = 1;
		if (patch->loopEnd == 0x00FFFFFF) patch->loopEnd = 0; // no loop
		patches->push_back(patch);
	}

	uint8_t inst[11];
	for (int i = 0; i < numOPLInst; i++) {
		OPLPatchPtr patch(new OPLPatch());
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
	stream::pos patternStart =
		10                     // sizeof fixed-length part of header
		+ 1 * patternListSize  // one byte for each pattern in the sequence
		+ 4 * numPatterns      // one uint32le for each pattern's offset
		+ 16 * numDigInst      // PCM instruments
		+ 11 * numOPLInst      // OPL instruments
	;
	assert(input->tellg() == patternStart);

	EventPtr gev;

	// Add the tempo event first
	{
		TempoEvent *ev = new TempoEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->usPerTick = tempo * 1750;
		music->events->push_back(gev);
	}

	// Set standard settings
	{
		ConfigurationEvent *ev = new ConfigurationEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->configType = ConfigurationEvent::EnableOPL3;
		ev->value = 0;
		music->events->push_back(gev);
	}
	{
		ConfigurationEvent *ev = new ConfigurationEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->configType = ConfigurationEvent::EnableDeepTremolo;
		ev->value = 0;
		music->events->push_back(gev);
	}
	{
		ConfigurationEvent *ev = new ConfigurationEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->configType = ConfigurationEvent::EnableDeepVibrato;
		ev->value = 0;
		music->events->push_back(gev);
	}
	{
		ConfigurationEvent *ev = new ConfigurationEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->configType = ConfigurationEvent::EnableWaveSel;
		ev->value = 1;
		music->events->push_back(gev);
	}

	bool noteOn[16]; // true if a note is currently playing on this channel
	for (int i = 0; i < 16; i++) {
		noteOn[i] = false;
	}

	unsigned long tick = 0; // Last event's time
	for (int i = 0; i < patternListSize; i++) {
		int pattern = patternOrder[i];
		assert(pattern < numPatterns);
		stream::pos offThisPattern = patternStart + patternOffset[pattern];
		stream::pos remaining = fileSize - offThisPattern;
		input->seekg(offThisPattern, stream::start);
		while (remaining > 0) {
			gev.reset();
			uint8_t cmd;
			input >> u8(cmd);
			remaining--;
			uint8_t channel = (cmd & 0x0F) + 1; // libgamemusic channel 0 is all channels
			switch (cmd & 0xF0) {
				case 0x00: { // note on
					uint8_t freq, inst_vel;
					input >> u8(freq) >> u8(inst_vel);
					remaining -= 2;
					freq %= 128; // notes larger than this wrap

					if (noteOn[channel]) {
						// A note is already playing, so turn it off first
						NoteOffEvent *evOff = new NoteOffEvent();
						gev.reset(evOff);
						evOff->channel = channel;
						evOff->absTime = tick;
						music->events->push_back(gev);
					} else {
						noteOn[channel] = true;
					}

					NoteOnEvent *ev = new NoteOnEvent();
					gev.reset(ev);
					ev->channel = channel;
					ev->instrument = inst_vel >> 4;
					if (channel > 4) {
						// This is an OPL channel so increment instrument index
						ev->instrument += numDigInst;
					}
					ev->milliHertz = midiToFreq(12 * ((freq >> 4)+1) + (freq & 0x0f));

					ev->velocity = ((inst_vel & 0x0F) << 4) | (inst_vel & 0x0F);
					break;
				}
				case 0x20: { // set volume
					uint8_t vol;
					input >> u8(vol);
					remaining--;

					if (vol == 0) {
						if (noteOn[channel]) {
							// A note is already playing, so turn it off first
							NoteOffEvent *evOff = new NoteOffEvent();
							gev.reset(evOff);
							evOff->channel = channel;
							evOff->absTime = tick;
							music->events->push_back(gev);
						}
					} else {
						std::cerr << "mus-cdfm-zone66: TODO - set volume on channel "
							<< (int)channel << std::endl;
					}
					break;
				}
				case 0x40: { // delay
					uint8_t data;
					input >> u8(data);
					remaining--;
					tick += data * 10;// * tempo * 17.5;
					break;
				}
				case 0x60: // end of pattern
					remaining = 0;
					break;
				default:
					std::cerr << "mus-cdfm-zone66: Unknown event " << (int)cmd
						<< " at offset " << (int)(fileSize-remaining) << std::endl;
					break;
			}
			if (gev) {
				gev->absTime = tick;
				music->events->push_back(gev);
			}
		}
	}

	for (int i = 0; i < 16; i++) {
		if (noteOn[i]) {
			// A note is still playing, so turn it off
			NoteOffEvent *evOff = new NoteOffEvent();
			gev.reset(evOff);
			evOff->channel = i;
			evOff->absTime = tick;
			music->events->push_back(gev);
		}
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
