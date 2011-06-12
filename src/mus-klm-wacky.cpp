/**
 * @file   mus-klm-wacky.cpp
 * @brief  MusicReader and MusicWriter classes for Creative Labs' KLM files.
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
#include <camoto/gamemusic/mus-generic-opl.hpp>
#include "mus-klm-wacky.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Adlib -> Hz conversion factor to use
#define KLM_FNUM_CONVERSION  49716

std::string MusicType_KLM::getCode() const
	throw ()
{
	return "klm-wacky";
}

std::string MusicType_KLM::getFriendlyName() const
	throw ()
{
	return "Wacky Wheels Adlib Music File";
}

std::vector<std::string> MusicType_KLM::getFileExtensions() const
	throw ()
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("klm");
	return vcExtensions;
}

MusicType::Certainty MusicType_KLM::isInstance(istream_sptr psMusic) const
	throw (std::ios::failure)
{
	psMusic->seekg(0, std::ios::end);
	io::stream_offset lenFile = psMusic->tellg();

	psMusic->seekg(0, std::ios::end);
	uint16_t offMusic;
	psMusic->seekg(3, std::ios::beg);
	psMusic >> u16le(offMusic);

	// Make sure the instruments are of the correct length
	// TESTED BY: mus_klm_wacky_isinstance_c01
	if ((offMusic - 5) % 11 != 0) return MusicType::DefinitelyNo;

	// Make sure the song data offset is contained within the file
	// TESTED BY: mus_klm_wacky_isinstance_c02
	if (offMusic >= lenFile) return MusicType::DefinitelyNo;

	// Read each instrument and pick out invalid values
	int instRead = 5;
	while (instRead < offMusic) {
		assert(lenFile > 11);  // if this fails, the logic above is wrong
		uint8_t instrument[11];
		psMusic->read((char *)instrument, 11);
		instRead += 11;

		// Upper five bits of base register 0xE0 are not used
		// TESTED BY: mus_klm_wacky_isinstance_c06
		if (instrument[8] & 0xF8) return MusicType::DefinitelyNo;
		// TESTED BY: mus_klm_wacky_isinstance_c07
		if (instrument[9] & 0xF8) return MusicType::DefinitelyNo;

		// Upper two bits of base register 0xC0 are not used
		// TESTED BY: mus_klm_wacky_isinstance_c08
		if (instrument[10] & 0xC0) return MusicType::DefinitelyNo;

		// If we're here, no invalid bits were set
		// TESTED BY: mus_klm_wacky_isinstance_c09
	}

	// This should always work unless there was an obscure I/O error
	assert(psMusic->tellg() == offMusic);
	lenFile -= instRead;

	// Skim-read data until EOF
	while (lenFile) {
		uint8_t code;
		uint8_t lenEvent;
		psMusic >> u8(code);
		lenFile--;
		switch (code & 0xF0) {
			case 0x00: lenEvent = 0; break;
			case 0x10: lenEvent = 2; break;
			case 0x20: lenEvent = 1; break;
			case 0x30: lenEvent = 1; break;
			case 0x40: lenEvent = 0; break;
			case 0xF0:
				switch (code) {
					case 0xFD: lenEvent = 1; break;
					case 0xFF: lenEvent = 0; break;
					default:
						// Invalid 0xF0 event type
						// TESTED BY: mus_klm_wacky_isinstance_c03
						return MusicType::DefinitelyNo;
				}
				break;
			default:
				// Invalid event type
				// TESTED BY: mus_klm_wacky_isinstance_c04
				return MusicType::DefinitelyNo;
		}
		// Truncated event
		// TESTED BY: mus_klm_wacky_isinstance_c05
		if (lenFile < lenEvent) return MusicType::DefinitelyNo;
		if (code == 0xFF) break;
		psMusic->seekg(lenEvent, std::ios::cur);
		lenFile -= lenEvent;
	}

	// TESTED BY: mus_klm_wacky_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicWriterPtr MusicType_KLM::create(ostream_sptr output, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicWriterPtr(new MusicWriter_KLM(output));
}

MusicReaderPtr MusicType_KLM::open(istream_sptr input, SuppData& suppData) const
	throw (std::ios::failure)
{
	return MusicReaderPtr(new MusicReader_KLM(input));
}

SuppFilenames MusicType_KLM::getRequiredSupps(const std::string& filenameMusic) const
	throw ()
{
	// No supplemental types/empty list
	return SuppFilenames();
}


MusicReader_KLM::MusicReader_KLM(istream_sptr input)
	throw (std::ios::failure) :
	//MusicReader_GenericMIDI(MIDIFlags::BasicMIDIOnly),
		input(input),
		patches(new OPLPatchBank()),
		setTempo(false),
		setRhythm(false),
		setWaveSel(false),
		tick(0)
{
	memset(this->patchMap, 0xFF, sizeof(this->patchMap));
	memset(this->freqMap, 0, sizeof(this->freqMap));
	memset(this->volMap, 0xFF, sizeof(this->volMap));
	memset(this->noteOn, 0, sizeof(this->noteOn));

	this->freqMap[12] = 142;
	this->freqMap[13] = 142;
	this->input->seekg(0, std::ios::beg);

	uint8_t unk1;
	uint16_t offSong;
	this->input
		>> u16le(this->tempo)
		>> u8(unk1)
		>> u16le(offSong)
	;

	// Read the instruments
	int numInstruments = (offSong - 5) / 11;
	this->patches->setPatchCount(numInstruments);
	for (int i = 0; i < numInstruments; i++) {
		OPLPatchPtr patch(new OPLPatch());
		uint8_t inst[11];
		this->input->read((char *)inst, 11);

		OPLOperator *o = &patch->m;
/*
off cmf klmoff
0   20  6
1   23   7
2   40  0
3   43   1
4   60  2
5   63   3
6   80  4
7   83   5
8   E0  8
9   E3   9
10  C0  10

off klm
0   40
1   43
2   60
3   63
4   80
5   83
6   20
7   23
8   E0
9   E3
10  C0
 */
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = (inst[6 + op] >> 7) & 1;
			o->enableVibrato = (inst[6 + op] >> 6) & 1;
			o->enableSustain = (inst[6 + op] >> 5) & 1;
			o->enableKSR     = (inst[6 + op] >> 4) & 1;
			o->freqMult      =  inst[6 + op] & 0x0F;
			o->scaleLevel    =  inst[0 + op] >> 6;
			o->outputLevel   =  inst[0 + op] & 0x3F;
			o->attackRate    =  inst[2 + op] >> 4;
			o->decayRate     =  inst[2 + op] & 0x0F;
			o->sustainRate   =  inst[4 + op] >> 4;
			o->releaseRate   =  inst[4 + op] & 0x0F;
			o->waveSelect    =  inst[8 + op] & 0x07;
			o = &patch->c;
		}
		patch->feedback    = (inst[10] >> 1) & 0x07;
		patch->connection  =  inst[10] & 1;
		// These next two affect the entire synth (all instruments/channels)
		patch->deepTremolo = false;
		patch->deepVibrato = false;

		patch->rhythm      = 0;

