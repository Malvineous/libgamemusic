/**
 * @file   mus-cmf-creativelabs.cpp
 * @brief  MusicReader and MusicWriter classes for Creative Labs' CMF files.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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
#include "mus-cmf-creativelabs.hpp"

using namespace camoto::gamemusic;

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

E_CERTAINTY MusicType_CMF::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	// Make sure the signature matches
	// TESTED BY: mus_cmf_creativelabs_isinstance_c01
	char sig[4];
	psMusic->seekg(0, std::ios::beg);
	psMusic->read(sig, 4);
	if (strncmp(sig, "CTMF", 4) != 0) return EC_DEFINITELY_NO;

	// Make sure the header says it's version 1.0 or 1.1
	// TESTED BY: mus_cmf_creativelabs_isinstance_c02 (wrong ver)
	// TESTED BY: mus_cmf_creativelabs_isinstance_c03 (1.0)
	uint16_t ver;
	psMusic >> u16le(ver);
	if ((ver != 0x100) && (ver != 0x101)) return EC_DEFINITELY_NO;

	// TESTED BY: mus_cmf_creativelabs_isinstance_c00
	return EC_DEFINITELY_YES;
}

MusicWriterPtr MusicType_CMF::create(ostream_sptr output, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_CMF(output));
}

MusicReaderPtr MusicType_CMF::open(istream_sptr input, MP_SUPPDATA& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_CMF(input));
}

MP_SUPPLIST MusicType_CMF::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return MP_SUPPLIST();
}


MusicReader_CMF::MusicReader_CMF(istream_sptr input)
	throw (std::ios::failure) :
		MusicReader_GenericMIDI(MIDIFlags::BasicMIDIOnly),
		input(input),
		patches(new OPLPatchBank())
{
	// Skip CTMF header.  This is an absolute seek as it will be by far the most
	// common situation and avoids a lot of complexity because the header includes
	// absolute file offsets, which we thus won't have to adjust.
	this->input->seekg(4, std::ios::beg); // skip CTMF header

	uint16_t ver, offInst, ticksPerQuarter, ticksPerSecond;
	uint16_t offTitle, offComposer, offRemarks;
	this->input
		>> u16le(ver)
		>> u16le(offInst)
		>> u16le(this->offMusic)
		>> u16le(ticksPerQuarter)
		>> u16le(ticksPerSecond)
		>> u16le(offTitle)
		>> u16le(offComposer)
		>> u16le(offRemarks)
	;

	this->setTicksPerQuarterNote(ticksPerQuarter);
	this->setusPerQuarterNote(ticksPerQuarter * 1000000 / ticksPerSecond);

	// Skip channel-in-use table as we don't need it
	this->input->seekg(16, std::ios::cur);

	// Rest of header depends on file version
	uint16_t numInstruments;
	switch (ver) {
		case 0x100: {
			uint8_t temp;
			this->input
				>> u8(temp)
			;
			numInstruments = temp;
			break;
		}
		default: // do this so you can force-open an unknown version
			std::cerr << "Warning: Unknown CMF version " << (int)(ver >> 8) << '.'
				<< (int)(ver & 0xFF) << ", proceeding as if v1.1" << std::endl;
		case 0x101:
			this->input
				>> u16le(numInstruments)
			;
			// Skip uint16le tempo value (unknown use)
			this->input->seekg(2, std::ios::cur);
			break;
	}

	// Read the instruments
	this->patches->setPatchCount(numInstruments);
	this->input->seekg(offInst, std::ios::beg);
	for (int i = 0; i < numInstruments; i++) {
		OPLPatchPtr patch(new OPLPatch());
		uint8_t inst[16];
		this->input->read((char *)inst, 16);

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
		this->patches->setPatch(i, patch);

		this->patchMap[i] = i;
	}

	/// @todo Scan MIDI data and get a list of all used instruments, and supply
	/// CMF defaults to any numbers beyond the supplied bank.

	// Pass the MIDI data on to the parent
	this->input->seekg(this->offMusic, std::ios::beg);
	this->setMIDIStream(input);
}

MusicReader_CMF::~MusicReader_CMF()
	throw ()
{
}

PatchBankPtr MusicReader_CMF::getPatchBank()
	throw ()
{
	return this->patches;
}

void MusicReader_CMF::rewind()
	throw ()
{
	this->input->clear(); // clear any errors (e.g. EOF)
	this->input->seekg(this->offMusic, std::ios::beg);
	return;
}


MusicWriter_CMF::MusicWriter_CMF(ostream_sptr output)
	throw (std::ios::failure) :
		MusicWriter_GenericMIDI(MIDIFlags::BasicMIDIOnly),
		output(output)
{
	memset(this->channelsInUse, 0, sizeof(this->channelsInUse));
}

MusicWriter_CMF::~MusicWriter_CMF()
	throw ()
{
	// If this assertion fails it means the user forgot to call finish()
//	assert(this->firstPair == true);
}

void MusicWriter_CMF::setPatchBank(const PatchBankPtr& instruments)
	throw (EBadPatchType)
{
	this->patches = boost::dynamic_pointer_cast<OPLPatchBank>(instruments);
	if (!this->patches) {
		// Patch bank isn't an OPL one, see if it can be converted into an
		// OPLPatchBank.  May throw EBadPatchType.
		this->patches = OPLPatchBankPtr(new OPLPatchBank(*instruments));
	}
	if (this->patches->getPatchCount() >= MIDI_PATCHES) {
		throw EBadPatchType("CMF files have a maximum of 128 instruments.");
	}
	return;
}

void MusicWriter_CMF::start()
	throw (std::ios::failure)
{
	this->output->write(
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
	uint16_t numInstruments = this->patches->getPatchCount();
	offNext += 16 * numInstruments;
	uint16_t offMusic = offNext;

	this->output
		<< u16le(offInst)
		<< u16le(offMusic)
	;
	this->output->write(
		"\x00\x00" // ticks per quarter note
		"\x00\x00" // ticks per second
	, 4);

	// Write offset of title, composer and remarks
	for (int i = 0; i < 3; i++) {
		this->output << u16le(offText[i]);
	}

	// Temporary channel-in-use table
	for (int i = 0; i < 16; i++) this->output << u8(1);

	this->output
		<< u16le(numInstruments)
		<< u16le(0) // TODO: default value for 'basic tempo'
	;

	/// @todo Write title, composer and remarks strings here (null terminated)

	for (int i = 0; i < numInstruments; i++) {
		uint8_t inst[16];
		OPLPatchPtr patch = this->patches->getTypedPatch(i);
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

		this->output->write((char *)inst, 16);
	}

	this->setMIDIStream(output);
	return;
}

void MusicWriter_CMF::finish()
	throw (std::ios::failure)
{
	this->MusicWriter_GenericMIDI::finish();

	uint16_t ticksPerQuarter = this->getTicksPerQuarterNote();
	unsigned long ticksPerus = this->getusPerQuarterNote() / ticksPerQuarter;
	uint16_t ticksPerSecond = 1000000 / ticksPerus;
	this->output->seekp(10, std::ios::beg);
	this->output
		<< u16le(ticksPerQuarter)
		<< u16le(ticksPerSecond)
	;

	this->output->seekp(20, std::ios::beg);
	this->output->write((char *)this->channelsInUse, 16);
	return;
}

void MusicWriter_CMF::handleEvent(NoteOnEvent *ev)
	throw (std::ios::failure, EChannelMismatch)
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
