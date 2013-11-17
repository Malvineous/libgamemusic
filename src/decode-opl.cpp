/**
 * @file   decode-opl.cpp
 * @brief  Function to convert raw OPL data into a Music instance.
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

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/opl-util.hpp>
#include "decode-opl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

///< A default value to use since most OPL songs don't have a field for this.
const unsigned int OPL_DEF_TICKS_PER_QUARTER_NOTE = 192;

class OPLDecoder
{
	public:
		/// Set decoding parameters.
		/**
		 * @param delayType
		 *   Where the delay is actioned - before its associated data pair is sent
		 *   to the OPL chip, or after.
		 *
		 * @param fnumConversion
		 *   Conversion constant to use when converting OPL frequency numbers into
		 *   Hertz.  Can be one of OPL_FNUM_* or a raw value.
		 *
		 * @param cb
		 *   Callback class used to read the actual OPL data bytes from the file.
		 */
		OPLDecoder(OPLReaderCallback *cb, DelayType delayType, double fnumConversion);

		~OPLDecoder();

		/// Read the input data and set events and instruments accordingly.
		/**
		 * This function will call the callback's readNextPair() function repeatedly
		 * until it returns false.  It will populate the events and instruments in
		 * the supplied Music instance.
		 *
		 * @throw stream:error
		 *   If the input data could not be read or converted for some reason.
		 */
		MusicPtr decode();

	private:
		OPLReaderCallback *cb;     ///< Callback to use to get more OPL data
		DelayType delayType;       ///< Location of the delay
		double fnumConversion;     ///< Conversion value to use in fnum -> Hz calc

		unsigned long lastTick;    ///< Time of last event
		uint8_t oplState[2][256];  ///< Current register values
		PatchBankPtr patches;      ///< Patch storage

		OPLPatchPtr getCurrentPatch(int chipIndex, int oplChannel);

		/// Add the given patch to the patchbank.
		/**
		 * @param patches
		 *   Patchbank to search and possibly append to.
		 *
		 * @param curPatch
		 *   Patch to search for and possibly add if it is not already in the
		 *   patchbank.
		 *
		 * @return Index of this instrument in the patchbank.
		 */
		int savePatch(PatchBankPtr& patches, OPLPatchPtr curPatch);

		EventPtr createNoteOn(PatchBankPtr& patches, uint8_t chipIndex,
			uint8_t oplChannel, int rhythm, int channel, int b0val);

		void createOrUpdatePitchbend(EventVectorPtr events,
			uint8_t chipIndex, int channel, int a0val, int b0val);
};


MusicPtr camoto::gamemusic::oplDecode(OPLReaderCallback *cb, DelayType delayType,
	double fnumConversion)
{
	OPLDecoder decoder(cb, delayType, fnumConversion);
	return decoder.decode();
}


OPLDecoder::OPLDecoder(OPLReaderCallback *cb, DelayType delayType, double fnumConversion)
	:	cb(cb),
		delayType(delayType),
		fnumConversion(fnumConversion),
		patches(new PatchBank())
{
}

OPLDecoder::~OPLDecoder()
{
}