///TEMP
		switch (i) {
			case 7: // snare->hihat instrument
//				patch->c = patch->m;
//				patch->rhythm = 4;
				//memset(&patch->m, 0, sizeof(patch->m));
				//memset(&patch->c, 0, sizeof(patch->c));
				break;
/*			case 6:
				memset(&patch->m, 0, sizeof(patch->m));
				memset(&patch->c, 0, sizeof(patch->c));
				patch->rhythm = 1;
				break;*/
		}
///ENDTEMP
		this->patches->setPatch(i, patch);
	}

	// Make sure we've reached the start of the song data
	assert(this->input->tellg() == offSong);
}

MusicReader_KLM::~MusicReader_KLM()
	throw ()
{
}

PatchBankPtr MusicReader_KLM::getPatchBank()
	throw ()
{
	return this->patches;
}

EventPtr MusicReader_KLM::readNextEvent()
	throw (std::ios::failure)
{
	EventPtr gev;

	if (!this->setTempo) {
		TempoEvent *ev = new TempoEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->usPerTick = 1000000 / this->tempo;
		this->setTempo = true;
		return gev;
	}

	if (!this->setRhythm) {
		ConfigurationEvent *ev = new ConfigurationEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->configType = ConfigurationEvent::EnableRhythm;
		ev->value = 1;
		this->setRhythm = true;
		return gev;
	}

	if (!this->setWaveSel) {
		ConfigurationEvent *ev = new ConfigurationEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->configType = ConfigurationEvent::EnableWaveSel;
		ev->value = 1;
		this->setWaveSel = true;
		return gev;
	}

	this->input->exceptions(std::ios::badbit);
	while (!gev && !this->input->eof()) {
		uint8_t code;
		this->input >> u8(code);
		uint8_t channel = code & 0x0F;
//std::cout << "Got event 0x" << std::hex << (int)code << "\n";
		if ((code < 0xF0) && (channel >= KLM_CHANNEL_COUNT)) throw std::ios::failure("Channel too large");
//		if ((code < 0xF0) && (channel == 6)) throw std::ios::failure("Invalid channel");

		// Convert "KLM" channels into libgamemusic ones (order of rhythm percussion
		// is opposite.)  Note that KLM treats the bass drum as a normal instrument
		// (on KLM channel 0x06, i.e. with frequency bytes) whereas it still needs
		// to be moved to libgamemusic channel 10 to trigger the right OPL keyon bit.
		// 6 -> 10, 7 -> 9 ... 10 -> 6
		//if (channel > 5) channel = 10 - (channel - 6);
		//if (channel > 5) channel = 9+ (channel - 6);
		switch (channel) {
			case 6: channel = 10; break;
			case 7: channel = 12; break;//9
			case 8: channel = 11; break;
			case 9: channel = 13; break;
			case 10: channel = 9; break;//12
		}

		switch (code & 0xF0) {
			case 0x00: { // note off
				NoteOffEvent *ev = new NoteOffEvent();
				gev.reset(ev);
				ev->channel = channel;
				this->noteOn[channel] = false;
				break;
			}
			case 0x10: { // set frequency
				uint8_t a0, b0;
				bool doNoteOn = true;
				if (code < 0x17) { // TODO: Should be chan six, but needs to be 7?
					this->input >> u8(a0) >> u8(b0);
					int fnum = ((b0 & 0x03) << 8) | a0;
					int block = (b0 >> 2) & 0x07;
					this->freqMap[channel] = ::fnumToMilliHertz(fnum, block, KLM_FNUM_CONVERSION);
					doNoteOn = b0 & 0x20; // keyon bit
				} else std::cout << "note on on chan " << (int)code - 0x10 << "\n";
				if (doNoteOn) {
					if (this->noteOn[channel]) {
						// Note is already playing, do a pitchbend
						PitchbendEvent *ev = new PitchbendEvent();
						gev.reset(ev);
						ev->channel = channel;
						ev->milliHertz = this->freqMap[channel];
					} else {
						// Note is not playing, do a note-on
						NoteOnEvent *ev = new NoteOnEvent();
						gev.reset(ev);
						ev->channel = channel;
						ev->instrument = this->patchMap[channel];
						ev->milliHertz = this->freqMap[channel];
						ev->velocity = this->volMap[channel];
						this->noteOn[channel] = true;
					}
				} else {
					NoteOffEvent *ev = new NoteOffEvent();
					gev.reset(ev);
					ev->channel = channel;
					this->noteOn[channel] = false;
					break;
				}
				break;
			}
			case 0x20: { // set volume
				this->input >> u8(this->volMap[channel]);
				if (this->volMap[channel] > 127) throw std::ios::failure("Velocity out of range");
				this->volMap[channel] *= 2;
				this->volMap[channel] += 4;
// TODO: Figure out which operator this affects on the percussive channels
				break;
			}
			case 0x30: { // set instrument
				uint8_t data;
				this->input >> u8(data);
				this->patchMap[channel] = data;
				break;
			}
			case 0x40: { // note on
				NoteOnEvent *ev = new NoteOnEvent();
				gev.reset(ev);
				ev->channel = channel;
				ev->instrument = this->patchMap[channel];
				ev->milliHertz = this->freqMap[channel];
				ev->velocity = this->volMap[channel];
				this->noteOn[channel] = true;
				break;
			}
			case 0xF0: {
				switch (code) {
					case 0xFD: // normal delay
						uint8_t data;
						this->input >> u8(data);
						this->tick += data;
						break;
					case 0xFF: // end of song
						return EventPtr();
					default:
						std::cerr << "Invalid delay event type 0x" << std::hex << (int)code
							<< std::dec << std::endl;
						throw std::ios::failure("Invalid delay event type");
						break;
				}
				break;
			}
			default:
				std::cerr << "Invalid event type 0x" << std::hex << (int)code
					<< std::dec << std::endl;
				throw std::ios::failure("Invalid event type");
		}
	}

	if (gev) {
		gev->absTime = this->tick;
	}

	return gev;
}


