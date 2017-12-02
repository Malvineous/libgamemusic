/**
 * @file  mus-mus-vinyl.cpp
 * @brief Format handler for Vinyl Goddess From Mars .mus files.
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

#include <iostream>
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/util-opl.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include "decode-midi.hpp"
#include "encode-midi.hpp"
#include "patch-adlib.hpp"
#include "mus-mus-vinyl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Snare+hi-hat are this many semitones above the last tom-tom note
const unsigned int SNARE_PERC_OFFSET = 7;

/// Initial tom-tom note until changed.  Two octaves below middle-C.
const unsigned int DEFAULT_TOM_FREQ = midiMiddleC - 24;

/// Length of the title field, in bytes.
const unsigned int MUS_TITLE_LEN = 30;

std::string MusicType_MUS_Vinyl::code() const
{
	return "mus-vinyl";
}

std::string MusicType_MUS_Vinyl::friendlyName() const
{
	return "Vinyl Goddess From Mars Music File";
}

std::vector<std::string> MusicType_MUS_Vinyl::fileExtensions() const
{
	return {
		"mus",
	};
}

MusicType::Caps MusicType_MUS_Vinyl::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
	;
}

MusicType::Certainty MusicType_MUS_Vinyl::isInstance(stream::input& content)
	const
{
	stream::len lenFile = content.size();
	// File too short
	// TESTED BY: mus_mus_vinyl_isinstance_c02
	if (lenFile < 0x2A + 4) return Certainty::DefinitelyNo;

	uint8_t majorVersion, minorVersion;
	content.seekg(0, stream::start);
	content
		>> u8(majorVersion)
		>> u8(minorVersion)
	;
	// Unknown version
	if ((majorVersion != 1) || (minorVersion != 0)) return Certainty::DefinitelyNo;

	// Make sure the signature matches
	// TESTED BY: mus_mus_vinyl_isinstance_c01
	uint32_t lenNotes;
	content.seekg(0x2A, stream::start);
	content >> u32le(lenNotes);
	if (lenNotes + 0x46 != lenFile) return Certainty::DefinitelyNo;

	// TESTED BY: mus_mus_vinyl_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_MUS_Vinyl::read(stream::input& content,
	SuppData& suppData) const
{
	assert(suppData.find(SuppItem::Instruments) != suppData.end());

	uint8_t majorVersion, minorVersion, tickBeat, beatMeasure, soundMode;
	uint8_t pitchBRange;
	uint32_t tuneId, totalTick, dataSize, nrCommand;
	uint16_t basicTempo;
	std::string title;

	content.seekg(0, stream::start);
	content
		>> u8(majorVersion)
		>> u8(minorVersion)
		>> u32le(tuneId)
		>> nullPadded(title, MUS_TITLE_LEN)
		>> u8(tickBeat)
		>> u8(beatMeasure)
		>> u32le(totalTick)
		>> u32le(dataSize)
		>> u32le(nrCommand)
	;
	content.seekg(8, stream::cur); // skip over filler
	content
		>> u8(soundMode)
		>> u8(pitchBRange)
		>> u16le(basicTempo)
	;
	content.seekg(8, stream::cur); // skip over filler2

	Tempo initialTempo;
	initialTempo.beatsPerBar = beatMeasure;
	initialTempo.ticksPerBeat = tickBeat;
	initialTempo.bpm(basicTempo);

	auto music = midiDecode(content,
		MIDIFlags::ShortAftertouch
		| MIDIFlags::Channel10NoPerc
		| MIDIFlags::AdLibMUS,
		initialTempo);

	// Populate metadata
	{
		auto& a = music->addAttribute();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = MUS_TITLE_LEN;
		a.textValue = title;
	}

	// Set standard settings
	auto& configTrack = music->patterns.at(0).at(0);
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableOPL3;
		ev->value = 0;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 0, te);
	}
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableDeepTremolo;
		ev->value = 0;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 1, te);
	}
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableDeepVibrato;
		ev->value = 0;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 2, te);
	}
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableWaveSel;
		ev->value = 1;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 3, te);
	}
	{
		auto ev = std::make_shared<ConfigurationEvent>();
		ev->configType = ConfigurationEvent::Type::EnableRhythm;
		ev->value = (soundMode == 0) ? 0 : 1;

		TrackEvent te;
		te.delay = 0;
		te.event = ev;
		configTrack.insert(configTrack.begin() + 4, te);
	}

	uint16_t numInstruments, offInstData;
	auto& ins = suppData[SuppItem::Instruments];
	ins->seekg(2, stream::start);
	*ins
		>> u16le(numInstruments)
		>> u16le(offInstData)
	;

	// Read the instruments
	auto oplBank = std::make_shared<PatchBank>();
	oplBank->reserve(numInstruments);
	ins->seekg(offInstData, stream::start);
	for (unsigned int i = 0; i < numInstruments; i++) {
		auto oplPatch = std::make_shared<OPLPatch>();
		*ins >> adlibPatch<uint16_t>(*oplPatch);

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
			oplPatch->rhythm = OPLPatch::Rhythm::Melodic;
		}
		oplBank->push_back(oplPatch);
	}


	auto& pattern = music->patterns[0];

	double lastPercNote = DEFAULT_TOM_FREQ;
	// For each track
	unsigned int trackIndex = 0;
	for (auto& pt : pattern) {
		auto& ti = music->trackInfo[trackIndex];
		const unsigned int& oplChannel = ti.channelIndex;

		OPLPatch::Rhythm rhythm = OPLPatch::Rhythm::Melodic;
		if ((soundMode != 0) && (oplChannel > 5)) {
			// Percussive mode
			ti.channelType = TrackInfo::ChannelType::OPLPerc;
			ti.channelIndex = 4 - (oplChannel - 6); // 4=bd, 3=snare, ...

			// Convert 'rhythm' from channel index into perc instrument type
			rhythm = (OPLPatch::Rhythm)(ti.channelIndex + 1);
		} else {
			// Melodic mode or melodic instruments in percussive mode
			ti.channelType = TrackInfo::ChannelType::OPL;
			// ti.channelIndex = unchanged, leave as source MIDI channel
		}

		// For each event in the track
		for (auto& te : pt) {
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
			auto midiPatch = dynamic_cast<MIDIPatch*>(
				music->patches->at(ev->instrument).get());
			const uint8_t& oplInstrument = midiPatch->midiPatch;

			if (oplInstrument >= oplBank->size()) {
				std::cerr << "[mus-vinyl] Warning: Song tried to set MIDI patch "
					<< (int)oplInstrument << " but there are only " << oplBank->size()
					<< " OPL patches" << std::endl;
				continue;
			}
			auto oplPatch = dynamic_cast<OPLPatch*>(oplBank->at(oplInstrument).get());
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
					case OPLPatch::Rhythm::TomTom:
						// When a note is played on the tom-tom channel, it changes the
						// frequency of all single-operator percussion notes.
						lastPercNote = freqToMIDI(ev->milliHertz);
						// TODO: We need to immediately update the pitch of any other
						// percussive notes currently playing.
						break;
					case OPLPatch::Rhythm::TopCymbal:
						ev->milliHertz = midiToFreq(lastPercNote);
						break;
					case OPLPatch::Rhythm::SnareDrum:
					case OPLPatch::Rhythm::HiHat:
						// This is 142mHz in crush.mus, which definitely isn't two octaves
						// below middle-C!
						ev->milliHertz = midiToFreq(lastPercNote + SNARE_PERC_OFFSET);
						break;
					case OPLPatch::Rhythm::Unknown:
					case OPLPatch::Rhythm::Melodic:
					case OPLPatch::Rhythm::BassDrum:
						break;
				}
			}
		}
		trackIndex++;
	}

	// Disregard the MIDI patches and use the OPL ones
	music->patches = oplBank;

	return music;
}

void MusicType_MUS_Vinyl::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	throw stream::error("Writing VGFM music files is not yet implemented.");
	return;
}

SuppFilenames MusicType_MUS_Vinyl::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	SuppFilenames supps;
	std::string filenameBase =
		filename.substr(0, filename.find_last_of('.'));
	supps[SuppItem::Instruments] = filenameBase + ".tim";
	return supps;
}

std::vector<Attribute> MusicType_MUS_Vinyl::supportedAttributes() const
{
	std::vector<Attribute> items;
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = MUS_TITLE_LEN;
	}
	return items;
}
