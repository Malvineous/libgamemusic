/**
 * @file  decode-opl.cpp
 * @brief Function to convert raw OPL data into a Music instance.
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

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <camoto/util.hpp> // make_unique
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/util-opl.hpp>
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
		std::unique_ptr<Music> decode();

	private:
		OPLReaderCallback *cb;     ///< Callback to use to get more OPL data
		DelayType delayType;       ///< Location of the delay
		double fnumConversion;     ///< Conversion value to use in fnum -> Hz calc
		Tempo initialTempo;        ///< Initial tempo to set

		unsigned long lastDelay[OPL_TRACK_COUNT];   ///< Delay ticks accrued since last event
		uint8_t oplState[OPL_NUM_CHIPS][256];  ///< Current register values
		//std::shared_ptr<PatchBank> patches;      ///< Patch storage

		std::shared_ptr<OPLPatch> getCurrentPatch(int chipIndex, int oplChannel);

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
		int savePatch(PatchBank& patches, std::shared_ptr<OPLPatch> curPatch);

		void createNoteOn(Track& trackEvents, PatchBank& patches,
			unsigned long *lastDelay, unsigned int chipIndex, unsigned int oplChannel,
			OPLPatch::Rhythm rhythm, unsigned int b0val);

		/// Switch off the note currently playing on the channel.
		/**
		 * This also backtracks the most recent events on the channel, and if there
		 * were any effects with zero delay before the note-off then those events
		 * are removed, since they will never be heard.
		 */
		void createNoteOff(unsigned int track, Track& trackEvents);

		void createOrUpdatePitchbend(Track& trackEvents,
			unsigned long *lastDelay, unsigned int a0val, unsigned int b0val);
};

/// Convert a chip index and OPL channel into a track index
constexpr unsigned int TRACK_INDEX_MELODIC(unsigned int chipIndex, unsigned int oplChannel)
{
	// 0..8 melodic, 0..5 perc, 0..8 chip1 melodic
	return chipIndex * 14 + oplChannel;
}

/// Convert an OPL rhythm instrument number (0..4) into a track index
constexpr unsigned int TRACK_INDEX_PERC(unsigned int rhythm)
{
	// 0..8 melodic, 0..5 perc, 0..8 chip1 melodic
	return 9 + rhythm;
}

