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

class OPLDecoder
{
	public:
		/// Set decoding parameters.
		/**
		 * @param cb
		 *   Callback class used to read the actual OPL data bytes from the file.
		 *
		 * @param delayType
		 *   Where the delay is actioned - before its associated data pair is sent
		 *   to the OPL chip, or after.
		 *
		 * @param fnumConversion
		 *   Conversion constant to use when converting OPL frequency numbers into
		 *   Hertz.  Can be one of OPL_FNUM_* or a raw value.
		 *
		 * @param initialTempo
		 *   Initial tempo of the song.
		 */
		OPLDecoder(OPLReaderCallback *cb, DelayType delayType,
			double fnumConversion, const Tempo& initialTempo);

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
		Tempo initialTempo;        ///< Initial tempo to set

		unsigned long lastDelay[OPL_TRACK_COUNT];   ///< Delay ticks accrued since last event
		uint8_t oplState[OPL_NUM_CHIPS][256];  ///< Current register values
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

		void createNoteOn(const TrackPtr& track, PatchBankPtr& patches,
			unsigned long *lastDelay, unsigned int chipIndex, unsigned int oplChannel,
			unsigned int rhythm, unsigned int b0val);

		void createOrUpdatePitchbend(const TrackPtr& track,
			unsigned long *lastDelay, unsigned int a0val, unsigned int b0val);
};


MusicPtr camoto::gamemusic::oplDecode(OPLReaderCallback *cb,
	DelayType delayType, double fnumConversion, const Tempo& initialTempo)
{
	OPLDecoder decoder(cb, delayType, fnumConversion, initialTempo);
	return decoder.decode();
}


OPLDecoder::OPLDecoder(OPLReaderCallback *cb, DelayType delayType,
	double fnumConversion, const Tempo& initialTempo)
	:	cb(cb),
		delayType(delayType),
		fnumConversion(fnumConversion),
		initialTempo(initialTempo),
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

	music->initialTempo = this->initialTempo;
	music->loopDest = -1; // no loop

	for (unsigned int c = 0; c < OPL_TRACK_COUNT; c++) {
		TrackInfo t;
		if (c < 9) {
			t.channelType = TrackInfo::OPLChannel;
			t.channelIndex = c;
		} else if (c < 9+5) {
			t.channelType = TrackInfo::OPLPercChannel;
			/// @todo: Make sure this correctly maps to the right perc instrument
			t.channelIndex = c - 9;
		} else {
			t.channelType = TrackInfo::OPLChannel;
			t.channelIndex = c - (9+5);
		}
		music->trackInfo.push_back(t);
	}

	PatternPtr pattern(new Pattern());
	for (unsigned int track = 0; track < OPL_TRACK_COUNT; track++) {
		TrackPtr t(new Track());
		pattern->push_back(t);
	}

	// Initialise all OPL registers to zero
	memset(oplState, 0, sizeof(oplState));
	for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) this->lastDelay[t] = 0;

	unsigned long totalDelay = 0;

	Tempo lastTempo = this->initialTempo;
	OPLEvent oplev;
	oplev.tempo = lastTempo;
	bool opl3 = false;
	for (;;) {
		oplev.valid = 0;
		if (!this->cb->readNextPair(&oplev)) {
			// No more events
			if (oplev.valid & OPLEvent::Delay) {
				// oplev.delay still has a final trailing delay
				totalDelay += oplev.delay;
				for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
					this->lastDelay[t] += oplev.delay;
				}
			}
			break;
		}
		assert(oplev.chipIndex < 2);
		totalDelay += oplev.delay;

		if (this->delayType == DelayIsPreData) {
			for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
				this->lastDelay[t] += oplev.delay;
			}
		}

		if ((oplev.valid & OPLEvent::Tempo) && (oplev.tempo != lastTempo)) {
			TrackEvent te;
			te.delay = this->lastDelay[0];
			this->lastDelay[0] = 0;
			TempoEvent *ev = new TempoEvent();
			te.event.reset(ev);
			ev->tempo = oplev.tempo;
			pattern->at(0)->push_back(te);
			lastTempo = oplev.tempo;
		}

		// Update the current state with the new value
		uint8_t oldval = oplState[oplev.chipIndex][oplev.reg];
		oplState[oplev.chipIndex][oplev.reg] = oplev.val;
#define bitsChanged(b) ((oplev.val ^ oldval) & b)

		unsigned int oplChannel = oplev.reg & 0x0F; // Only for regs 0xA0, 0xB0 and 0xC0!
		//uint8_t channel = 1 + oplChannel + 14 * oplev.chipIndex;

		// Figure out what track to map this event to