MusicWriter_KLM::MusicWriter_KLM(ostream_sptr output)
	throw (std::ios::failure) :
		lastTick(0),
	//MusicWriter_GenericMIDI(MIDIFlags::BasicMIDIOnly),
		output(output)
{
	memset(this->patchMap, 0xFF, sizeof(this->patchMap));
}

MusicWriter_KLM::~MusicWriter_KLM()
	throw ()
{
	// If this assertion fails it means the user forgot to call finish()
//	assert(this->firstPair == true);
}

void MusicWriter_KLM::setPatchBank(const PatchBankPtr& instruments)
	throw (EBadPatchType)
{
	this->patches = boost::dynamic_pointer_cast<OPLPatchBank>(instruments);
	if (!this->patches) {
		// Patch bank isn't an OPL one, see if it can be converted into an
		// OPLPatchBank.  May throw EBadPatchType.
		this->patches = OPLPatchBankPtr(new OPLPatchBank(*instruments));
	}
	if (this->patches->getPatchCount() > 256) {
		throw EBadPatchType("KLM files have a maximum of 256 instruments.");
	}
	return;
}

void MusicWriter_KLM::start()
	throw (std::ios::failure)
{
	uint16_t musOffset = 5 + this->patches->getPatchCount() * 11;
	this->output
		<< u16le(0) // tempo placeholder
		<< u8(1)    // unknown
		<< u16le(musOffset)
	;
	for (int i = 0; i < this->patches->getPatchCount(); i++) {
		uint8_t inst[11];
		OPLPatchPtr patch = this->patches->getTypedPatch(i);
		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			inst[6 + op] =
				((o->enableTremolo & 1) << 7) |
				((o->enableVibrato & 1) << 6) |
				((o->enableSustain & 1) << 5) |
				((o->enableKSR     & 1) << 4) |
				 (o->freqMult      & 0x0F)
			;
			inst[0 + op] = (o->scaleLevel << 6) | (o->outputLevel & 0x3F);
			inst[2 + op] = (o->attackRate << 4) | (o->decayRate & 0x0F);
			inst[4 + op] = (o->sustainRate << 4) | (o->releaseRate & 0x0F);
			inst[8 + op] =  o->waveSelect & 7;

			o = &patch->c;
		}
		inst[10] = ((patch->feedback & 7) << 1) | (patch->connection & 1);
		//inst[11] = inst[12] = inst[13] = inst[14] = inst[15] = 0;

		/// @todo handle these
		//patch->deepTremolo = false;
		//patch->deepVibrato = false;

		this->output->write((char *)inst, 11);
	}
