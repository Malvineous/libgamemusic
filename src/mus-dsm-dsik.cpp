/**
 * @file  mus-dsm-dsik.cpp
 * @brief Support for the Digital Sound Interface Kit DSMF format.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/DSIK_Module_Format
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

#include <camoto/iostream_helpers.hpp>
#include <camoto/util.hpp>
#include <camoto/iff.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp> // freqToMIDI
#include "mus-dsm-dsik.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define DSM_CHANNEL_COUNT 16 ///< Number of storage channels in DSM file

/// Maximum number of bytes needed to store one complete packed pattern
#define DSM_MAX_PACKED_PATTERN_SIZE (64*(32*6+1))

/// Number of rows in every pattern
#define DSM_ROWS_PER_PATTERN 64

/// Length of title field, in bytes
#define DSMF_TITLE_LEN 28

/// Calculate number of bytes to add to len to bring it up to a parapointer
/// boundary (multiple of 16.)
#define PP_PAD(len) ((16 - (len % 16)) % 16)

#define FOURCC_RIFF "RIFF"
#define FOURCC_DSMF "DSMF"

class EventConverter_DSM: virtual public EventHandler
{
	public:
		/// Prepare to convert events into DSM data sent to a stream.
		/**
		 * @param iff
		 *   IFF interface to content.
		 *
		 * @param content
		 *   Output stream the DSM data will be written to.
		 *
		 * @param patches
		 *   Patch bank to write.
		 */
		EventConverter_DSM(IFFWriter *iff, stream::output& content,
			const PatchBank& patches);

		/// Destructor.
		virtual ~EventConverter_DSM();

		// EventHandler overrides
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const GotoEvent *ev);
		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

		std::vector<stream::len> lenPattern; ///< Offset of each pattern
		Tempo lastTempo; ///< Last tempo set by song (or initial tempo)

	protected:
		IFFWriter *iff;              ///< IFF helper class
		stream::output& content;     ///< Where to write data
		uint8_t patternBuffer[DSM_MAX_PACKED_PATTERN_SIZE]; ///< Store pattern before writing to file
		uint8_t *patBufPos;          ///< Current position in patternBuffer
		unsigned int curRow;         ///< Current row in pattern (0-63 inclusive)
		const PatchBank& patches;  ///< Copy of the patches for the song

		/// Write out the current delay
		void writeDelay(unsigned long curTick);
};

std::string MusicType_DSM::code() const
{
	return "dsm-dsik";
}

std::string MusicType_DSM::friendlyName() const
{
	return "Digital Sound Interface Kit Module";
}

std::vector<std::string> MusicType_DSM::fileExtensions() const
{
	return {
		"dsm",
	};
}

MusicType::Caps MusicType_DSM::caps() const
{
	return
		Caps::InstPCM
		| Caps::HasEvents
		| Caps::HasPatterns
	;
}