#define OPL_IS_RHYTHM_ON (this->oplState[0][0xBD] & 0x20)
		bool noteon = false;
		unsigned int track = -1;
		if (oplev.reg == 0xBD) {
			// handled below, could map to multiple channels at the same time
		} else if (oplev.reg < 0x20) {
			track = 0;
		} else if ((oplev.reg < 0xA0) || (oplev.reg >= 0xE0)) {
			// Convert operator index into channel
			unsigned int oplOpIndex = oplev.reg & 0x1F;
			unsigned int rhythm = 99;
			if (OPL_IS_RHYTHM_ON && (oplev.chipIndex == 0)) {
				switch (oplOpIndex) {
					case 13: rhythm = 0; break; // hi-hat
					case 17: rhythm = 1; break; // top cymbal
					case 14: rhythm = 2; break; // tom-tom
					case 16: rhythm = 3; break; // snare
					case 12:
					case 15: rhythm = 4; break; // bass drum
					default: break; // normal instrument
				}
			}
			if (rhythm == 99) {
				unsigned int oplChannel = OPL2_OFF2CHANNEL(oplOpIndex); /// @todo: This only works for OPL2
				track = oplChannel + OPL_TRACK_COUNT * oplev.chipIndex;
				noteon = oplState[oplev.chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON;
			} else {
				track = 9 + rhythm;
				noteon = this->oplState[0][0xBD] & (1 << rhythm);
			}

		} else { // A0, B0, C0
			assert(oplev.reg < 0xE0);
			unsigned int oplChannel = oplev.reg & 0x0F;
			if (oplChannel > 8) {
				std::cout << "decode-opl: Invalid OPL channel " << oplChannel
					<< "\n";

				if (this->delayType == DelayIsPostData) {
					for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
						this->lastDelay[t] += oplev.delay;
					}
				}
				// Update the current state with the new value
				oplState[oplev.chipIndex][oplev.reg] = oplev.val;
				continue;
			}

			if ((OPL_IS_RHYTHM_ON) && (oplev.chipIndex == 0) && (oplChannel > 5)) {
				// Rhythm mode instrument
				noteon = false;
				track = -1;
			} else {
				// Normal channel (or rhythm mode bass drum)
				track = oplChannel + OPL_TRACK_COUNT * oplev.chipIndex;
				noteon = oplState[oplev.chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON;
			}
		}

		unsigned int regClass = oplev.reg & 0xF0;
		if (oplev.reg == 0xBD) regClass = 0xBD;
		switch (regClass) {
			case 0x00:
				if (oplev.reg == 0x01) {
					if (bitsChanged(0x20)) {
						TrackEvent te;
						te.delay = this->lastDelay[track];
						this->lastDelay[track] = 0;
						ConfigurationEvent *ev = new ConfigurationEvent();
						te.event.reset(ev);
						ev->configType = ConfigurationEvent::EnableWaveSel;
						ev->value = (oplev.val & 0x20) ? 1 : 0;
						pattern->at(0)->push_back(te);
					}
				} else if (oplev.reg == 0x05) {
					if (bitsChanged(0x01)) {
						unsigned int newState = (oplev.val & 0x01) ? 1 : 0;
						if (
							((opl3) && (newState == 0))
							|| ((!opl3) && (newState == 1))
						) {
							TrackEvent te;
							te.delay = this->lastDelay[track];
							this->lastDelay[track] = 0;
							ConfigurationEvent *ev = new ConfigurationEvent();
							te.event.reset(ev);
							ev->configType = ConfigurationEvent::EnableOPL3;
							ev->value = newState;
							pattern->at(0)->push_back(te);
							opl3 = newState == 1;
						}
					}
				}
				break;

			case 0x40:
			case 0x50:
				if (noteon && bitsChanged(0x3F)) {
					// Volume change
/// @todo createOrUpdateVolume, plus replace volume if it is immediately followed by noteon
				}
				break;

			case 0xA0: {
				if (noteon && bitsChanged(0xFF)) {
					// The pitch has changed while a note was playing
					this->createOrUpdatePitchbend(pattern->at(track),
						&this->lastDelay[track], oplev.val,
						oplState[oplev.chipIndex][0xB0 | oplChannel]);
				}
				break;
			}

			case 0xBD:
				if (oplev.val & 0x20) { // can't use OPL_IS_RHYTHM_ON as that is obsolete here
					if (bitsChanged(0x20)) {
						// Rhythm was off, now it's on
						track = 0;
						TrackEvent te;
						te.delay = this->lastDelay[track];
						this->lastDelay[track] = 0;
						ConfigurationEvent *ev = new ConfigurationEvent();
						te.event.reset(ev);
						ev->configType = ConfigurationEvent::EnableRhythm;
						ev->value = 1;
						pattern->at(track)->push_back(te);
					}
					for (int rhythm = 0; rhythm < 5; rhythm++) {
						int keyonBit = 1 << rhythm;
						// If rhythm mode has just been enabled and this instrument's
						// keyon bit is set, OR rhythm mode was always on and this
						// instrument's keyon bit has changed, write out a note-on or
						// note-off event as appropriate.
						if ((bitsChanged(0x20) && (oplev.val & keyonBit)) || bitsChanged(keyonBit)) {
							// Hi-hat is playing or about to play
							switch (rhythm) {
								case 0: oplChannel = 7; break; // mod
								case 1: oplChannel = 8; break; // car
								case 2: oplChannel = 8; break; // mod
								case 3: oplChannel = 7; break; // car
								case 4: oplChannel = 6; break; // both
							}

							track = 9 + rhythm;
							noteon = this->oplState[0][0xBD] & keyonBit;

							if (oplev.val & keyonBit) {
								this->createNoteOn(pattern->at(track), patches, &this->lastDelay[track],
									oplev.chipIndex, oplChannel, rhythm + 1,
									oplState[oplev.chipIndex][0xB0 | oplChannel]);
							} else {
								TrackEvent te;
								te.delay = this->lastDelay[track];
								this->lastDelay[track] = 0;
								te.event.reset(new NoteOffEvent());
								pattern->at(track)->push_back(te);
							}
						}
					}
				} else if (bitsChanged(0x20)) { // Rhythm mode just got disabled
					// Turn off any currently playing rhythm instruments
					for (int rhythm = 0; rhythm < 5; rhythm++) {
						track = 9 + rhythm;
						noteon = this->oplState[0][0xBD] & (1 << rhythm);
						if (noteon) {
							TrackEvent te;
							te.delay = this->lastDelay[track];
							this->lastDelay[track] = 0;
							te.event.reset(new NoteOffEvent());
							pattern->at(track)->push_back(te);
						}
					}
					// Rhythm was on, now it's off
					track = 0;
					TrackEvent te;
					te.delay = this->lastDelay[track];
					this->lastDelay[track] = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableRhythm;
					ev->value = 0;
					pattern->at(track)->push_back(te);
				}
				if (bitsChanged(0x80)) {
					track = 0;
					TrackEvent te;
					te.delay = this->lastDelay[track];
					this->lastDelay[track] = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableDeepTremolo;
					ev->value = (oplev.val & 0x80) ? 1 : 0; // bit0 is enable/disable
					if (oplev.chipIndex) ev->value |= 2;    // bit1 is chip index
					pattern->at(track)->push_back(te);
				}
				if (bitsChanged(0x40)) {
					track = 0;
					TrackEvent te;
					te.delay = this->lastDelay[track];
					this->lastDelay[track] = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableDeepVibrato;
					ev->value = (oplev.val & 0x40) ? 1 : 0; // bit0 is enable/disable
					if (oplev.chipIndex) ev->value |= 2;    // bit1 is chip index
					pattern->at(track)->push_back(te);
				}
				break;

			case 0xB0:
				if (oplChannel > 8) {
					std::cerr << "decode-opl: Bad OPL note-on register/channel 0x" << std::hex
						<< (int)oplev.reg << std::endl;
					break;
				}

				if (OPL_IS_RHYTHM_ON && (oplev.chipIndex == 0) && (oplChannel > 5)) {
					if (noteon && bitsChanged(0x1F)) {
						// Rhythm-mode instrument (incl. bass drum) has changed pitch
						this->createOrUpdatePitchbend(pattern->at(track), &this->lastDelay[track],
							oplState[oplev.chipIndex][0xA0 | oplChannel], oplev.val);
					}
				} else { // normal instrument
					if (bitsChanged(OPLBIT_KEYON)) {
						if (oplev.val & OPLBIT_KEYON) {
							// Note is now on
							this->createNoteOn(pattern->at(track), patches, &this->lastDelay[track],
								oplev.chipIndex, oplChannel, 0, oplev.val);
						} else {
							// Note is now off
							TrackEvent te;
							te.delay = this->lastDelay[track];
							this->lastDelay[track] = 0;
							NoteOffEvent *ev = new NoteOffEvent();
							te.event.reset(ev);
							pattern->at(track)->push_back(te);
						}
					} else if (noteon && bitsChanged(0x1F)) {
						// The note is already on and the pitch has changed.
						this->createOrUpdatePitchbend(pattern->at(track), &this->lastDelay[track],
							oplState[oplev.chipIndex][0xA0 | oplChannel], oplev.val);
					}
				}
				break;
		} // switch (OPL reg)

		/// @todo If the instrument settings have changed, generate a patch change event?
		// Will have to combine with previous patch change event if there has been no delay.

		if (this->delayType == DelayIsPostData) {
			for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
				this->lastDelay[t] += oplev.delay;
			}
		}

		// Update the current state with the new value
		oplState[oplev.chipIndex][oplev.reg] = oplev.val;

	} // while (all events)

	// Put dummy events if necessary to preserve trailing delays
	for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
		if (this->lastDelay[t] && pattern->at(t)->size()) {
			TrackEvent te;
			te.delay = this->lastDelay[t];
			this->lastDelay[t] = 0;
			ConfigurationEvent *ev = new ConfigurationEvent();
			te.event.reset(ev);
			ev->configType = ConfigurationEvent::None;
			ev->value = 0;
			pattern->at(t)->push_back(te);
		}
	}

	music->patterns.push_back(pattern);
	music->patternOrder.push_back(0);
	music->ticksPerTrack = totalDelay;

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