/*
off cmf klmoff
0   20  6
1   23   7
2   40  0
3   43   1
4   60  2
5   63   3
6   80  4
7   83   5
8   E0  8
9   E3   9
10  C0  10

off klm
0   40
1   43
2   60
3   63
4   80
5   83
6   20
7   23
8   E0
9   E3
10  C0
 */

	return;
}

void MusicWriter_KLM::finish()
	throw (std::ios::failure)
{
	this->output << u8(0xFF); // end of song
	return;
}

void MusicWriter_KLM::handleEvent(TempoEvent *ev)
	throw (std::ios::failure)
{
	// TODO: Set current multiplier for future delay modifications
	if (ev->absTime == 0) {
		int ticksPerSecond = 1000000 / ev->usPerTick;
		// First tempo event, update the header
		unsigned int pos = this->output->tellp(); // should nearly always be musOffset from the header
		this->output->seekp(0, std::ios::beg);
		this->output << u16le(ticksPerSecond);
		this->output->seekp(pos, std::ios::beg);
		//this->tempoMultiplier = 1;
	} else {
		// Tempo has changed, but since this format doesn't support tempo changes
		// we will have to modify all future delay values.
		// TODO
	}
	
/*
	assert(ev->usPerTick > 0);
	unsigned long n = ev->usPerTick * this->ticksPerQuarterNote;
	if (this->usPerQuarterNote != n) {
		if (!(this->midiFlags & MIDIFlags::BasicMIDIOnly)) {
			this->writeMIDINumber(ev->absTime - this->lastTick);
			this->midi->write("\xFF\x51\x03", 3);
			this->midi
				<< u8(this->usPerQuarterNote >> 16)
				<< u8(this->usPerQuarterNote >> 8)
				<< u8(this->usPerQuarterNote & 0xFF)
			;
		}
		this->usPerQuarterNote = n;
	}
*/
	return;
}