MusicPtr OPLDecoder::decode()
{
	MusicPtr music(new Music());
	music->patches = this->patches;
	// The patches inside the Music instance and inside our own instance are now
	// the same, except of course the Music ones are generic, but our ones are
	// typed as OPL instruments.
	music->events.reset(new EventVector());
	music->ticksPerQuarterNote = OPL_DEF_TICKS_PER_QUARTER_NOTE;

	// Initialise all OPL registers to zero
	memset(oplState, 0, sizeof(oplState));
	this->lastTick = 0;
	unsigned long lastTempo = 0;

	OPLEvent oplev;
	oplev.tempo = 0;
	while (this->cb->readNextPair(&oplev)) {
		assert(oplev.chipIndex < 2);

		if (this->delayType == DelayIsPreData) this->lastTick += oplev.delay;

		// Update the current state with the new value
		uint8_t oldval = oplState[oplev.chipIndex][oplev.reg];
		oplState[oplev.chipIndex][oplev.reg] = oplev.val;
#define bitsChanged(b) ((oplev.val ^ oldval) & b)

		uint8_t oplChannel = oplev.reg & 0x0F; // Only for regs 0xA0, 0xB0 and 0xC0!
		uint8_t channel = 1 + oplChannel + 14 * oplev.chipIndex;
		switch (oplev.reg & 0xF0) {
			case 0x00:
				if (oplev.reg == 0x01) {
					if (bitsChanged(0x20)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableWaveSel;
						ev->value = (oplev.val & 0x20) ? 1 : 0;
						music->events->push_back(gev);
					}
				} else if (oplev.reg == 0x05) {
					if (bitsChanged(0x01)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableOPL3;
						ev->value = (oplev.val & 0x01) ? 1 : 0;
						music->events->push_back(gev);
					}
				}
				break;
			case 0xA0:
				if (bitsChanged(0xFF) && (oplState[oplev.chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON)) {
					// The pitch has changed while a note was playing
					this->createOrUpdatePitchbend(music->events, oplev.chipIndex, channel,
						oplev.val, oplState[oplev.chipIndex][0xB0 | oplChannel]);
				}
				break;
			case 0xB0:
				if (oplev.reg == 0xBD) {
					// Percussion/rhythm mode
					if (oplev.val & 0x20) {
						// Rhythm mode enabled
						for (int p = 0; p < 5; p++) {
							int keyonBit = 1 << p;
							// If rhythm mode has just been enabled and this instrument's
							// keyon bit is set, OR rhythm mode was always on and this
							// instrument's keyon bit has changed, write out a note-on or
							// note-off event as appropriate.
							if ((bitsChanged(0x20) && (oplev.val & keyonBit)) || bitsChanged(keyonBit)) {
								// Hi-hat is playing or about to play
								channel = 14 * oplev.chipIndex + 9 + p;
								switch (p) {
									case 0: oplChannel = 7; break; // mod
									case 1: oplChannel = 8; break; // car
									case 2: oplChannel = 8; break; // mod
									case 3: oplChannel = 7; break; // car
									case 4: oplChannel = 6; break; // both
								}
								EventPtr gev;
								if (oplev.val & keyonBit) {
									gev = this->createNoteOn(patches, oplev.chipIndex, oplChannel, p + 1,
										channel, oplState[oplev.chipIndex][0xB0 | oplChannel]);
								} else {
									gev.reset(new NoteOffEvent());
									gev->absTime = this->lastTick;
									gev->channel = channel;
								}
								music->events->push_back(gev);
							}
						}
					} else if (bitsChanged(0x20)) { // Rhythm mode just got disabled
						// Turn off any currently playing rhythm instruments
						for (int p = 0; p < 5; p++) {
							if (oplState[oplev.chipIndex][0xBD] & (1 << p)) {
								EventPtr gev(new NoteOffEvent());
								gev->absTime = this->lastTick;
								gev->channel = 1 + 9 + p;
								music->events->push_back(gev);
							}
						}
					}
					if (bitsChanged(0x80)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableDeepTremolo;
						ev->value = (oplev.val & 0x80) ? 1 : 0; // bit0 is enable/disable
						if (oplev.chipIndex) ev->value |= 2;    // bit1 is chip index
						music->events->push_back(gev);
					}
					if (bitsChanged(0x40)) {
						ConfigurationEvent *ev = new ConfigurationEvent();
						EventPtr gev(ev);
						ev->channel = 0; // unused
						ev->absTime = this->lastTick;
						ev->configType = ConfigurationEvent::EnableDeepVibrato;
						ev->value = (oplev.val & 0x40) ? 1 : 0; // bit0 is enable/disable
						if (oplev.chipIndex) ev->value |= 2;    // bit1 is chip index
						music->events->push_back(gev);
					}

				} else if (oplChannel < 9) { // valid register/channel
					if (bitsChanged(OPLBIT_KEYON)) {
						if (oplev.val & OPLBIT_KEYON) {
							// Note is on, get instrument settings of this note
							EventPtr gev = this->createNoteOn(patches, oplev.chipIndex, oplChannel, 0, channel, oplev.val);
							music->events->push_back(gev);
						} else {
							EventPtr gev(new NoteOffEvent());
							gev->absTime = this->lastTick;
							gev->channel = channel;
							music->events->push_back(gev);
						}
					} else if ((oplev.val & OPLBIT_KEYON) && bitsChanged(0x1F)) {
						// The note is already on and the pitch has changed.
						this->createOrUpdatePitchbend(music->events, oplev.chipIndex, channel,
							oplState[oplev.chipIndex][0xA0 | oplChannel], oplev.val);
					}
				} else {
					std::cerr << "WARNING: Bad OPL register/channel 0x" << std::hex
						<< (int)oplev.reg << std::endl;
				}

				break;
		} // switch (OPL reg)

		if ((oplev.tempo) && (oplev.tempo != lastTempo)) {
			TempoEvent *ev = new TempoEvent();
			EventPtr gev(ev);
			ev->channel = 0; // global event (all channels)
			ev->absTime = this->lastTick;
			ev->usPerTick = oplev.tempo;
			music->events->push_back(gev);
			lastTempo = oplev.tempo;
			oplev.tempo = 0;
		}

		/// @todo If the instrument settings have changed, generate a patch change event?
		// Will have to combine with previous patch change event if there has been no delay.

		if (this->delayType == DelayIsPostData) this->lastTick += oplev.delay;

		// Update the current state with the new value
		oplState[oplev.chipIndex][oplev.reg] = oplev.val;

	} // while (all events)

	return music;
}

OPLPatchPtr OPLDecoder::getCurrentPatch(int chipIndex, int oplChannel)
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
	curPatch->rhythm          = 0; // will be overridden later if needed
	return curPatch;
}

