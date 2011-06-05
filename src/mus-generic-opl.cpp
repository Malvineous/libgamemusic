/**
 * @file   mus-generic-opl.cpp
 * @brief  MusicReader and MusicWriter classes to process raw OPL register data.
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

#include <math.h> // pow
#include <boost/pointer_cast.hpp>
#include <camoto/gamemusic/patchbank-singletype.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/mus-generic-opl.hpp>

// OPL register offsets
#define BASE_CHAR_MULT  0x20
#define BASE_SCAL_LEVL  0x40
#define BASE_ATCK_DCAY  0x60
#define BASE_SUST_RLSE  0x80
#define BASE_FNUM_L     0xA0
#define BASE_KEYON_FREQ 0xB0
#define BASE_RHYTHM     0xBD
#define BASE_WAVE       0xE0
#define BASE_FEED_CONN  0xC0

#define OPLBIT_KEYON    0x20 // Bit in BASE_KEYON_FREQ register for turning a note on

/// Supplied with a channel, return the offset from a base OPL register for the
/// Modulator cell (e.g. channel 4's modulator is at offset 0x09.  Since 0x60 is
/// the attack/decay function, register 0x69 will thus set the attack/decay for
/// channel 4's modulator.)  Channels go from 0 to 8 inclusive.
#define OPLOFFSET_MOD(channel)   (((channel) / 3) * 8 + ((channel) % 3))
#define OPLOFFSET_CAR(channel)   (OPLOFFSET_MOD(channel) + 3)

/// Supplied with an operator offset, return the OPL channel it is associated
/// with (0-8).  Note that this only works in 2-operator mode, the OPL3's 4-op
/// mode requires a different formula.
#define OPL_OFF2CHANNEL(off)   (((off) % 8 % 3) + ((off) / 8 * 3))

/// Convert the OPLPatch::rhythm value into text for error messages.
const char *rhythmToText(int rhythm)
{
	switch (rhythm) {
		case 0: return "normal (non-rhythm) instrument";
		case 1: return "hi-hat";
		case 2: return "top cymbal";
		case 3: return "tom tom";
		case 4: return "snare drum";
		case 5: return "bass drum";
		default: return "[unknown instrument type]";
	}
}

using namespace camoto::gamemusic;

#include <iomanip>
std::ostream& operator << (std::ostream& s, const OPLPatchPtr p)
{
	s << *(p.get());
	return s;
}

std::ostream& operator << (std::ostream& s, const OPLPatch& p)
{
	s << "[" << std::hex
		<< (int)p.rhythm << ":"
		<< (int)p.feedback << '.'
		<< (p.connection ? 'C' : 'c')
		<< (p.deepTremolo ? 'T' : 't')
		<< (p.deepVibrato ? 'V' : 'v') << '/'

		<< (p.c.enableTremolo ? 'T' : 't')
		<< (p.c.enableVibrato ? 'V' : 'v')
		<< (p.c.enableSustain ? 'S' : 's')
		<< (p.c.enableKSR ? 'K' : 'k') << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.freqMult << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.scaleLevel << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.c.outputLevel << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.attackRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.c.decayRate << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.sustainRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.c.releaseRate << '.'
		<< (int)p.c.waveSelect << '/'

		<< (p.m.enableTremolo ? 'T' : 't')
		<< (p.m.enableVibrato ? 'V' : 'v')
		<< (p.m.enableSustain ? 'S' : 's')
		<< (p.m.enableKSR ? 'K' : 'k') << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.freqMult << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.scaleLevel << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.m.outputLevel << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.attackRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.m.decayRate << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.sustainRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.m.releaseRate << '.'
		<< (int)p.m.waveSelect

		<< ']' << std::dec
	;
	return s;
}


MusicReader_GenericOPL::MusicReader_GenericOPL(DelayType delayType)
	throw () :
		delayType(delayType),
		lastTick(0),
		patches(new OPLPatchBank()),
		fnumConversion(49716) // TODO: or 50000 - make user settable
{
	assert(OPLOFFSET_MOD(1-1) == 0x00);
	assert(OPLOFFSET_MOD(5-1) == 0x09);
	assert(OPLOFFSET_MOD(9-1) == 0x12);
	assert(OPLOFFSET_CAR(1-1) == 0x03);
	assert(OPLOFFSET_CAR(5-1) == 0x0C);
	assert(OPLOFFSET_CAR(9-1) == 0x15);
	assert(OPL_OFF2CHANNEL(0x00) == 1-1);
	assert(OPL_OFF2CHANNEL(0x09) == 5-1);
	assert(OPL_OFF2CHANNEL(0x12) == 9-1);
	assert(OPL_OFF2CHANNEL(0x03) == 1-1);
	assert(OPL_OFF2CHANNEL(0x0C) == 5-1);
	assert(OPL_OFF2CHANNEL(0x15) == 9-1);
	memset(this->oplState, 0, sizeof(this->oplState));
}

MusicReader_GenericOPL::~MusicReader_GenericOPL()
	throw ()
{
}

PatchBankPtr MusicReader_GenericOPL::getPatchBank()
	throw ()
{
	if (this->patches->getPatchCount() == 0) {
		int numPatches = 0;

		this->rewind();
		memset(this->oplState, 0, sizeof(this->oplState));
		uint32_t delay;
		uint8_t reg, val;
		uint8_t chipIndex;
		while (this->nextPair(&delay, &chipIndex, &reg, &val)) {
			assert(chipIndex < 2);
			this->oplState[chipIndex][reg] = val;

			if ((reg & 0xB0) == 0xB0) {
				int oplChannel = reg & 0x0F;
				if (reg == 0xBD) {
					if ((val & 0x20) && (val & 0x1F)) {
						// Percussion mode is enabled, and a percussion instrument is sounding
						if (val & 0x01) { //&& (!noteOn[chipIndex][9])) {
							// Hi-hat is playing or about to play
							oplChannel = 7;//mod
							OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
							curPatch->rhythm = 1;
							this->savePatch(curPatch);
						}
						if (val & 0x02) {
							// Top-cymbal is playing or about to play
							oplChannel = 8;//car
							OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
							curPatch->rhythm = 2;
							this->savePatch(curPatch);
						}
						if (val & 0x04) {
							// Tom-tom is playing or about to play
							oplChannel = 8;//mod
							OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
							curPatch->rhythm = 3;
							this->savePatch(curPatch);
						}
						if (val & 0x08) {
							// Snare-drum is playing or about to play
							oplChannel = 7;//car
							OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
							curPatch->rhythm = 4;
							this->savePatch(curPatch);
						}
						if (val & 0x10) {
							// Bass-drum is playing or about to play
							oplChannel = 6;//both
							OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
							curPatch->rhythm = 5;
							this->savePatch(curPatch);
						}
					}
				} else if (oplChannel < 9) { // valid register/channel
					if (val & OPLBIT_KEYON) {
						// Note is on, get instrument settings of this note
						OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
						this->savePatch(curPatch);
						/*noteOn[chipIndex][oplChannel] = true;
					} else {
						noteOn[chipIndex][oplChannel] = false;*/
					}
				} else {
					std::cerr << "WARNING: Bad OPL register/channel 0x" << std::hex
						<< (int)reg << std::endl;
				}
			}
		}
		this->rewind();
		memset(this->oplState, 0, sizeof(this->oplState));
	}
	return boost::static_pointer_cast<PatchBank>(this->patches);
}