MusicType::Certainty MusicType_DSM::isInstance(stream::input& content) const
{
	content.seekg(0, stream::start);
	IFF::fourcc sig1, sig2;
	uint32_t len1;
	content
		>> fixedLength(sig1, 4)
		>> u32le(len1)
		>> fixedLength(sig2, 4)
	;

	// Invalid RIFF signature
	// TESTED BY: mus_dsm_dsik_isinstance_c01
	if (sig1.compare(FOURCC_RIFF) != 0) return Certainty::DefinitelyNo;

	// Invalid DSMF signature
	// TESTED BY: mus_dsm_dsik_isinstance_c02
	if (sig2.compare(FOURCC_DSMF) != 0) return Certainty::DefinitelyNo;

	// File truncated
	// TESTED BY: mus_dsm_dsik_isinstance_c03
	if (len1 + 8 != content.size()) return Certainty::DefinitelyNo;

	// TESTED BY: mus_dsm_dsik_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_DSM::read(stream::input& content,
	SuppData& suppData) const
{
	auto music = std::make_unique<Music>();
	music->patches = std::make_shared<PatchBank>();

	// All DSM files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 4;
	music->ticksPerTrack = 64;
	music->loopDest = -1; // no loop

	IFFReader iff(content, IFF::Filetype_RIFF_Unpadded);
	IFF::fourcc type;
	iff.open(FOURCC_RIFF, &type);
	if (type.compare(FOURCC_DSMF) != 0) {
		throw camoto::error("This is not a DSMF file.");
	}

	for (unsigned int i = 0; i < DSM_CHANNEL_COUNT; i++) {
		TrackInfo t;
		t.channelType = TrackInfo::ChannelType::PCM;
		t.channelIndex = i;
		music->trackInfo.push_back(t);
	}

	Tempo lastTempo = music->initialTempo;

	auto& attrTitle = music->addAttribute();
	attrTitle.changed = false;
	attrTitle.type = Attribute::Type::Text;
	attrTitle.name = CAMOTO_ATTRIBUTE_TITLE;
	attrTitle.desc = "Song title";
	attrTitle.textMaxLength = DSMF_TITLE_LEN;

	std::vector<IFF::fourcc> chunks = iff.list();
	int j = 0;
	for (auto& i : chunks) {
		stream::len lenChunk = iff.seek(j);
		if (i.compare("SONG") == 0) {
			// Load song
			uint8_t volGlobal, volMaster, initialSpeed, initialBPM;
			uint16_t version, flags, orderCount, instrumentCount, patternCount;
			uint16_t channelCount;
			uint32_t pad;
			content
				>> nullPadded(attrTitle.textValue, DSMF_TITLE_LEN)
				>> u16le(version)
				>> u16le(flags)
				>> u32le(pad)
				>> u16le(orderCount)
				>> u16le(instrumentCount)
				>> u16le(patternCount)
				>> u16le(channelCount)
				>> u8(volGlobal)
				>> u8(volMaster)
				>> u8(initialSpeed)
				>> u8(initialBPM)
			;
			if (initialSpeed == 0) initialSpeed = 6;
			if (initialBPM == 0) initialBPM = 125;
			music->initialTempo.module(initialSpeed, initialBPM);
			music->patterns.reserve(patternCount);

			uint8_t channelMap[16], orders[128];
			content.read(channelMap, 16);
			content.read(orders, 128);

			/// @todo: Process pan values from channel map

			uint8_t *order = &orders[0];
			if (orderCount > 128) orderCount = 128;
			music->patternOrder.reserve(orderCount);
			for (unsigned int i = 0; i < orderCount; i++) {
				if (*order < 0xFE) {
					music->patternOrder.push_back(*order);
				} else if (*order == 0xFE) {
					std::cout << "DSM TODO: Don't ignore marker pattern\n";
				}
				order++;
			}

		} else if (i.compare("INST") == 0) {
			auto p = std::make_shared<PCMPatch>();
			uint8_t volDefault;
			uint16_t flags, period;
			uint32_t address;
			std::string filename;
			unsigned long lenData;
			content
				>> nullPadded(filename, 13)
				>> u16le(flags)
				>> u8(volDefault)
				>> u32le(lenData)
				>> u32le(p->loopStart)
				>> u32le(p->loopEnd)
				>> u32le(address)
				>> u16le(p->sampleRate)
				>> u16le(period)
				>> nullPadded(p->name, 28)
			;
			if (flags & 4) {
				throw stream::error("Unsupported sample compression - please report "
					"this problem!");
			}
			if (!(flags & 1)) {
				// Loop off
				p->loopStart = 0;
				p->loopEnd = 0;
			}

			p->numChannels = 1;
			p->bitDepth = 8;
			if (volDefault > 63) p->defaultVolume = 255;
			else p->defaultVolume = (volDefault << 2) | (volDefault >> 4);

			// Read the PCM data
			p->data = std::vector<uint8_t>(lenData);
			content.read(p->data.data(), p->data.size());

			// Convert from signed 8-bit to unsigned
			if (flags & 2) {
				for (auto& i : p->data) {
					i = static_cast<uint8_t>(
						(int8_t)(i) + 128
					);
				}
			}
			music->patches->push_back(p);

		} else if (i.compare("PATT") == 0) {
			// Skip redundant length field
			content.seekg(2, stream::cur);

			music->patterns.emplace_back();
			auto& pattern = music->patterns.back();

			unsigned int lastRow[DSM_CHANNEL_COUNT];
			unsigned int lastInstrument[DSM_CHANNEL_COUNT];
			for (unsigned int track = 0; track < DSM_CHANNEL_COUNT; track++) {
				lastRow[track] = 0;
				lastInstrument[track] = 0;

				pattern.emplace_back();
			}

			stream::len lenRead = 0;
			for (unsigned int row = 0; row < DSM_ROWS_PER_PATTERN; row++) {
				if (lenRead >= lenChunk) {
					std::cerr << "DSM: Truncated pattern (chunk ended before row " << row
						<< ")\n";
					break;
				}
				uint8_t what;
				do {
					content >> u8(what);
					lenRead++;
					unsigned int channel = what & 0x0F;
					auto& track = pattern.at(channel);
					uint8_t note = 0, instrument = 0, volume = 255, command = 0, info = 0;
					if (what & 0x80) {
						content >> u8(note);
						lenRead++;
					}
					if (what & 0x40) {
						content >> u8(instrument);
						lenRead++;
					}
					if (what & 0x20) {
						content >> u8(volume);
						lenRead++;
					}
					if (what & 0x10) {
						content
							>> u8(command)
							>> u8(info)
						;
						lenRead += 2;
					}
					if (what & 0x80) {
						if (note == 255) {
							// No note?
						} else if (note == 254) {
							// Note off
							auto ev = std::make_shared<NoteOffEvent>();

							TrackEvent te;
							te.delay = row - lastRow[channel];
							te.event = ev;
							track.push_back(te);

							lastRow[channel] = row;
						} else {
							auto ev = std::make_shared<NoteOnEvent>();

							if (instrument == 0) instrument = lastInstrument[channel];
							else {
								instrument--;
								lastInstrument[channel] = instrument;
							}
							ev->instrument = lastInstrument[channel];

							ev->milliHertz = midiToFreq(note + 11);

							if ((what & 0x20) && (volume < 65)) {
								if (volume == 64) ev->velocity = 255;
								else ev->velocity = (volume << 2) | (volume >> 4);
							} else { // missing or =255
								ev->velocity = DefaultVelocity; // instrument default
							}

							TrackEvent te;
							te.delay = row - lastRow[channel];
							te.event = ev;
							track.push_back(te);

							lastRow[channel] = row;
						}
					} else if (what & 0x20) { // 0x80 unset because of 'else'
						// Volume change
						auto ev = std::make_shared<EffectEvent>();
						ev->type = EffectEvent::Type::Volume;
						if (volume == 64) ev->data = 255;
						else ev->data = (volume << 2) | (volume >> 4);

						TrackEvent te;
						te.delay = row - lastRow[channel];
						te.event = ev;
						track.push_back(te);

						lastRow[channel] = row;
					}
					if (what & 0x10) {
						switch (command) {
							case 0x0c: { // volume change
								// Volume change
								auto ev = std::make_shared<EffectEvent>();
								ev->type = EffectEvent::Type::Volume;
								if (info >= 64) ev->data = 255;
								else ev->data = (info << 2) | (info >> 4);

								TrackEvent te;
								te.delay = row - lastRow[channel];
								te.event = ev;
								track.push_back(te);

								lastRow[channel] = row;
								break;
							}
							case 0x0f: { // set speed
								if (info == 0) break; // ignore
								auto ev = std::make_shared<TempoEvent>();
								ev->tempo = lastTempo;
								if (info < 32) {
									ev->tempo.module(info, lastTempo.module_tempo());
								} else {
									ev->tempo.module(lastTempo.module_speed(), info);
								}

								TrackEvent te;
								te.delay = row - lastRow[channel];
								te.event = ev;
								track.push_back(te);

								lastRow[channel] = row;
								lastTempo = ev->tempo;
								break;
							}
							default:
								std::cout << "DSM: Disregarding unimplemented effect 0x" << std::hex
									<< (int)command << ":" << (int)info << std::dec << "\n";
								break;
						}
					}
				} while (what);
			}

		} else {
			std::cout << "DSM: Skipping unknown chunk type " << i << "\n";
		}
		j++;
	}

	return music;
}

