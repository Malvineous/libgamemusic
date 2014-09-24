/**
 * @file   mus-cmf-creativelabs.cpp
 * @brief  Support for Creative Labs' CMF format.
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
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/opl-util.hpp>
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "mus-cmf-creativelabs.hpp"

/// @todo Handle MIDI controller events for transpose up and down

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of available channels in a CMF file.
const unsigned int CMF_MAX_CHANNELS = 16;

/// Number of preset instruments (repeated from patch 0 to 128)
#define CMF_NUM_DEFAULT_INSTRUMENTS 16

static const char *CMF_DEFAULT_PATCHES =
"\x01\x11\x4F\x00\xF1\xD2\x53\x74\x00\x00\x06"
"\x07\x12\x4F\x00\xF2\xF2\x60\x72\x00\x00\x08"
"\x31\xA1\x1C\x80\x51\x54\x03\x67\x00\x00\x0E"
"\x31\xA1\x1C\x80\x41\x92\x0B\x3B\x00\x00\x0E"
"\x31\x16\x87\x80\xA1\x7D\x11\x43\x00\x00\x08"
"\x30\xB1\xC8\x80\xD5\x61\x19\x1B\x00\x00\x0C"
"\xF1\x21\x01\x00\x97\xF1\x17\x18\x00\x00\x08"
"\x32\x16\x87\x80\xA1\x7D\x10\x33\x00\x00\x08"
"\x01\x12\x4F\x00\x71\x52\x53\x7C\x00\x00\x0A"
"\x02\x03\x8D\x00\xD7\xF5\x37\x18\x00\x00\x04"
"\x21\x21\xD1\x00\xA3\xA4\x46\x25\x00\x00\x0A"
"\x22\x22\x0F\x00\xF6\xF6\x95\x36\x00\x00\x0A"
"\xE1\xE1\x00\x00\x44\x54\x24\x34\x02\x02\x07"
"\xA5\xB1\xD2\x80\x81\xF1\x03\x05\x00\x00\x02"
"\x71\x22\xC5\x00\x6E\x8B\x17\x0E\x00\x00\x02"
"\x32\x21\x16\x80\x73\x75\x24\x57\x00\x00\x0E";

std::string MusicType_CMF::getCode() const
{
	return "cmf-creativelabs";
}

std::string MusicType_CMF::getFriendlyName() const
{
	return "Creative Labs Music File";
}

std::vector<std::string> MusicType_CMF::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("cmf");
	return vcExtensions;
}

unsigned long MusicType_CMF::getCaps() const
{
	return
		InstEmpty
		| InstOPL
		| HasEvents
	;
}

MusicType::Certainty MusicType_CMF::isInstance(stream::input_sptr psMusic) const
{
	// Make sure the signature matches
	// TESTED BY: mus_cmf_creativelabs_isinstance_c01
	char sig[4];
	psMusic->seekg(0, stream::start);
	psMusic->read(sig, 4);
	if (strncmp(sig, "CTMF", 4) != 0) return MusicType::DefinitelyNo;

	// Make sure the header says it's version 1.0 or 1.1
	// TESTED BY: mus_cmf_creativelabs_isinstance_c02 (wrong ver)
	// TESTED BY: mus_cmf_creativelabs_isinstance_c03 (1.0)
	uint16_t ver;
	psMusic >> u16le(ver);
	if ((ver != 0x100) && (ver != 0x101)) return MusicType::DefinitelyNo;

	// TESTED BY: mus_cmf_creativelabs_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_CMF::read(stream::input_sptr input, SuppData& suppData) const
{
	// Skip CTMF header.  This is an absolute seek as it will be by far the most
	// common situation and avoids a lot of complexity because the header includes
	// absolute file offsets, which we thus won't have to adjust.
	input->seekg(4, stream::start); // skip CTMF header

	uint16_t ver, offInst, ticksPerQuarter, ticksPerSecond;
	uint16_t offMusic, offTitle, offComposer, offRemarks;
	input
		>> u16le(ver)
		>> u16le(offInst)
		>> u16le(offMusic)
		>> u16le(ticksPerQuarter)
		>> u16le(ticksPerSecond)
		>> u16le(offTitle)
		>> u16le(offComposer)
		>> u16le(offRemarks)
	;

	// Skip channel-in-use table as we don't need it
	input->seekg(16, stream::cur);

	// Rest of header depends on file version
	uint16_t numInstruments;
	switch (ver) {
		case 0x100: {
			uint8_t temp;
			input
				>> u8(temp)
			;
			numInstruments = temp;
			break;
		}
		default: // do this so you can force-open an unknown version
			std::cerr << "Warning: Unknown CMF version " << (int)(ver >> 8) << '.'
				<< (int)(ver & 0xFF) << ", proceeding as if v1.1" << std::endl;
		case 0x101:
			input
				>> u16le(numInstruments)
			;
			// Skip uint16le tempo value (unknown use)
			input->seekg(2, stream::cur);
			break;
	}

	// Process the MIDI data
	input->seekg(offMusic, stream::start);
	Tempo initialTempo;
	initialTempo.hertz(ticksPerSecond);
	initialTempo.ticksPerQuarterNote(ticksPerQuarter);
	MusicPtr music = midiDecode(input, MIDIFlags::UsePatchIndex
		| MIDIFlags::CMFExtensions, initialTempo);

	for (TrackInfoVector::iterator
		ti = music->trackInfo.begin(); ti != music->trackInfo.end(); ti++
	) {
		if (ti->channelIndex >= 11) {
			ti->channelType = TrackInfo::OPLPercChannel;
			ti->channelIndex = 15 - ti->channelIndex;
		} else {
			ti->channelType = TrackInfo::OPLChannel;
			if (ti->channelIndex > 8) {
				std::cout << "cmf-creativelabs: Event on MIDI channel "
					<< ti->channelIndex << " - map this to a valid OPL channel!\n";
			}
			// TODO: remap channels based on channel-in-use table?
		}
	}

	// Set standard CMF settings
	TrackPtr configTrack = music->patterns.at(0)->at(0);
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableDeepTremolo;
		ev->value = 1;
		configTrack->insert(configTrack->begin() + 0, te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableDeepVibrato;
		ev->value = 1;
		configTrack->insert(configTrack->begin() + 1, te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableWaveSel;
		ev->value = 1;
		configTrack->insert(configTrack->begin() + 2, te);
	}

	// Read the instruments
	PatchBankPtr oplBank(new PatchBank());
	oplBank->reserve(numInstruments);
	input->seekg(offInst, stream::start);
	for (unsigned int i = 0; i < numInstruments; i++) {
		OPLPatchPtr patch(new OPLPatch());
		uint8_t inst[16];
		input->read((char *)inst, 16);

		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = (inst[0 + op] >> 7) & 1;
			o->enableVibrato = (inst[0 + op] >> 6) & 1;
			o->enableSustain = (inst[0 + op] >> 5) & 1;
			o->enableKSR     = (inst[0 + op] >> 4) & 1;
			o->freqMult      =  inst[0 + op] & 0x0F;
			o->scaleLevel    =  inst[2 + op] >> 6;
			o->outputLevel   =  inst[2 + op] & 0x3F;
			o->attackRate    =  inst[4 + op] >> 4;
			o->decayRate     =  inst[4 + op] & 0x0F;
			o->sustainRate   =  inst[6 + op] >> 4;
			o->releaseRate   =  inst[6 + op] & 0x0F;
			o->waveSelect    =  inst[8 + op] & 0x07;
			o = &patch->c;
		}
		patch->feedback    = (inst[10] >> 1) & 0x07;
		patch->connection  =  inst[10] & 1;
		patch->rhythm      = 0;

		oplBank->push_back(patch);
	}

	// Load the default instruments
	int genericMapping[CMF_NUM_DEFAULT_INSTRUMENTS];
	PatchBankPtr oplBankDefault(new PatchBank());
	oplBank->reserve(CMF_NUM_DEFAULT_INSTRUMENTS);
	for (unsigned int i = 0; i < CMF_NUM_DEFAULT_INSTRUMENTS; i++) {
		OPLPatchPtr patch(new OPLPatch());
		uint8_t *inst = (uint8_t *)(CMF_DEFAULT_PATCHES + i * 16);

		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = (inst[0 + op] >> 7) & 1;
			o->enableVibrato = (inst[0 + op] >> 6) & 1;
			o->enableSustain = (inst[0 + op] >> 5) & 1;
			o->enableKSR     = (inst[0 + op] >> 4) & 1;
			o->freqMult      =  inst[0 + op] & 0x0F;
			o->scaleLevel    =  inst[2 + op] >> 6;
			o->outputLevel   =  inst[2 + op] & 0x3F;
			o->attackRate    =  inst[4 + op] >> 4;
			o->decayRate     =  inst[4 + op] & 0x0F;
			o->sustainRate   =  inst[6 + op] >> 4;
			o->releaseRate   =  inst[6 + op] & 0x0F;
			o->waveSelect    =  inst[8 + op] & 0x07;
			o = &patch->c;
		}
		patch->feedback    = (inst[10] >> 1) & 0x07;
		patch->connection  =  inst[10] & 1;
		patch->rhythm      = 0;

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
	for (std::vector<unsigned int>::const_iterator
		o = music->patternOrder.begin(); o != music->patternOrder.end(); o++
	) {
		const unsigned int& patternIndex = *o;
		const PatternPtr& pattern = music->patterns[patternIndex];

		// For each track
		unsigned int trackIndex = 0;
		for (Pattern::const_iterator
			pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++
		) {
			const TrackInfo& ti = music->trackInfo[trackIndex];

			// For each event in the track
			for (Track::const_iterator
				ev = (*pt)->begin(); ev != (*pt)->end(); ev++
			) {
				const TrackEvent& te = *ev;
				NoteOnEvent *nev = dynamic_cast<NoteOnEvent *>(te.event.get());
				if (!nev) continue;

				// The velocity is loaded directly into the OPL chip, which uses a
				// logarithmic volume scale rather than MIDI's linear one.
				nev->velocity = 255 - log_volume_to_lin_velocity(255 - nev->velocity, 255);

				// Figure out which rhythm instrument (if any) this is
				// If there's a mapping (CMF instrument block # to patchbank #), use that, otherwise:
				int targetRhythm;
				if (ti.channelType == TrackInfo::OPLPercChannel) {
					targetRhythm = ti.channelIndex + 1;
				} else {
					targetRhythm = 0;
				}

				// Figure out what CMF instrument number to play
				MIDIPatchPtr midiInst = boost::dynamic_pointer_cast<MIDIPatch>(music->patches->at(nev->instrument));
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
		}
	}

	// Disregard the MIDI patches and use the OPL ones
	music->patches = oplBank;

	// Read metadata
	if (offTitle) {
		input->seekg(offTitle, stream::start);
		input >> nullTerminated(music->metadata[Metadata::Title], 256);
	}
	if (offComposer) {
		input->seekg(offComposer, stream::start);
		input >> nullTerminated(music->metadata[Metadata::Author], 256);
	}
	if (offRemarks) {
		input->seekg(offRemarks, stream::start);
		input >> nullTerminated(music->metadata[Metadata::Description], 256);
	}

	// Swap operators for required percussive patches
	oplDenormalisePerc(music, OPLPerc_CarFromMod);

	return music;
}

void MusicType_CMF::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	requirePatches<OPLPatch>(music->patches);
	if (music->patches->size() >= MIDI_PATCHES) {
		throw bad_patch("CMF files have a maximum of 128 instruments.");
	}

	// Swap operators for required percussive patches
	PatchBankPtr patches = oplNormalisePerc(music, OPLPerc_CarFromMod);

	output->write(
		"CTMF"
		"\x01\x01" // version 1.1
	, 6);

	uint16_t offNext = 20 + 16 + 4;
	uint16_t lenText[3], offText[3];

	// Get lengths from metadata
	Metadata::TypeMap::const_iterator itTitle = music->metadata.find(Metadata::Title);
	Metadata::TypeMap::const_iterator itComposer = music->metadata.find(Metadata::Author);
	Metadata::TypeMap::const_iterator itDesc = music->metadata.find(Metadata::Description);
	if (itTitle != music->metadata.end()) {
		if (itTitle->second.length() > 65535) {
			throw format_limitation("The metadata element 'Title' is longer than "
				"the maximum 65535 character length.");
		}
		lenText[0] = itTitle->second.length();
	} else {
		lenText[0] = 0; // no tag
	}
	if (itComposer != music->metadata.end()) {
		if (itComposer->second.length() > 65535) {
			throw format_limitation("The metadata element 'Author' is longer than "
				"the maximum 65535 character length.");
		}
		lenText[1] = itComposer->second.length();
	} else {
		lenText[1] = 0; // no tag
	}
	if (itDesc != music->metadata.end()) {
		if (itDesc->second.length() > 65535) {
			throw format_limitation("The metadata element 'Description' is longer than "
				"the maximum 65535 character length.");
		}
		lenText[2] = itDesc->second.length();
	} else {
		lenText[2] = 0; // no tag
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

	output
		<< u16le(offInst)
		<< u16le(offMusic)
	;
	output->write(
		"\x00\x00" // ticks per quarter note
		"\x00\x00" // ticks per second
	, 4);

	// Write offset of title, composer and remarks
	for (int i = 0; i < 3; i++) {
		output << u16le(offText[i]);
	}

	// Temporary channel-in-use table
	for (int i = 0; i < 16; i++) output << u8(1);

	output
		<< u16le(numInstruments)
		<< u16le(0) // TODO: default value for 'basic tempo'
	;

	// Write title, composer and remarks strings here (null terminated)
	if (itTitle != music->metadata.end()) {
		output << nullTerminated(itTitle->second, 65535);
	}
	if (itComposer != music->metadata.end()) {
		output << nullTerminated(itComposer->second, 65535);
	}
	if (itDesc != music->metadata.end()) {
		output << nullTerminated(itDesc->second, 65535);
	}

	// Create a new TrackInfo that moves everything onto the correct MIDI channels
	TrackInfoVector midiTrackInfo;
	for (TrackInfoVector::iterator
		ti = music->trackInfo.begin(); ti != music->trackInfo.end(); ti++
	) {
		TrackInfo nti;
		nti.channelType = TrackInfo::MIDIChannel;
		if (ti->channelType == TrackInfo::OPLPercChannel) {
			nti.channelIndex = 15 - ti->channelIndex;
		} else {
			nti = *ti;
		}
		if (nti.channelIndex > 15) {
			throw format_limitation("CMF files can only have up to 16 channels.");
		}
		midiTrackInfo.push_back(nti);
	}
	MusicPtr musicMIDI(new Music);
	*musicMIDI = *music;
	musicMIDI->trackInfo = midiTrackInfo;
	// musicMIDI is now the same as music, but with the CMF MIDI track assignment
	// for midiEncode to use to place percussive events on the right channels.

	for (int i = 0; i < numInstruments; i++) {
		uint8_t inst[16];
		OPLPatchPtr patch = boost::dynamic_pointer_cast<OPLPatch>(patches->at(i));
		assert(patch);

		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			inst[0 + op] =
				((o->enableTremolo & 1) << 7) |
				((o->enableVibrato & 1) << 6) |
				((o->enableSustain & 1) << 5) |
				((o->enableKSR     & 1) << 4) |
				 (o->freqMult      & 0x0F)
			;
			inst[2 + op] = (o->scaleLevel << 6) | (o->outputLevel & 0x3F);
			inst[4 + op] = (o->attackRate << 4) | (o->decayRate & 0x0F);
			inst[6 + op] = (o->sustainRate << 4) | (o->releaseRate & 0x0F);
			inst[8 + op] =  o->waveSelect & 7;

			if (OPL_IS_RHYTHM_CARRIER_ONLY(patch->rhythm)) break; // ignore other op
			o = &patch->c;
		}
		inst[10] = ((patch->feedback & 7) << 1) | (patch->connection & 1);
		inst[11] = inst[12] = inst[13] = inst[14] = inst[15] = 0;

		output->write((char *)inst, 16);
	}

	// Call the generic OPL writer.
	bool channelsUsed[MIDI_CHANNELS];
	unsigned long midiFlags = MIDIFlags::UsePatchIndex | MIDIFlags::CMFExtensions;
	if (flags & MusicType::IntegerNotesOnly) {
		midiFlags |= MIDIFlags::IntegerNotesOnly;
	}
	midiEncode(output, musicMIDI, midiFlags, channelsUsed,
		EventHandler::Order_Row_Track, NULL);

	// Set final filesize to this
	output->truncate_here();
	output->seekp(10, stream::start);
	output
		<< u16le(music->initialTempo.ticksPerQuarterNote())
		<< u16le(music->initialTempo.hertz())
	;

	// Update the channel-in-use table
	uint8_t channelsInUse[MIDI_CHANNELS];
	for (unsigned int i = 0; i < MIDI_CHANNELS; i++) {
		channelsInUse[i] = channelsUsed[i] ? 1 : 0;
	}
	output->seekp(20, stream::start);
	output->write((char *)channelsInUse, 16);

	return;
}

SuppFilenames MusicType_CMF::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_CMF::getMetadataList() const
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	types.push_back(Metadata::Author);
	types.push_back(Metadata::Description);
	return types;
}