void MusicWriter_KLM::handleEvent(NoteOnEvent *ev)
	throw (std::ios::failure, EChannelMismatch)
{
	// Make sure a patch bank has been set
	assert(this->patches);

	if (ev->channel >= KLM_CHANNEL_COUNT) throw std::ios::failure("Too many channels");

	this->writeDelay(ev->absTime);

	uint8_t event;

	if (ev->instrument != this->patchMap[ev->channel]) {
		// Change the instrument first
		event = 0x30 | ev->channel;
		this->output
			<< u8(event)
			<< u8(ev->channel)
		;
	}

	unsigned int fnum, block;
	::milliHertzToFnum(ev->milliHertz, &fnum, &block, KLM_FNUM_CONVERSION);

	event = 0x10 | ev->channel;
	uint8_t a0 = fnum & 0xFF;
	uint8_t b0 =
		OPLBIT_KEYON  // keyon enabled
		| (block << 2) // and the octave
		| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
	;

	// Write lower eight bits of note freq
	this->output
		<< u8(event)
		<< u8(a0)
		<< u8(b0)
	;

/*
	uint32_t delay = ev->absTime - this->lastTick;
	this->writeMIDINumber(delay);

	if (this->updateDeep) {
		// Need to set KLM controller 0x63
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

	// Flag this channel as used, to be written into the KLM header later
	this->channelsInUse[midiChannel] = 1;

	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
*/
	return;
}

void MusicWriter_KLM::handleEvent(NoteOffEvent *ev)
	throw (std::ios::failure)
{
	if (ev->channel >= KLM_CHANNEL_COUNT) throw std::ios::failure("Too many channels");

	this->writeDelay(ev->absTime);

	uint8_t code = 0x00 | ev->channel;
	this->output << u8(code);

	return;
}

void MusicWriter_KLM::handleEvent(PitchbendEvent *ev)
	throw (std::ios::failure)
{
/*
	if (this->flags & IntegerNotesOnly) return;

	uint8_t midiChannel = this->getMIDIchannel(ev->channel, MIDI_CHANNELS);

	uint8_t note;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &note, &bend, this->activeNote[ev->channel]);
	/// @todo Handle pitch bending further than one note or if 'note' comes out
	/// differently to the currently playing note
	if (bend != this->currentPitchbend[midiChannel]) {
		uint32_t delay = ev->absTime - this->lastTick;
		this->writeMIDINumber(delay);
		bend += 8192;
		uint8_t msb = (bend >> 7) & 0x7F;
		uint8_t lsb = bend & 0x7F;
		this->writeCommand(0xE0 | midiChannel);
		this->midi
			<< u8(lsb)
			<< u8(msb)
		;
		this->currentPitchbend[midiChannel] = bend;
	}
*/
	this->lastTick = ev->absTime;
	//this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void MusicWriter_KLM::handleEvent(ConfigurationEvent *ev)
	throw (std::ios::failure)
{
	switch (ev->configType) {
		case ConfigurationEvent::EnableRhythm:
			// Ignored, will set when rhythm channels are played
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			if (ev->value != 1) std::cerr << "Deep tremolo cannot be disabled in this format, ignoring event.";
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			if (ev->value != 1) std::cerr << "Deep vibrato cannot be disabled in this format, ignoring event.";
			break;
		default:
			break;
	}
	//this->lastTick = ev->absTime;
	return;
}

void MusicWriter_KLM::writeDelay(unsigned long curTick)
	throw (std::ios::failure)
{
	uint32_t delay = curTick - this->lastTick;
	while (delay) {
		uint8_t d;
		if (delay > 255) d = 255; else d = delay;
		this->output << u8(0xFD) << u8(delay);
		delay -= d;
	}

	this->lastTick = curTick;
	return;
}
