/**
 * @file   mus-mus-vinyl.cpp
 * @brief  Format handler for Vinyl Goddess From Mars .mus files.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/VGFM_Music_Format
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
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "mus-mus-vinyl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

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
	uint32_t lenNotes;
	input->seekg(0x2A, stream::start);
	input >> u32le(lenNotes);

	// Process the MIDI data
	input->seekg(0x46, stream::start);

	Tempo initialTempo;
	initialTempo.hertz(500);
	initialTempo.ticksPerQuarterNote(240);
	MusicPtr music = midiDecode(input, MIDIFlags::ShortAftertouch, initialTempo);

	// Set standard settings
	TrackPtr configTrack = music->patterns.at(0)->at(0);
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableOPL3;
		ev->value = 1;
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
		ev->value = 1;
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
		OPLPatchPtr patch(new OPLPatch());
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
		OPLOperator *o = &patch->m;
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
			o = &patch->c;
		}
		// The instruments store both a Modulator and a Carrier value for the
		// Feedback and Connection, but the OPL only uses one value for each
		// Modulator+Carrier pair.  Luckily both values often seem to be set the
		// same, so we can just pick the Modulator's value here.
		int op = 0;
		patch->feedback    = le16toh(INS_FEEDBACK) & 7;
		patch->connection  = le16toh(INS_CON) ? 0 : 1;
		// The last five instruments appear to be percussion mode ones
		if ((signed int)i > (signed int)numInstruments - 6) {
			patch->rhythm      = (OPLPatch::Rhythm)(numInstruments - i);
			if (oplCarOnly(patch->rhythm)) {
				// These two have their carrier settings stored in the modulator
				// fields, so swap them.
				std::swap(patch->c, patch->m);
			}
		} else {
			patch->rhythm      = OPLPatch::Melodic;
		}

		oplBank->push_back(patch);
	}

	// Disregard the MIDI patches and use the OPL ones
	music->patches = oplBank;


	PatternPtr& pattern = music->patterns[0];

	// For each track
	unsigned int trackIndex = 0;
	unsigned int nextChannel = 0;
	for (Pattern::iterator
		pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++
	) {
		TrackInfo& ti = music->trackInfo[trackIndex];

		// For each event in the track
		for (Track::iterator
			tev = (*pt)->begin(); tev != (*pt)->end(); tev++
		) {
			TrackEvent& te = *tev;
			NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
			if (!ev) continue;

			// If we are here, this is a note-on event
			if (ev->instrument > music->patches->size()) continue;

			OPLPatchPtr oplPatch = boost::dynamic_pointer_cast<OPLPatch>(music->patches->at(ev->instrument));
			if (!oplPatch) continue;

			if (oplPatch->rhythm) {
				ti.channelType = TrackInfo::OPLPercChannel;
				ti.channelIndex = oplPatch->rhythm - 1;
			} else {
				ti.channelType = TrackInfo::OPLChannel;
				ti.channelIndex = nextChannel++;
			}
			break;
		}
	}

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