void OPLDecoder::createNoteOn(const TrackPtr& track, PatchBankPtr& patches,
	unsigned long *lastDelay, unsigned int chipIndex, unsigned int oplChannel,
	unsigned int rhythm, unsigned int b0val)
{
	TrackEvent te;
	te.delay = *lastDelay;
	*lastDelay = 0;
	NoteOnEvent *ev = new NoteOnEvent();
	te.event.reset(ev);

	OPLPatchPtr curPatch = this->getCurrentPatch(chipIndex, oplChannel);
	curPatch->rhythm = rhythm;

	/// Make sure the patch has been added to the patchbank
	ev->instrument = this->savePatch(patches, curPatch);

	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | this->oplState[chipIndex][0xA0 | oplChannel];
	int block = (b0val >> 2) & 0x07;

	ev->milliHertz = fnumToMilliHertz(fnum, block, this->fnumConversion);

	// Ignore velocity for modulator-only rhythm instruments
	if (OPL_IS_RHYTHM_MODULATOR_ONLY(rhythm)) {
		ev->velocity = DefaultVelocity;
	} else {
		unsigned int curVol = 0x3F &
			this->oplState[chipIndex][BASE_SCAL_LEVL | OPLOFFSET_CAR(oplChannel)];
		ev->velocity = log_volume_to_lin_velocity(63 - curVol, 63);
	}
	track->push_back(te);
	return;
}