#define bitsChanged(b) ((val ^ this->oplState[chipIndex][reg]) & b)

EventPtr MusicReader_GenericOPL::readNextEvent()
	throw (std::ios::failure)
{
	if (this->eventBuffer.size() == 0) {
		if (!this->populateEventBuffer()) return EventPtr(); // end of song
	}
	EventPtr next = this->eventBuffer.front();
	this->eventBuffer.pop_front();
	return next;
}

bool MusicReader_GenericOPL::populateEventBuffer()
	throw (std::ios::failure)
{
	// Process some more bytes until we end up with an event
	uint32_t delay;
	uint8_t reg, val;
	uint8_t chipIndex;

	// Have we added at least one event to the buffer?
	bool addedEvent = false;

	do {
		if (!this->nextPair(&delay, &chipIndex, &reg, &val)) break; // end of song
		assert(chipIndex < 2);
		if (this->delayType == DelayIsPreData) this->lastTick += delay;

		uint8_t oplChannel = reg & 0x0F; // Only for regs 0xA0, 0xB0 and 0xC0!
		uint8_t channel = oplChannel + 14 * chipIndex;
		switch (reg & 0xF0) {
			case 0x00:
				if (reg == 0x01) {
					if (bitsChanged(0x20)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableWaveSel;
						ev->value = (val & 0x20) ? 1 : 0;
						this->eventBuffer.push_back(gev);
						addedEvent = true;
					}
				} else if (reg == 0x05) {
					if (bitsChanged(0x01)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableOPL3;
						ev->value = (val & 0x01) ? 1 : 0;
						this->eventBuffer.push_back(gev);
						addedEvent = true;
					}
				}
				break;
			case 0xA0:
				if (bitsChanged(0xFF) && (this->oplState[chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON)) {
					// The pitch has changed while a note was playing
					this->createOrUpdatePitchbend(chipIndex, channel,
						val, this->oplState[chipIndex][0xB0 | oplChannel]);
					addedEvent = true;
				}
				break;
			case 0xB0:
				if (reg == 0xBD) {
					// Percussion/rhythm mode
					if (val & 0x20) {
						// Rhythm mode enabled
						for (int p = 0; p < 5; p++) {
							int keyonBit = 1 << p;
							// If rhythm mode has just been enabled and this instrument's
							// keyon bit is set, OR rhythm mode was always on and this
							// instrument's keyon bit has changed, write out a note-on or
							// note-off event as appropriate.
							if ((bitsChanged(0x20) && (val & keyonBit)) || bitsChanged(keyonBit)) {
								// Hi-hat is playing or about to play
								channel = 14 * chipIndex + 9 + p;
								switch (p) {
									case 0: oplChannel = 7; break; // mod
									case 1: oplChannel = 8; break; // car
									case 2: oplChannel = 8; break; // mod
									case 3: oplChannel = 7; break; // car
									case 4: oplChannel = 6; break; // both
								}
								EventPtr gev;
								if (val & keyonBit) {
									gev = this->createNoteOn(chipIndex, oplChannel, p + 1,
										channel, this->oplState[chipIndex][0xB0 | oplChannel]);
								} else {
									gev = this->createNoteOff(channel);
								}
								this->eventBuffer.push_back(gev);
								addedEvent = true;
							}
						}
					} else if (bitsChanged(0x20)) { // Rhythm mode just got disabled
						// Turn off any currently playing rhythm instruments
						for (int p = 0; p < 5; p++) {
							if (this->oplState[chipIndex][0xBD] & (1 << p)) {
								EventPtr gev = this->createNoteOff(9 + p);
								this->eventBuffer.push_back(gev);
								addedEvent = true;
							}
						}
					}
					if (bitsChanged(0x80)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableDeepTremolo;
						ev->value = (val & 0x80) ? 1 : 0; // bit0 is enable/disable
						if (chipIndex) ev->value |= 2;    // bit1 is chip index
						this->eventBuffer.push_back(gev);
						addedEvent = true;
					}
					if (bitsChanged(0x40)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableDeepVibrato;
						ev->value = (val & 0x40) ? 1 : 0; // bit0 is enable/disable
						if (chipIndex) ev->value |= 2;    // bit1 is chip index
						this->eventBuffer.push_back(gev);
						addedEvent = true;
					}
				} else if (bitsChanged(OPLBIT_KEYON)) {
					if (val & OPLBIT_KEYON) {
						EventPtr gev = this->createNoteOn(chipIndex, oplChannel, 0, channel, val);
						this->eventBuffer.push_back(gev);
						addedEvent = true;
					} else {
						EventPtr gev = this->createNoteOff(channel);
						this->eventBuffer.push_back(gev);
						addedEvent = true;
					}
				} else if ((val & OPLBIT_KEYON) && bitsChanged(0x1F)) {
					// The note is already on and the pitch has changed.
					this->createOrUpdatePitchbend(chipIndex, channel,
						this->oplState[chipIndex][0xA0 | oplChannel], val);
					addedEvent = true;
				}

				break;
		}

		if (this->delayType == DelayIsPostData) this->lastTick += delay;

		// Update the current state with the new value
		this->oplState[chipIndex][reg] = val;

	// Keep looping until we've added at least one event and encountered a delay.
	// The delay requirement gives us a chance to combine similar events (like
	// pitchbends) into one Event instance when they occur at the same time.
	} while ((!addedEvent) || (delay == 0));

	// If we're here, we've either added at least one event to the buffer or we've
	// run out of data and the last few register changes didn't result in any
	// events being generated.  So we return addedEvent which will still be false
	// if we didn't add any events, and returning false signals the end of the
	// song.
	return addedEvent;
}

void MusicReader_GenericOPL::changeSpeed(uint32_t usPerTick)
	throw ()
{
	TempoEvent *ev = new TempoEvent();
	EventPtr gev(ev);
	ev->channel = 0; // global event (all channels)
	ev->absTime = this->lastTick;
	ev->usPerTick = usPerTick;
	this->eventBuffer.push_back(gev);
	return;
}

OPLPatchPtr MusicReader_GenericOPL::getCurrentPatch(int chipIndex, int oplChannel)
	throw ()
{
	OPLPatchPtr curPatch(new OPLPatch());

	OPLOperator *o = &curPatch->m;
	uint8_t op = OPLOFFSET_MOD(oplChannel);
	for (int i = 0; i < 2; ++i) {
		o->enableTremolo = (this->oplState[chipIndex][BASE_CHAR_MULT | op] >> 7) & 1;
		o->enableVibrato = (this->oplState[chipIndex][BASE_CHAR_MULT | op] >> 6) & 1;
		o->enableSustain = (this->oplState[chipIndex][BASE_CHAR_MULT | op] >> 5) & 1;
		o->enableKSR     = (this->oplState[chipIndex][BASE_CHAR_MULT | op] >> 4) & 1;
		o->freqMult      =  this->oplState[chipIndex][BASE_CHAR_MULT | op] & 0x0F;
		o->scaleLevel    =  this->oplState[chipIndex][BASE_SCAL_LEVL | op] >> 6;
		o->outputLevel   =  this->oplState[chipIndex][BASE_SCAL_LEVL | op] & 0x3F;
		o->attackRate    =  this->oplState[chipIndex][BASE_ATCK_DCAY | op] >> 4;
		o->decayRate     =  this->oplState[chipIndex][BASE_ATCK_DCAY | op] & 0x0F;
		o->sustainRate   =  this->oplState[chipIndex][BASE_SUST_RLSE | op] >> 4;
		o->releaseRate   =  this->oplState[chipIndex][BASE_SUST_RLSE | op] & 0x0F;
		o->waveSelect    =  this->oplState[chipIndex][BASE_WAVE      | op] & 0x07;
		// Switch to other operator for next loop iteration
		op = OPLOFFSET_CAR(oplChannel);
		o = &curPatch->c;
	}

	curPatch->feedback        = (this->oplState[chipIndex][BASE_FEED_CONN | oplChannel] >> 1) & 0x07;
	curPatch->connection      =  this->oplState[chipIndex][BASE_FEED_CONN | oplChannel] & 1;
	// These next two affect the entire synth (all instruments/channels)
	curPatch->deepTremolo     =  this->oplState[chipIndex][BASE_RHYTHM] >> 7;
	curPatch->deepVibrato     = (this->oplState[chipIndex][BASE_RHYTHM] >> 6) & 1;
	curPatch->rhythm          = 0; // will be overridden later if needed
	return curPatch;
}

void MusicReader_GenericOPL::savePatch(OPLPatchPtr curPatch)
	throw ()
{
	// See if the patch has already been saved
	int numPatches = this->patches->getPatchCount();
	for (int i = 0; i < numPatches; ++i) {
		if (*(this->patches->getTypedPatch(i)) == *curPatch) return; // patch already saved
	}

	// Save the patch
	this->patches->setPatchCount(numPatches + 1);
	this->patches->setPatch(numPatches, curPatch);
	return;
}

EventPtr MusicReader_GenericOPL::createNoteOn(uint8_t chipIndex,
	uint8_t oplChannel, int rhythm, int channel, int b0val
)
	throw ()
{
	NoteOnEvent *ev = new NoteOnEvent();
	EventPtr gev(ev);
	ev->channel = channel;
	ev->absTime = this->lastTick;

	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | this->oplState[chipIndex][0xA0 | oplChannel];
	int block = (b0val >> 2) & 0x07;
	// TODO: Calculate the frequency from the fnum

	ev->milliHertz = this->fnumToMilliHertz(fnum, block);
	// TODO: ignore velocity (or use other operator) for MOD-only rhythm instruments
	ev->velocity = (0x3F - (this->oplState[chipIndex][BASE_SCAL_LEVL | OPLOFFSET_CAR(oplChannel)] & 0x3F)) << 2;

	// Figure out which instrument is being used
	OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
	curPatch->rhythm = rhythm;
	ev->instrument = -1;
	int numPatches = this->patches->getPatchCount();
	if (numPatches == 0) {
		std::cerr << "WARNING: This file tried to play a note without setting any "
			"instrument settings first.  Adding a default instrument." << std::endl;
		this->patches->setPatchCount(numPatches + 1);
		this->patches->setPatch(numPatches, curPatch);
		numPatches++;
	}
	for (int i = 0; i < numPatches; ++i) {
		if (*(this->patches->getTypedPatch(i)) == *curPatch) {
			ev->instrument = i;
			break;
		}
	}
	// This should never fail otherwise instruments aren't getting saved into the
	// PatchBank properly.  Or the user forgot to call getPatchBank() before
	// reading out the events.  Or the file being read is playing notes before
	// setting instruments.
	//assert(ev->instrument >= 0);
	if (ev->instrument < 0) {
		std::cerr << "WARNING: This file tried to play a note without setting an "
			"instrument first.  Picking instrument #0." << std::endl;
		ev->instrument = 0;
	}
	return gev;
}

EventPtr MusicReader_GenericOPL::createNoteOff(int channel)
	throw ()
{
	NoteOffEvent *ev = new NoteOffEvent();
	EventPtr gev(ev);
	ev->channel = channel;
	ev->absTime = this->lastTick;
	return gev;
}

void MusicReader_GenericOPL::createOrUpdatePitchbend(uint8_t chipIndex,
	int channel, int a0val, int b0val)
	throw ()
{
	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | a0val;
	int block = (b0val >> 2) & 0x07;
	int freq = this->fnumToMilliHertz(fnum, block);

	// See if there is already a pitchbend event in the buffer with the
	// same time as this one.  If there is, it will be because the OPL
	// fnum is spread across two registers and we should combine this
	// into a single event.

	// This will only check the previous event, if there's something else
	// (like an instrument effect) in between the two pitch events then
	// they won't be combined into a single pitchbend event.
	bool addNew = true;
	for (std::deque<EventPtr>::reverse_iterator i = this->eventBuffer.rbegin();
		i != this->eventBuffer.rend();
		i++
	) {
		if ((*i)->absTime != this->lastTick) break; // no more events at this time
		PitchbendEvent *pbev = dynamic_cast<PitchbendEvent *>(i->get());
		if (pbev) {
			// There is an existing pitchbend event at the same time, so edit
			// that one.
			pbev->milliHertz = freq;
			addNew = false;
			break;
		}
	}
	if (addNew) {
		// Last event was unrelated, create a new pitchbend event.
		PitchbendEvent *ev = new PitchbendEvent();
		EventPtr gev(ev);
		ev->channel = channel;
		ev->absTime = this->lastTick;
		ev->milliHertz = freq;
		this->eventBuffer.push_back(gev);
	}
	return;
}

int MusicReader_GenericOPL::fnumToMilliHertz(int fnum, int block)
	throw ()
{
	double dbOriginalFreq = this->fnumConversion * (double)fnum * pow(2, (double)(block - 20));
	return dbOriginalFreq * 1000;
}


MusicWriter_GenericOPL::MusicWriter_GenericOPL(DelayType delayType)
	throw () :
		delayType(delayType),
		lastTick(0),
		cachedDelay(0),
		//delayedReg(0),  // will never be used uninitialised
		//delayedVal(0),  // will never be used uninitialised
		firstPair(true),
		fnumConversion(49716) // TODO: or 50000 - make user settable
{
	memset(this->oplState, 0, sizeof(this->oplState));
}

MusicWriter_GenericOPL::~MusicWriter_GenericOPL()
	throw ()
{
	// If this assertion fails it means the user forgot to call finish()
	assert(this->firstPair == true);
}

void MusicWriter_GenericOPL::setPatchBank(const PatchBankPtr& instruments)
	throw (EBadPatchType)
{
	this->inst = boost::dynamic_pointer_cast<OPLPatchBank>(instruments);
	if (!this->inst) {
		// Patch bank isn't an OPL one, see if it can be converted into an
		// OPLPatchBank.  May throw EBadPatchType.
		this->inst = OPLPatchBankPtr(new OPLPatchBank(*instruments));
	}
	return;
}

void MusicWriter_GenericOPL::finish()
	throw (std::ios::failure)
{
	if (this->delayType == DelayIsPostData) {
		// The last reg/val pair have been cached, so flush them out.
		this->nextPair(this->cachedDelay, 0, this->delayedReg, this->delayedVal);
		this->cachedDelay = 0;
	}
	this->firstPair = true;
}

void MusicWriter_GenericOPL::handleEvent(TempoEvent *ev)
	throw (std::ios::failure)
{
	assert(ev->usPerTick > 0);
	this->changeSpeed(ev->usPerTick);
	return;
}

void MusicWriter_GenericOPL::handleEvent(NoteOnEvent *ev)
	throw (std::ios::failure, EChannelMismatch, EBadPatchType)
{
	assert(this->inst);

	int fnum, block;
	this->milliHertzToFnum(ev->milliHertz, &fnum, &block);

	int oplChannel = ev->channel % 14; // TODO: channel map for >9 chans
	int rhythm = 0;
	int chipIndex = 0; // TODO: calculate from channel map

	int delay = ev->absTime - this->lastTick;

	// Set the delay as a cached delay, so that it will be written out before the
	// next register write, whenever that happens to be.
	this->cachedDelay += delay;

	// See if the instrument is already set
	if (ev->instrument >= this->inst->getPatchCount()) {
		std::stringstream ss;
		ss << "Instrument bank too small - tried to play note with instrument #"
			<< ev->instrument + 1 << " but patch bank only has "
			<< this->inst->getPatchCount() << " instruments.";
		throw EBadPatchType(ss.str());
	}
	OPLPatchPtr i = this->inst->getTypedPatch(ev->instrument);

	if (oplChannel > 8) {
		// Rhythm-mode instrument
		rhythm = oplChannel - 8;

		// Make sure the correct rhythm channel is in use
		if (i->rhythm != rhythm) {
			std::stringstream ss;
			ss << "Instrument type mismatch - patch was for "
				<< rhythmToText(i->rhythm) << " but channel can only be used for "
				<< rhythmToText(rhythm);
			throw EChannelMismatch(ev->instrument, oplChannel, ss.str());
		}

		switch (rhythm) {
			case 1:
				oplChannel = 7;
				this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
				break; // mod, hihat
			case 2:
				oplChannel = 8;
				this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
				break; // car, topcym
			case 3:
				oplChannel = 8;
				this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
				break; // mod, tomtom
			case 4:
				oplChannel = 7;
				this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
				break; // car, snare
			case 5:
				oplChannel = 6;
				this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
				this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
				break; // both, bassdrum
		}

	} else { // normal instrument, write both operators
		if (i->rhythm != 0) {
			std::stringstream ss;
			ss << "Instrument type mismatch - patch was for rhythm instrument ("
				<< rhythmToText(i->rhythm) << ") but channel can only be used for "
					"normal (non-rhythm) instruments";
			throw EChannelMismatch(ev->instrument, oplChannel, ss.str());
		}
		this->writeOpSettings(chipIndex, oplChannel, 0, i, ev->velocity);
		this->writeOpSettings(chipIndex, oplChannel, 1, i, ev->velocity);
	}

	// Write the rest of the (global) instrument settings.
	//if ((rhythm != 1) && (rhythm != 2)) {
	this->writeNextPair(0, chipIndex, BASE_FEED_CONN | oplChannel,  ((i->feedback & 7) << 1) | (i->connection & 1));
	//}
	// These next two affect the entire synth (all instruments/channels)
	//TODO curPatch->deepTremolo     =  this->oplState[BASE_RHYTHM] >> 7;
	//TODO curPatch->deepVibrato     = (this->oplState[BASE_RHYTHM] >> 6) & 1;

	if (rhythm == 0) { // normal instrument
		// Write lower eight bits of note freq
		this->writeNextPair(0, chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

		// Write 0xB0 w/ keyon bit
		this->writeNextPair(0, chipIndex, 0xB0 | oplChannel,
			OPLBIT_KEYON  // keyon enabled
			| (block << 2) // and the octave
			| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
		);
	} else { // rhythm instrument
		if ((rhythm != 2) && (rhythm != 1)) { // can't set hihat or cymbal freq
			// Write lower eight bits of note freq
			this->writeNextPair(0, chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

			this->writeNextPair(0, chipIndex, 0xB0 | oplChannel,
				//OPLBIT_KEYON  // different keyon for rhythm instruments
				(block << 2) // and the octave
				| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
			);
		}
		// Write keyon for correct instrument
		this->writeNextPair(0, chipIndex, 0xBD,
			0x20 |  // enable percussion mode
			this->oplState[chipIndex][0xBD] |
			(1 << (rhythm-1))  // instrument-specific keyon
		);
	}

	this->lastTick = ev->absTime;
	return;
}

void MusicWriter_GenericOPL::handleEvent(NoteOffEvent *ev)
	throw (std::ios::failure)
{
	// TODO: These are just converting back the mappings we used when reading
	// files.  We'll need to implement some kind of proper channel map (probably
	// specified in the UI with the instrument map) so that arbitrary incoming
	// channels can be mapped to correct OPL instrument/rhythm channels.
	int oplChannel = ev->channel % 14; // TODO: channel map for >9 chans
	int chipIndex = ev->channel / 14; // TODO: calculate from channel map


	int delay = ev->absTime - this->lastTick;

	if (oplChannel > 8) {
		// Rhythm-mode instrument
		int rhythm = oplChannel - 8;
		this->writeNextPair(delay, chipIndex, 0xBD,
			(this->oplState[chipIndex][0xBD] & ~(1 << (rhythm-1)))
		);
	} else {
		// Write 0xB0 w/ keyon bit disabled
		this->writeNextPair(delay, chipIndex, 0xB0 | oplChannel,
			(this->oplState[chipIndex][0xB0 | oplChannel] & ~OPLBIT_KEYON)
		);
	}

	this->lastTick = ev->absTime;
	return;
}

void MusicWriter_GenericOPL::handleEvent(PitchbendEvent *ev)
	throw (std::ios::failure)
{
	int fnum, block;
	this->milliHertzToFnum(ev->milliHertz, &fnum, &block);
	int oplChannel = ev->channel % 9; // TODO: channel map for >9 chans
	int chipIndex = 0; // TODO: calculate from channel map

	int delay = ev->absTime - this->lastTick;

	// Write lower eight bits of note freq
	this->writeNextPair(delay, chipIndex, 0xA0 | oplChannel, fnum & 0xFF);

	// Write 0xB0 w/ keyon bit
	this->writeNextPair(0, chipIndex, 0xB0 | oplChannel,
		OPLBIT_KEYON  // keyon enabled (wouldn't have a pitchbend with no note playing)
		| (block << 2) // and the octave
		| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
	);

	this->lastTick = ev->absTime;
	return;
}

void MusicWriter_GenericOPL::handleEvent(ConfigurationEvent *ev)
	throw (std::ios::failure)
{
	int delay = ev->absTime - this->lastTick;
	switch (ev->configType) {
		case ConfigurationEvent::EnableOPL3:
			this->writeNextPair(delay, 0, 0x05, ev->value ? 0x01 : 0x00);
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			if (ev->value & 1) this->oplState[ev->value >> 1][0xBD] |= 0x80;
			else this->oplState[ev->value >> 1][0xBD] &= 0x7F;
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			if (ev->value & 1) this->oplState[ev->value >> 1][0xBD] |= 0x40;
			else this->oplState[ev->value >> 1][0xBD] &= 0xBF;
			break;
		case ConfigurationEvent::EnableRhythm:
			// Nothing required, rhythm mode is enabled the first type a rhythm
			// instrument is played.
			break;
		case ConfigurationEvent::EnableWaveSel:
			this->writeNextPair(delay, 0, 0x01, ev->value ? 0x20 : 0x00);
			break;
	}
	this->lastTick = ev->absTime;
	return;
}

void MusicWriter_GenericOPL::writeNextPair(uint32_t delay, uint8_t chipIndex,
	uint8_t reg, uint8_t val
)
	throw (std::ios::failure)
{
	// TODO: Cache all the events with no delay then optimise them before
	// writing them out (once a delay is encountered.)  This should allow multiple
	// 0xBD register writes to be combined into one, for situations where multiple
	// perc instruments play at the same instant.
	if (this->oplState[chipIndex][reg] != val) {
		if (this->delayType == DelayIsPreData) {
			this->nextPair(this->cachedDelay + delay, chipIndex, reg, val);
		} else { // DelayIsPostData
			// The delay we've cached should happen before the next data bytes are
			// written.  But this is a DelayIsPostData format, which means the delay
			// value we pass to nextPair() will happen *after* the data bytes.  So
			// instead we have to now write the previous set of bytes, with the cached
			// delay, and then remember the current bytes to write out later!
			if (!this->firstPair) {
				this->nextPair(this->cachedDelay + delay, chipIndex,
					this->delayedReg, this->delayedVal);
			} else {
				// This is the first data pair, so there are no delayed values to write,
				// we just cache the new values.  We will lose any delay before the
				// first note starts, but that should be fine...if you want silence
				// at the start of a song then wait longer before pressing play :-)
				this->firstPair = false;
			}
			this->delayedReg = reg;
			this->delayedVal = val;
		}
		this->cachedDelay = 0;
		this->oplState[chipIndex][reg] = val;
	} else {
		// Value was already set, don't write redundant data, but remember the
		// delay so it can be added to the next write.
		this->cachedDelay += delay;
	}
	return;
}

void MusicWriter_GenericOPL::writeOpSettings(int chipIndex, int oplChannel,
	int opNum, OPLPatchPtr i, int velocity
)
	throw (std::ios::failure)
{
	uint8_t op;
	OPLOperator *o;
	int volume;
	if (opNum == 0) {
		op = OPLOFFSET_MOD(oplChannel);
		o = &(i->m);
		volume = o->outputLevel;
	} else {
		op = OPLOFFSET_CAR(oplChannel);
		o = &(i->c);
		volume = o->outputLevel;
		if (velocity != 0) {
			// Not using default velocity
			volume = 0x3F - (velocity >> 2);
		}
	}
//this->writeNextPair(0, chipIndex, 0x01, 0x20); // WSEnable

	this->writeNextPair(0, chipIndex, BASE_CHAR_MULT | op,
		((o->enableTremolo & 1) << 7) |
		((o->enableVibrato & 1) << 6) |
		((o->enableSustain & 1) << 5) |
		((o->enableKSR     & 1) << 4) |
		 (o->freqMult      & 0x0F)
	);
	this->writeNextPair(0, chipIndex, BASE_SCAL_LEVL | op, (o->scaleLevel << 6) | (o->outputLevel & 0x3F));
	this->writeNextPair(0, chipIndex, BASE_ATCK_DCAY | op, (o->attackRate << 4) | (o->decayRate & 0x0F));
	this->writeNextPair(0, chipIndex, BASE_SUST_RLSE | op, (o->sustainRate << 4) | (o->releaseRate & 0x0F));
	this->writeNextPair(0, chipIndex, BASE_WAVE      | op,  o->waveSelect & 7);

	return;
}

void MusicWriter_GenericOPL::milliHertzToFnum(int milliHertz, int *fnum, int *block)
	throw ()
{
	// Special case to avoid divide by zero
	if (milliHertz == 0) {
		*block = 0; // actually any block will work
		*fnum = 0;
		return;
	}

	int invertedBlock = log2(6208430 / milliHertz);
	// Very low frequencies will produce very high inverted block numbers, but
	// as they can all be covered by inverted block 7 (block 0) we can just clip
	// the value.
	if (invertedBlock > 7) invertedBlock = 7;

	*block = 7 - invertedBlock;
	*fnum = milliHertz * pow(2, 20 - *block) / 1000 / this->fnumConversion;
	if ((*block == 7) && (*fnum > 1023)) {
		std::cerr << "Warning: Frequency out of range, clipped to max" << std::endl;
		*fnum = 1023;
	}

	// Make sure the values come out within range
	assert(*block >= 0);
	assert(*block <= 7);
	assert(*fnum >= 0);
	assert(*fnum < 1024);
	return;
}