void MusicType_DSM::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	if (music.trackInfo.size() > DSM_CHANNEL_COUNT) {
		throw format_limitation("This format has a maximum of 16 channels.");
	}

	if (music.patternOrder.size() > 128) {
		throw format_limitation("This format has a maximum of 128 entries in the "
			"order list.");
	}

	unsigned long tempo = music.initialTempo.module_tempo();
	if (tempo > 255) {
		throw stream::error(createString("Tempo is too fast for DSM file!  "
			"Calculated value is " << tempo << " but max permitted value is 255."));
	}

	IFFWriter iff(content, IFF::Filetype_RIFF_Unpadded);
	iff.begin(FOURCC_RIFF, FOURCC_DSMF);

	uint8_t speed = music.initialTempo.module_speed();
	iff.begin("SONG");
	content
		<< nullPadded(music.attributes().at(0).textValue, 28)
		<< u16le(0) // version
		<< u16le(0) // flags
		<< u32le(0) // pad
		<< u16le(music.patternOrder.size())
		<< u16le(music.patches->size())
		<< u16le(music.patterns.size())
		<< u16le(music.trackInfo.size())
		<< u8(63) // global volume
		<< u8(63) // master volume
		<< u8(speed)
		<< u8(tempo)
	;
	// Default panning
	for (unsigned int i = 0; i < DSM_CHANNEL_COUNT; i++) {
		if (i % 2) {
			content << u8(0x80);
		} else {
			content << u8(0);
		}
	}
	uint8_t orders[128];
	memset(orders, 0xFF, sizeof(orders));
	unsigned int j = 0;
	for (auto& i : music.patternOrder) {
		orders[j++] = i;
	}
	content.write(orders, 128);
	iff.end(); // SONG

	for (auto& i : *music.patches) {
		iff.begin("INST");

		auto p = dynamic_cast<PCMPatch*>(i.get());
		unsigned int flags = 0;
		if (p->loopEnd != 0) flags |= 1;
		unsigned int period = 8363 * 428 / p->sampleRate;
		uint8_t defVolume = p->defaultVolume >> 2; // 0..255 -> 0..63
		content
			<< nullPadded(std::string(), 13)
			<< u16le(flags)
			<< u8(defVolume)
			<< u32le(p->data.size())
			<< u32le(p->loopStart)
			<< u32le(p->loopEnd)
			<< u32le(0) // address pointer
			<< u16le(p->sampleRate)
			<< u16le(period)
			<< nullPadded(p->name, 28)
		;
		content.write(p->data.data(), p->data.size());

		iff.end(); // INST
	}

	// Write out the patterns
	EventConverter_DSM conv(&iff, content, *music.patches);
	conv.lastTempo = music.initialTempo;
	conv.handleAllEvents(EventHandler::Pattern_Row_Track, music, 1);

	iff.end(); // RIFF

	// Set final filesize to this
	content.truncate_here();
	return;
}