void OPLDecoder::createOrUpdatePitchbend(const TrackPtr& track,
	unsigned long *lastDelay, unsigned int a0val, unsigned int b0val)
{
	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | a0val;
	int block = (b0val >> 2) & 0x07;
	int freq = ::fnumToMilliHertz(fnum, block, this->fnumConversion);

	// See if there is already a pitchbend event in the buffer with the
	// same time as this one.  If there is, it will be because the OPL
	// fnum is spread across two registers and we should combine this
	// into a single event.

	bool addNew = true;

	// Still at the last event
	if (*lastDelay == 0) {
		// This will only check the previous event, if there's something else
		// (like an instrument effect) in between the two pitch events then
		// they won't be combined into a single pitchbend event.
		for (Track::reverse_iterator
			i = track->rbegin(); i != track->rend(); i++
		) {
			const TrackEvent& te = *i;
			if (te.delay != 0) break; // no more events at this time
			EffectEvent *pbev = dynamic_cast<EffectEvent *>(i->event.get());
			if (pbev && (pbev->type == EffectEvent::PitchbendNote)) {
				// There is an existing pitchbend event at the same time, so edit
				// that one.
				pbev->data = freq;
				addNew = false;
				break;
			}
		}
	}

	if (addNew) {
		// Last event was unrelated, create a new pitchbend event.
		TrackEvent te;
		te.delay = *lastDelay;
		*lastDelay = 0;
		EffectEvent *ev = new EffectEvent();
		te.event.reset(ev);
		ev->type = EffectEvent::PitchbendNote;
		ev->data = freq;
		track->push_back(te);
	}
	return;
}