int OPLDecoder::savePatch(PatchBankPtr& patches, OPLPatchPtr curPatch)
{
	// See if the patch has already been saved
	unsigned int j = 0;
	for (PatchBank::const_iterator i = patches->begin(); i != patches->end(); i++, j++) {
		OPLPatchPtr p = boost::dynamic_pointer_cast<OPLPatch>(*i);
		if ((p) && (*p == *curPatch)) return j; // patch already saved
	}

	// Save the patch
	unsigned int numPatches = patches->size();
	patches->push_back(curPatch);
	return numPatches;
}

EventPtr OPLDecoder::createNoteOn(PatchBankPtr& patches, uint8_t chipIndex,
	uint8_t oplChannel, int rhythm, int channel, int b0val
)
{
	NoteOnEvent *ev = new NoteOnEvent();
	EventPtr gev(ev);
	ev->channel = channel;
	ev->absTime = this->lastTick;

	OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
	curPatch->rhythm = rhythm;

	/// Make sure the patch has been added to the patchbank
	ev->instrument = this->savePatch(patches, curPatch);

	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | this->oplState[chipIndex][0xA0 | oplChannel];
	int block = (b0val >> 2) & 0x07;

	ev->milliHertz = fnumToMilliHertz(fnum, block, this->fnumConversion);

	// Ignore velocity for MOD-only rhythm instruments
	if ((rhythm == 1) || (rhythm == 3)) {
		ev->velocity = 0;
	} else {
		uint8_t vel = this->oplState[chipIndex][BASE_SCAL_LEVL | OPLOFFSET_CAR(oplChannel)] & 0x3F;
		ev->velocity = pow(M_E, (double)(63.0 - vel) / 63.0 * log(256.0));
	}

	return gev;
}

void OPLDecoder::createOrUpdatePitchbend(EventVectorPtr events,
	uint8_t chipIndex, int channel, int a0val, int b0val)
{
	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | a0val;
	int block = (b0val >> 2) & 0x07;
	int freq = ::fnumToMilliHertz(fnum, block, this->fnumConversion);

	// See if there is already a pitchbend event in the buffer with the
	// same time as this one.  If there is, it will be because the OPL
	// fnum is spread across two registers and we should combine this
	// into a single event.

	// This will only check the previous event, if there's something else
	// (like an instrument effect) in between the two pitch events then
	// they won't be combined into a single pitchbend event.
	bool addNew = true;
	for (EventVector::reverse_iterator i = events->rbegin();
		i != events->rend();
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
		events->push_back(gev);
	}
	return;
}