SuppFilenames MusicType_DSM::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_DSM::supportedAttributes() const
{
	std::vector<Attribute> items;
	{
		items.emplace_back();
		auto& a = items.back();
		a.changed = false;
		a.type = Attribute::Type::Text;
		a.name = CAMOTO_ATTRIBUTE_TITLE;
		a.desc = "Song title";
		a.textMaxLength = DSMF_TITLE_LEN;
	}
	return items;
}


EventConverter_DSM::EventConverter_DSM(IFFWriter *iff, stream::output& content,
	const PatchBank& patches)
	:	iff(iff),
		content(content),
		patBufPos(patternBuffer),
		curRow(0),
		patches(patches)
{
}

EventConverter_DSM::~EventConverter_DSM()
{
}

void EventConverter_DSM::endOfTrack(unsigned long delay)
{
	return;
}

void EventConverter_DSM::endOfPattern(unsigned long delay)
{
	this->writeDelay(delay);
	this->writeDelay(64 - this->curRow);
	if (this->curRow != 64) {
		throw stream::error(createString("Tried to write an DSM pattern with "
			<< this->curRow << " rows, but this format only supports 64 rows per "
			"pattern."));
	}

	// Write out the pattern
	unsigned int lenUsed = this->patBufPos - this->patternBuffer;
	uint16_t valLengthField = lenUsed + 2; // include size of length field itself
	this->iff->begin("PATT");
	this->content << u16le(valLengthField);
	this->content.write(this->patternBuffer, lenUsed);
	this->iff->end(); // PATT
	this->patBufPos = this->patternBuffer;
	this->curRow = 0;
	return;
}

