/**
 * @file   mus-cmf-creativelabs.cpp
 * @brief  Support for Creative Labs' CMF format.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "mus-cmf-creativelabs.hpp"

/// @todo Handle MIDI controller events for transpose up and down

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of available channels in a CMF file.
const int CMF_MAX_CHANNELS = 16;

std::string MusicType_CMF::getCode() const
	throw ()
{
	return "cmf-creativelabs";
}

std::string MusicType_CMF::getFriendlyName() const
	throw ()
{
	return "Creative Labs Music File";
}

std::vector<std::string> MusicType_CMF::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("cmf");
	return vcExtensions;
}

MusicType::Certainty MusicType_CMF::isInstance(stream::input_sptr psMusic) const
	throw (stream::error)
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
	throw (stream::error)
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
	unsigned long usPerQuarterNote = ticksPerQuarter * 1000000 / ticksPerSecond;
	MusicPtr music = midiDecode(input, MIDIFlags::BasicMIDIOnly, ticksPerQuarter,
		usPerQuarterNote);

	// Read the instruments
	PatchBankPtr oplPatches(new PatchBank());
	music->patches = oplPatches;
	oplPatches->setPatchCount(numInstruments);
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
		// These next two affect the entire synth (all instruments/channels)
		patch->deepTremolo = false;
		patch->deepVibrato = false;

		patch->rhythm      = 0;
		oplPatches->setPatch(i, patch);

		//this->patchMap[i] = i;
	}
	for (unsigned int i = numInstruments; i < 128; i++) {
		/// @todo Default CMF instruments - load from .ibk?
	}

	// Read metadata
	input->seekg(offTitle, stream::start);
	input >> nullTerminated(music->metadata[Metadata::Title], 256);
	input->seekg(offComposer, stream::start);
	input >> nullTerminated(music->metadata[Metadata::Author], 256);
	input->seekg(offRemarks, stream::start);
	input >> nullTerminated(music->metadata[Metadata::Description], 256);

	return music;
}

void MusicType_CMF::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
	throw (stream::error, format_limitation)
{
	assert(music->ticksPerQuarterNote != 0);

	requirePatches<OPLPatch>(music->patches);
	if (music->patches->getPatchCount() >= MIDI_PATCHES) {
		throw bad_patch("CMF files have a maximum of 128 instruments.");
	}

	uint8_t channelsInUse[CMF_MAX_CHANNELS];
	memset(channelsInUse, 0, sizeof(channelsInUse));

	output->write(
		"CTMF"
		"\x01\x01" // version 1.1
	, 6);

	uint16_t offNext = 20 + 16 + 4;
	uint16_t lenText[3], offText[3];

	/// @todo Get lengths from metadata
	lenText[0] = lenText[1] = lenText[2] = 0;

	for (int i = 0; i < 3; i++) {
		if (lenText[i]) {
			offText[i] = offNext;
			offNext += lenText[i] + 1;
		} else {
			offText[i] = 0;
		}
	}
	uint16_t offInst = offNext;
	uint16_t numInstruments = music->patches->getPatchCount();
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

	/// @todo Write title, composer and remarks strings here (null terminated)

	for (int i = 0; i < numInstruments; i++) {
		uint8_t inst[16];
		OPLPatchPtr patch = boost::dynamic_pointer_cast<OPLPatch>(music->patches->getPatch(i));
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

			o = &patch->c;
		}
		inst[10] = ((patch->feedback & 7) << 1) | (patch->connection & 1);
		inst[11] = inst[12] = inst[13] = inst[14] = inst[15] = 0;

		/// @todo handle these
		//patch->deepTremolo = false;
		//patch->deepVibrato = false;

		output->write((char *)inst, 16);
	}

	// Call the generic OPL writer.
	unsigned long usPerTick;
	midiEncode(output, MIDIFlags::BasicMIDIOnly, music, &usPerTick);

	// Set final filesize to this
	output->truncate_here();
	uint16_t ticksPerSecond = 1000000 / usPerTick;
	output->seekp(10, stream::start);
	output
		<< u16le(music->ticksPerQuarterNote)
		<< u16le(ticksPerSecond)
	;

	// Update the channel-in-use table
	for (EventVector::iterator i =
		music->events->begin(); i != music->events->end(); i++)
	{
		NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(i->get());
		if ((ev) && (ev->channel > 0)) {
			assert((ev->channel - 1) < CMF_MAX_CHANNELS);
			channelsInUse[ev->channel - 1] = 1;
		}
	}
	output->seekp(20, stream::start);
	output->write((char *)channelsInUse, 16);

	return;
}

SuppFilenames MusicType_CMF::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_CMF::getMetadataList() const
	throw ()
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	types.push_back(Metadata::Author);
	types.push_back(Metadata::Description);
	return types;
}

/*
void MusicWriter_CMF::handleEvent(NoteOnEvent *ev)
	throw (stream::error, EChannelMismatch)
{
	// We need to override this event handler to do channel mapping appropriate
	// for OPL instruments instead of MIDI ones.

	// Make sure a patch bank has been set
	assert(this->patches);

	uint32_t delay = ev->absTime - this->lastTick;
	this->writeMIDINumber(delay);

	if (this->updateDeep) {
		// Need to set CMF controller 0x63
		uint8_t val = (this->deepTremolo ? 1 : 0) | (this->deepVibrato ? 1 : 0);
		this->writeCommand(0xB0);
		this->midi
			<< u8(0x63)
			<< u8(val)
			<< u8(0) // delay until next event
		;
		this->updateDeep = false;
	}

	OPLPatchPtr patch = this->patches->getTypedPatch(ev->instrument);
	uint8_t midiChannel;
	if (patch->rhythm) {
		midiChannel = 16 - patch->rhythm;
	} else {
		// Normal instrument
		midiChannel = this->getMIDIchannel(ev->channel, 11);
	}
	this->channelMap[ev->channel] = midiChannel;

	uint8_t note;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &note, &bend, 0xFF);
	if (!(this->flags & IntegerNotesOnly)) {
		if (bend != this->currentPitchbend[midiChannel]) {
			bend += 8192;
			uint8_t msb = (bend >> 7) & 0x7F;
			uint8_t lsb = bend & 0x7F;
			this->writeCommand(0xE0 | midiChannel);
			this->midi
				<< u8(lsb)
				<< u8(msb)
				<< u8(0) // delay until next event
			;
			this->currentPitchbend[midiChannel] = bend;
		}
	}
	if (ev->instrument != this->currentPatch[midiChannel]) {
		assert(ev->instrument < MIDI_PATCHES);
		// Instrument has changed
		this->writeCommand(0xC0 | midiChannel);
		this->midi
			<< u8(ev->instrument)
			<< u8(0) // delay until next event
		;
		this->currentPatch[midiChannel] = ev->instrument;
	}

	// Use 64 as the default velocity, otherwise squish it into a 7-bit value.
	uint8_t velocity = (ev->velocity == 0) ? 64 : (ev->velocity >> 1);

	assert(midiChannel < 16);
	assert(note < 0x80);
	assert(velocity < 0x80);
	this->writeCommand(0x90 | midiChannel);
	this->midi
		<< u8(note)
		<< u8(velocity)
	;

	this->activeNote[ev->channel] = note;

	// Flag this channel as used, to be written into the CMF header later
	this->channelsInUse[midiChannel] = 1;

	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}
*/
