/**
 * @file   mus-mus-vinyl.cpp
 * @brief  Format handler for Vinyl Goddess From Mars .mus files.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/AdLib_MIDI_Format
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

#include <boost/pointer_cast.hpp>
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/util-opl.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "mus-mus-vinyl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Snare+hi-hat are this many semitones above the last tom-tom note
const unsigned int SNARE_PERC_OFFSET = 7;

/// Initial tom-tom note until changed.  Two octaves below middle-C.
const unsigned int DEFAULT_TOM_FREQ = midiMiddleC - 24;

std::string MusicType_MUS_Vinyl::getCode() const
{
	return "mus-vinyl";
}

std::string MusicType_MUS_Vinyl::getFriendlyName() const
{
	return "Vinyl Goddess From Mars Music File";
}

std::vector<std::string> MusicType_MUS_Vinyl::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("mus");
	return vcExtensions;
}

unsigned long MusicType_MUS_Vinyl::getCaps() const
{
	return
		InstOPL
		| HasEvents
	;
}

MusicType::Certainty MusicType_MUS_Vinyl::isInstance(stream::input_sptr psMusic) const
{
	stream::len lenFile = psMusic->size();
	// File too short
	// TESTED BY: mus_mus_vinyl_isinstance_c02
	if (lenFile < 0x2A + 4) return MusicType::DefinitelyNo;

	uint8_t majorVersion, minorVersion;
	psMusic->seekg(0, stream::start);
	psMusic
		>> u8(majorVersion)
		>> u8(minorVersion)
	;
	// Unknown version
	if ((majorVersion != 1) || (minorVersion != 0)) return MusicType::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_mus_vinyl_isinstance_c01
	uint32_t lenNotes;
	psMusic->seekg(0x2A, stream::start);
	psMusic >> u32le(lenNotes);
	if (lenNotes + 0x46 != lenFile) return MusicType::DefinitelyNo;

	// TESTED BY: mus_mus_vinyl_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_MUS_Vinyl::read(stream::input_sptr input, SuppData& suppData) const
{
	uint8_t majorVersion, minorVersion, tickBeat, beatMeasure, soundMode, pitchBRange;
	uint32_t tuneId, totalTick, dataSize, nrCommand;
	uint16_t basicTempo;
	std::string title;
	input->seekg(0, stream::start);
	input
		>> u8(majorVersion)
		>> u8(minorVersion)
		>> u32le(tuneId)
		>> nullPadded(title, 30)
		>> u8(tickBeat)
		>> u8(beatMeasure)
		>> u32le(totalTick)
		>> u32le(dataSize)
		>> u32le(nrCommand)
	;
	input->seekg(8, stream::cur); // skip over filler
	input
		>> u8(soundMode)
		>> u8(pitchBRange)
		>> u16le(basicTempo)
	;
	input->seekg(8, stream::cur); // skip over filler2

	Tempo initialTempo;
	initialTempo.beatsPerBar = beatMeasure;
	initialTempo.ticksPerBeat = tickBeat;
	initialTempo.bpm(basicTempo);
	MusicPtr music = midiDecode(input,
		MIDIFlags::ShortAftertouch
		| MIDIFlags::Channel10NoPerc
		| MIDIFlags::AdLibMUS,
		initialTempo);

	// Set standard settings
	TrackPtr configTrack = music->patterns.at(0)->at(0);
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableOPL3;
		ev->value = 0;
		configTrack->insert(configTrack->begin() + 0, te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableDeepTremolo;
		ev->value = 0;
		configTrack->insert(configTrack->begin() + 1, te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableDeepVibrato;
		ev->value = 0;
		configTrack->insert(configTrack->begin() + 2, te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableWaveSel;
		ev->value = 1;
		configTrack->insert(configTrack->begin() + 3, te);
	}
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableRhythm;
		ev->value = (soundMode == 0) ? 0 : 1;
		configTrack->insert(configTrack->begin() + 4, te);
	}

	uint16_t numInstruments, offInstData;
	stream::input_sptr ins = suppData[SuppItem::Instruments];
	ins->seekg(2, stream::start);
	ins
		>> u16le(numInstruments)
		>> u16le(offInstData)
	;

	// Read the instruments
	PatchBankPtr oplBank(new PatchBank());
	oplBank->reserve(numInstruments);
	ins->seekg(offInstData, stream::start);
	for (unsigned int i = 0; i < numInstruments; i++) {
		OPLPatchPtr oplPatch(new OPLPatch());
		uint16_t inst[13*2+2];
		ins->read((uint8_t *)&inst[0], sizeof(inst));

#define INS_KSL      inst[op * 13 + 0]
#define INS_MULTIPLE inst[op * 13 + 1]
#define INS_FEEDBACK inst[op * 13 + 2]
#define INS_ATTACK   inst[op * 13 + 3]
#define INS_SUSTAIN  inst[op * 13 + 4]
#define INS_EG       inst[op * 13 + 5]
#define INS_DECAY    inst[op * 13 + 6]
#define INS_RELEASE  inst[op * 13 + 7]
#define INS_LEVEL    inst[op * 13 + 8]
#define INS_AM       inst[op * 13 + 9]
#define INS_VIB      inst[op * 13 + 10]
#define INS_KSR      inst[op * 13 + 11]
#define INS_CON      inst[op * 13 + 12]
#define INS_WAVESEL  inst[26 + op]
		OPLOperator *o = &oplPatch->m;
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = le16toh(INS_AM) ? 1 : 0;
			o->enableVibrato = le16toh(INS_VIB) ? 1 : 0;
			o->enableSustain = le16toh(INS_EG) ? 1 : 0;
			o->enableKSR     = le16toh(INS_KSR) ? 1 : 0;
			o->freqMult      = le16toh(INS_MULTIPLE) & 0x0f;
			o->scaleLevel    = le16toh(INS_KSL) & 0x03;
			o->outputLevel   = le16toh(INS_LEVEL) & 0x3f;
			o->attackRate    = le16toh(INS_ATTACK) & 0x0f;
			o->decayRate     = le16toh(INS_DECAY) & 0x0f;
			o->sustainRate   = le16toh(INS_SUSTAIN) & 0x0f;
			o->releaseRate   = le16toh(INS_RELEASE) & 0x0f;
			o->waveSelect    = le16toh(INS_WAVESEL) & 0x07;
			o = &oplPatch->c;
		}
		// The instruments store both a Modulator and a Carrier value for the
		// Feedback and Connection, but the OPL only uses one value for each
		// Modulator+Carrier pair.  Both values often seem to be set the same,
		// however the official docs say to use op0 and ignore the op1 value.
		// same, so we can just pick the Modulator's value here.
		int op = 0;
		oplPatch->feedback   = le16toh(INS_FEEDBACK) & 7;
		oplPatch->connection = le16toh(INS_CON) ? 0 : 1;

		// The last five instruments are often percussion mode ones.  We'll set
		// them like this here as defaults, and then if we are wrong, they will
		// be changed below when we run through the note-on events.
		if ((signed int)i > (signed int)numInstruments - 6) {
			oplPatch->rhythm = (OPLPatch::Rhythm)(numInstruments - i);
			if (oplCarOnly(oplPatch->rhythm)) {
				// These two have their carrier settings stored in the modulator
				// fields, so swap them.
				std::swap(oplPatch->c, oplPatch->m);
			}
		} else {
			oplPatch->rhythm = OPLPatch::Melodic;
		}
		oplBank->push_back(oplPatch);
	}


	PatternPtr& pattern = music->patterns[0];

	double lastPercNote = DEFAULT_TOM_FREQ;
	// For each track
	unsigned int trackIndex = 0;
	for (Pattern::iterator
		pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++
	) {
		TrackInfo& ti = music->trackInfo[trackIndex];
		const unsigned int& oplChannel = ti.channelIndex;

		OPLPatch::Rhythm rhythm = OPLPatch::Melodic;
		if ((soundMode != 0) && (oplChannel > 5)) {
			// Percussive mode
			ti.channelType = TrackInfo::OPLPercChannel;
			ti.channelIndex = 4 - (oplChannel - 6); // 4=bd, 3=snare, ...

			// Convert 'rhythm' from channel index into perc instrument type
			rhythm = (OPLPatch::Rhythm)(ti.channelIndex + 1);
		} else {
			// Melodic mode or melodic instruments in percussive mode
			ti.channelType = TrackInfo::OPLChannel;
			// ti.channelIndex = unchanged, leave as source MIDI channel
		}

		// For each event in the track
		for (Track::iterator
			tev = (*pt)->begin(); tev != (*pt)->end(); tev++
		) {
			TrackEvent& te = *tev;
			NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
			if (!ev) continue;

			// If we are here, this is a note-on event

			// midiDecode() returned a MIDI patch bank, which is a list of MIDI patch
			// numbers used by the song.  For example if the first note in the song
			// used patch 5, then the bank would contain one entry with a MIDI patch
			// of 5.
			//
			// This means all notes that use MIDI patch 5 are set to instrument #0
			// because instrument #0 is the only entry in the returned MIDI bank
			// (and this single entry points to MIDI patch 5, remember.)
			//
			// However once we load the OPL instruments, that same event will point
			// to OPL instrument #0, whereas it needs to point to OPL instrument #5,
			// as if it was a MIDI patch.
			//
			// So we have to go through all the note-on events, find out what MIDI
			// patch they are using, then change the instrument number to be that of
			// the final MIDI patch, instead of an index into the MIDI bank.  Once
			// that's done we can discard the MIDI bank.
			if (ev->instrument > music->patches->size()) continue;
			MIDIPatchPtr midiPatch = boost::dynamic_pointer_cast<MIDIPatch>
				(music->patches->at(ev->instrument));
			const uint8_t& oplInstrument = midiPatch->midiPatch;

			if (oplInstrument >= oplBank->size()) {
				std::cerr << "[mus-vinyl] Warning: Song tried to set MIDI patch "
					<< oplInstrument << " but there are only " << oplBank->size()
					<< " OPL patches" << std::endl;
				continue;
			}
			OPLPatchPtr oplPatch = boost::dynamic_pointer_cast<OPLPatch>(oplBank->at(oplInstrument));
			// This should never fail as we never add any other type to oplBank above.
			assert(oplPatch);

			// Remap the instrument from an index into the patchbank back to the
			// original target MIDI patch number.
			ev->instrument = oplInstrument;
			ev->velocity = log_volume_to_lin_velocity(ev->velocity, 255);

			if (soundMode != 0) {
				// If this instrument is used on a rhythm channel, update that too
				if (oplPatch->rhythm != rhythm) {
					// This patch assignment is non-default, so correct it.

					// Undo the swap above, if we swapped operators there.
					if (oplCarOnly(oplPatch->rhythm)) {
						// These two have their carrier settings stored in the modulator
						// fields, so swap them.
						std::swap(oplPatch->c, oplPatch->m);
					}

					oplPatch->rhythm = rhythm;

					// Now swap again, if we need to
					if (oplCarOnly(oplPatch->rhythm)) {
						// These two have their carrier settings stored in the modulator
						// fields, so swap them.
						std::swap(oplPatch->c, oplPatch->m);
					}
				}

				switch (rhythm) {
					case OPLPatch::TomTom:
						// When a note is played on the tom-tom channel, it changes the
						// frequency of all single-operator percussion notes.
						lastPercNote = freqToMIDI(ev->milliHertz);
						// TODO: We need to immediately update the pitch of any other
						// percussive notes currently playing.
						break;
					case OPLPatch::TopCymbal:
						ev->milliHertz = midiToFreq(lastPercNote);
						break;
					case OPLPatch::SnareDrum:
					case OPLPatch::HiHat:
						// This is 142mHz in crush.mus, which definitely isn't two octaves
						// below middle-C!
						ev->milliHertz = midiToFreq(lastPercNote + SNARE_PERC_OFFSET);
						break;
					case OPLPatch::Unknown:
					case OPLPatch::Melodic:
					case OPLPatch::BassDrum:
						break;
				}
			}
		}
	}

	// Disregard the MIDI patches and use the OPL ones
	music->patches = oplBank;

	return music;
}

void MusicType_MUS_Vinyl::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	throw stream::error("Writing VGFM music files is not yet implemented.");
	return;
}

SuppFilenames MusicType_MUS_Vinyl::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	SuppFilenames supps;
	std::string filenameBase =
		filenameMusic.substr(0, filenameMusic.find_last_of('.'));
	supps[SuppItem::Instruments] = filenameBase + ".tim";
	return supps;
}

Metadata::MetadataTypes MusicType_MUS_Vinyl::getMetadataList() const
{
	Metadata::MetadataTypes types;
	return types;
}