bool EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->writeDelay(delay);
	if (this->lastTempo.module_speed() != ev->tempo.module_speed()) {
		*this->patBufPos++ = (trackIndex & 0x1F) | 0x10; // effect
		*this->patBufPos++ = 0x0F; // set speed
		*this->patBufPos++ = ev->tempo.module_speed(); // new speed
	} else if (this->lastTempo.module_tempo() != ev->tempo.module_tempo()) {
		*this->patBufPos++ = (trackIndex & 0x1F) | 0x10; // effect
		*this->patBufPos++ = 0x0F; // set tempo
		*this->patBufPos++ = ev->tempo.module_tempo(); // new tempo
	}
	this->lastTempo = ev->tempo;
	return true;
}

bool EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	this->writeDelay(delay);
	uint8_t midiNote;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &midiNote, &bend, 0xFF);

	if (midiNote <= 11) {
		std::cerr << "DSM: Dropping note in octave -1.\n";
		return true;
	}

	if (ev->instrument >= this->patches.size()) {
		std::cerr << "DSM: Dropping note with out-of-range instrument #"
			<< ev->instrument << "\n";
		return true;
	}
	uint8_t velFlag = 0;
	if (
		(ev->velocity >= 0) &&
		((unsigned int)ev->velocity != this->patches.at(ev->instrument)->defaultVolume)
	) velFlag = 0x20;
	*this->patBufPos++ = (trackIndex & 0x1F) | 0xC0 | velFlag; // event with note+inst
	*this->patBufPos++ = midiNote - 11;
	*this->patBufPos++ = ev->instrument + 1; // +1 because 0 means no/prev instrument
	if (velFlag) *this->patBufPos++ = ev->velocity >> 2;
	return true;
}

bool EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->writeDelay(delay);
	*this->patBufPos++ = (trackIndex & 0x1F) | 0x80; // event with note+inst
	*this->patBufPos++ = 0xFE; // note off
	*this->patBufPos++ = 0x00; // no instrument

	return true;
}

bool EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->type) {
		case EffectEvent::Type::PitchbendNote:
			//if (this->flags & IntegerNotesOnly) return true;
			std::cout << "DSM: TODO - implement pitch bends\n";
			break;
		case EffectEvent::Type::Volume:
			*this->patBufPos++ = (trackIndex & 0x1F) | 0x20; // event with volume only
			*this->patBufPos++ = ev->data >> 2; // volume value
			break;
	}
	return true;
}

bool EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const GotoEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->type) {
		case GotoEvent::Type::CurrentPattern:
			std::cout << "DSM: TODO - implement pattern breaks\n";
			break;
	}
	return true;
}

bool EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->configType) {
		case ConfigurationEvent::Type::EmptyEvent:
			break;
		case ConfigurationEvent::Type::EnableOPL3:
		case ConfigurationEvent::Type::EnableDeepTremolo:
		case ConfigurationEvent::Type::EnableDeepVibrato:
		case ConfigurationEvent::Type::EnableRhythm:
		case ConfigurationEvent::Type::EnableWaveSel:
			throw format_limitation("This format cannot store OPL events.");
	}
	return true;
}

void EventConverter_DSM::writeDelay(unsigned long delay)
{
	if (delay == 0) return; // no delay yet
	assert(delay < 1048576*1024); // safety catch

	if (this->curRow + delay > 64) {
		throw stream::error(createString("DSM: Tried to write pattern with more "
			"than 64 rows (next row is " << this->curRow + delay << ")."));
	}

	this->curRow += delay;
	while (delay--) {
		*this->patBufPos++ = 0x00; // finish current row
	}
	return;
}