std::unique_ptr<Music> camoto::gamemusic::oplDecode(OPLReaderCallback *cb,
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
		initialTempo(initialTempo)
{
}

OPLDecoder::~OPLDecoder()
{
}

std::unique_ptr<Music> OPLDecoder::decode()
{
	auto music = std::make_unique<Music>();
	music->patches = std::make_shared<PatchBank>();

	music->initialTempo = this->initialTempo;
	music->loopDest = -1; // no loop

	for (unsigned int c = 0; c < OPL_TRACK_COUNT; c++) {
		music->trackInfo.emplace_back();
		auto& t = music->trackInfo.back();
		if (c < 9) {
			t.channelType = TrackInfo::ChannelType::OPL;
			t.channelIndex = c;
		} else if (c < 9+5) {
			t.channelType = TrackInfo::ChannelType::OPLPerc;
			/// @todo: Make sure this correctly maps to the right perc instrument
			t.channelIndex = c - 9;
		} else {
			t.channelType = TrackInfo::ChannelType::OPL;
			t.channelIndex = c - 5;
		}
	}

	music->patterns.emplace_back();
	auto& pattern = music->patterns.back();
	for (unsigned int track = 0; track < OPL_TRACK_COUNT; track++) {
		pattern.emplace_back();
	}
	music->patternOrder.push_back(0);

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

		if (oplev.valid & OPLEvent::Delay) {
			totalDelay += oplev.delay;
			if (this->delayType == DelayType::DelayIsPreData) {
				for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
					this->lastDelay[t] += oplev.delay;
				}
			}
		}

		if ((oplev.valid & OPLEvent::Tempo) && (oplev.tempo != lastTempo)) {
			auto& trackEvents = pattern.at(0);
			trackEvents.emplace_back();
			auto& te = trackEvents.back();
			te.delay = this->lastDelay[0];
			this->lastDelay[0] = 0;
			auto ev = std::make_shared<TempoEvent>();
			ev->tempo = oplev.tempo;
			te.event = std::move(ev);
			lastTempo = oplev.tempo;
		}

		if (oplev.valid & OPLEvent::Regs) {
			assert(oplev.chipIndex < 2);
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
					track = TRACK_INDEX_MELODIC(oplev.chipIndex, oplChannel);
					noteon = oplState[oplev.chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON;
				} else {
					track = TRACK_INDEX_PERC(rhythm);
					noteon = this->oplState[0][0xBD] & (1 << rhythm);
				}

			} else { // A0, B0, C0
				assert(oplev.reg < 0xE0);
				unsigned int oplChannel = oplev.reg & 0x0F;
				if (oplChannel > 8) {
					std::cout << "decode-opl: Invalid OPL channel " << oplChannel
						<< "\n";

					if (this->delayType == DelayType::DelayIsPostData) {
						for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
							this->lastDelay[t] += oplev.delay;
						}
					}
					// Update the current state with the new value
					oplState[oplev.chipIndex][oplev.reg] = oplev.val;
					continue;
				}

				if ((OPL_IS_RHYTHM_ON) && (oplev.chipIndex == 0) && (oplChannel > 5)) {
					// Melodic event on channels used by percussive mode
					noteon = false;
					track = -1;
				} else {
					// Normal channel (or rhythm mode bass drum)
					track = TRACK_INDEX_MELODIC(oplev.chipIndex, oplChannel);
					noteon = oplState[oplev.chipIndex][0xB0 | oplChannel] & OPLBIT_KEYON;
				}
			}
			assert((track == (unsigned int)-1) || (track < pattern.size()));

			unsigned int regClass = oplev.reg & 0xF0;
			if (oplev.reg == 0xBD) regClass = 0xBD;
			switch (regClass) {
				case 0x00:
					if (oplev.reg == 0x01) {
						if (bitsChanged(0x20)) {
							auto& trackEvents = pattern.at(0);
							trackEvents.emplace_back();
							auto& te = trackEvents.back();
							te.delay = this->lastDelay[track];
							this->lastDelay[track] = 0;
							auto ev = std::make_shared<ConfigurationEvent>();
							ev->configType = ConfigurationEvent::Type::EnableWaveSel;
							ev->value = (oplev.val & 0x20) ? 1 : 0;
							te.event = std::move(ev);
						}
					} else if (oplev.reg == 0x05) {
						if (bitsChanged(0x01)) {
							unsigned int newState = (oplev.val & 0x01) ? 1 : 0;
							if (
								((opl3) && (newState == 0))
								|| ((!opl3) && (newState == 1))
							) {
								auto& trackEvents = pattern.at(0);
								trackEvents.emplace_back();
								auto& te = trackEvents.back();
								te.delay = this->lastDelay[track];
								this->lastDelay[track] = 0;
								auto ev = std::make_shared<ConfigurationEvent>();
								ev->configType = ConfigurationEvent::Type::EnableOPL3;
								ev->value = newState;
								te.event = std::move(ev);
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
						this->createOrUpdatePitchbend(pattern.at(track),
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
							auto& trackEvents = pattern.at(track);
							trackEvents.emplace_back();
							auto& te = trackEvents.back();
							te.delay = this->lastDelay[track];
							this->lastDelay[track] = 0;
							auto ev = std::make_shared<ConfigurationEvent>();
							ev->configType = ConfigurationEvent::Type::EnableRhythm;
							ev->value = 1;
							te.event = std::move(ev);
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
									this->createNoteOn(pattern.at(track), *music->patches,
										&this->lastDelay[track], oplev.chipIndex, oplChannel,
										(OPLPatch::Rhythm)(rhythm + 1),
										oplState[oplev.chipIndex][0xB0 | oplChannel]);
								} else {
									this->createNoteOff(track, pattern.at(track));
								}
							}
						}
					} else if (bitsChanged(0x20)) { // Rhythm mode just got disabled
						// Turn off any currently playing rhythm instruments
						for (int rhythm = 0; rhythm < 5; rhythm++) {
							track = 9 + rhythm;
							noteon = this->oplState[0][0xBD] & (1 << rhythm);
							if (noteon) {
								this->createNoteOff(track, pattern.at(track));
							}
						}
						// Rhythm was on, now it's off
						track = 0;
						auto& trackEvents = pattern.at(track);
						trackEvents.emplace_back();
						auto& te = trackEvents.back();
						te.delay = this->lastDelay[track];
						this->lastDelay[track] = 0;
						auto ev = std::make_shared<ConfigurationEvent>();
						ev->configType = ConfigurationEvent::Type::EnableRhythm;
						ev->value = 0;
						te.event = std::move(ev);
					}
					if (bitsChanged(0x80)) {
						track = 0;
						auto& trackEvents = pattern.at(track);
						trackEvents.emplace_back();
						auto& te = trackEvents.back();
						te.delay = this->lastDelay[track];
						this->lastDelay[track] = 0;
						auto ev = std::make_shared<ConfigurationEvent>();
						ev->configType = ConfigurationEvent::Type::EnableDeepTremolo;
						ev->value = (oplev.val & 0x80) ? 1 : 0; // bit0 is enable/disable
						if (oplev.chipIndex) ev->value |= 2;    // bit1 is chip index
						te.event = std::move(ev);
					}
					if (bitsChanged(0x40)) {
						track = 0;
						auto& trackEvents = pattern.at(track);
						trackEvents.emplace_back();
						auto& te = trackEvents.back();
						te.delay = this->lastDelay[track];
						this->lastDelay[track] = 0;
						auto ev = std::make_shared<ConfigurationEvent>();
						ev->configType = ConfigurationEvent::Type::EnableDeepVibrato;
						ev->value = (oplev.val & 0x40) ? 1 : 0; // bit0 is enable/disable
						if (oplev.chipIndex) ev->value |= 2;    // bit1 is chip index
						te.event = std::move(ev);
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
							this->createOrUpdatePitchbend(pattern.at(track),
								&this->lastDelay[track],
								oplState[oplev.chipIndex][0xA0 | oplChannel], oplev.val);
						}
					} else { // normal instrument
						if (bitsChanged(OPLBIT_KEYON)) {
							if (oplev.val & OPLBIT_KEYON) {
								// Note is now on
								this->createNoteOn(pattern.at(track), *music->patches,
									&this->lastDelay[track], oplev.chipIndex, oplChannel,
									OPLPatch::Rhythm::Melodic, oplev.val);
							} else {
								// Note is now off
								this->createNoteOff(track, pattern.at(track));
							}
						} else if (noteon && bitsChanged(0x1F)) {
							// The note is already on and the pitch has changed.
							this->createOrUpdatePitchbend(pattern.at(track), &this->lastDelay[track],
								oplState[oplev.chipIndex][0xA0 | oplChannel], oplev.val);
						}
					}
					break;
			} // switch (OPL reg)
		} // if (OPLEvent::Regs)

		/// @todo If the instrument settings have changed, generate a patch change event?
		// Will have to combine with previous patch change event if there has been no delay.

		if (oplev.valid & OPLEvent::Delay) {
			if (this->delayType == DelayType::DelayIsPostData) {
				for (unsigned int t = 0; t < OPL_TRACK_COUNT; t++) {
					this->lastDelay[t] += oplev.delay;
				}
			}
		}

	} // while (all events)

	// Put dummy events if necessary to preserve trailing delays
	for (unsigned int track = 0; track < OPL_TRACK_COUNT; track++) {
		auto& trackEvents = pattern.at(track);
		if (this->lastDelay[track] && trackEvents.size()) {
			trackEvents.emplace_back();
			auto& te = trackEvents.back();
			te.delay = this->lastDelay[track];
			this->lastDelay[track] = 0;
			auto ev = std::make_shared<ConfigurationEvent>();
			ev->configType = ConfigurationEvent::Type::EmptyEvent;
			ev->value = 0;
			te.event = std::move(ev);
		}
	}

	music->ticksPerTrack = totalDelay;

	return std::move(music);
}

std::shared_ptr<OPLPatch> OPLDecoder::getCurrentPatch(int chipIndex, int oplChannel)
{
	auto curPatch = std::make_shared<OPLPatch>();

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

	curPatch->feedback   = (this->oplState[chipIndex][BASE_FEED_CONN | oplChannel] >> 1) & 0x07;
	curPatch->connection =  this->oplState[chipIndex][BASE_FEED_CONN | oplChannel] & 1;
	curPatch->rhythm     = OPLPatch::Rhythm::Melodic; // will be overridden later if needed
	return curPatch;
}

int OPLDecoder::savePatch(PatchBank& patches, std::shared_ptr<OPLPatch> curPatch)
{
	// See if the patch has already been saved
	unsigned int j = 0;
	for (auto& p : patches) {
		auto oplPatch = dynamic_cast<OPLPatch*>(p.get());
		if (
			(oplPatch)
			&& (*oplPatch == *curPatch)
			&& (oplPatch->rhythm == curPatch->rhythm)
		) return j; // patch already saved
		j++;
	}

	// Save the patch
	unsigned int numPatches = patches.size();
	patches.push_back(curPatch);
	return numPatches;
}

void OPLDecoder::createNoteOn(Track& trackEvents, PatchBank& patches,
	unsigned long *lastDelay, unsigned int chipIndex, unsigned int oplChannel,
	OPLPatch::Rhythm rhythm, unsigned int b0val)
{
	trackEvents.emplace_back();
	auto& te = trackEvents.back();
	te.delay = *lastDelay;
	*lastDelay = 0;
	auto ev = std::make_shared<NoteOnEvent>();

	auto curPatch = this->getCurrentPatch(chipIndex, oplChannel);
	curPatch->rhythm = rhythm;

	/// Make sure the patch has been added to the patchbank
	ev->instrument = this->savePatch(patches, curPatch);

	// Get the OPL frequency number for this channel
	int fnum = ((b0val & 0x03) << 8) | this->oplState[chipIndex][0xA0 | oplChannel];
	int block = (b0val >> 2) & 0x07;

	ev->milliHertz = fnumToMilliHertz(fnum, block, this->fnumConversion);

	// Ignore velocity for modulator-only rhythm instruments
	if (oplModOnly(rhythm)) {
		ev->velocity = DefaultVelocity;
	} else {
		unsigned int curVol = 0x3F &
			this->oplState[chipIndex][BASE_SCAL_LEVL | OPLOFFSET_CAR(oplChannel)];
		ev->velocity = log_volume_to_lin_velocity(63 - curVol, 63);
	}

	te.event = std::move(ev);
	return;
}

void OPLDecoder::createNoteOff(unsigned int track, Track& trackEvents)
{
	// Create the note-off event
	trackEvents.emplace_back();
	auto& te = trackEvents.back();
	te.delay = this->lastDelay[track];
	this->lastDelay[track] = 0;
	te.event = std::make_shared<NoteOffEvent>();
	return;
}

void OPLDecoder::createOrUpdatePitchbend(Track& trackEvents,
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
			i = trackEvents.rbegin(); i != trackEvents.rend(); i++
		) {
			const TrackEvent& te = *i;
			if (te.delay != 0) break; // no more events at this time
			auto pbev = dynamic_cast<EffectEvent *>(i->event.get());
			if (pbev && (pbev->type == EffectEvent::Type::PitchbendNote)) {
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
		trackEvents.emplace_back();
		auto& te = trackEvents.back();
		te.delay = *lastDelay;
		*lastDelay = 0;
		auto ev = std::make_shared<EffectEvent>();
		ev->type = EffectEvent::Type::PitchbendNote;
		ev->data = freq;
		te.event = std::move(ev);
	}
	return;
}
