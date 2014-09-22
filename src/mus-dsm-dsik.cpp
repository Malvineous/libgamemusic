/**
 * @file   mus-dsm-dsik.cpp
 * @brief  Support for the Digital Sound Interface Kit DSMF format.
 *
 * This file format is fully documented on the ModdingWiki:
 *   http://www.shikadi.net/moddingwiki/DSIK_Module_Format
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
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

/// Calculate number of bytes to add to len to bring it up to a parapointer
/// boundary (multiple of 16.)
#define PP_PAD(len) ((16 - (len % 16)) % 16)

#define FOURCC_RIFF "RIFF"
#define FOURCC_DSMF "DSMF"

class EventConverter_DSM: virtual public EventHandler
{
	public:
		/// Prepare to convert events into KLM data sent to a stream.
		/**
		 * @param output
		 *   Output stream the DSM data will be written to.
		 */
		EventConverter_DSM(IFFWriter *iff, stream::output_sptr output,
			const PatchBankPtr& patches);

		/// Destructor.
		virtual ~EventConverter_DSM();

		// EventHandler overrides
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

		std::vector<stream::len> lenPattern; ///< Offset of each pattern
		Tempo lastTempo; ///< Last tempo set by song (or initial tempo)

	protected:
		IFFWriter *iff;              ///< IFF helper class
		stream::output_sptr output;  ///< Where to write data
		uint8_t patternBuffer[DSM_MAX_PACKED_PATTERN_SIZE]; ///< Store pattern before writing to file
		uint8_t *patBufPos;          ///< Current position in patternBuffer
		unsigned int curRow;         ///< Current row in pattern (0-63 inclusive)
		const PatchBankPtr patches;  ///< Copy of the patches for the song

		/// Write out the current delay
		void writeDelay(unsigned long curTick);
};

std::string MusicType_DSM::getCode() const
{
	return "dsm-dsik";
}

std::string MusicType_DSM::getFriendlyName() const
{
	return "Digital Sound Interface Kit Module";
}

std::vector<std::string> MusicType_DSM::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("dsm");
	return vcExtensions;
}

unsigned long MusicType_DSM::getCaps() const
{
	return
		InstEmpty
		| InstPCM
		| HasEvents
		| HasPatterns
	;
}

MusicType::Certainty MusicType_DSM::isInstance(stream::input_sptr input) const
{
	input->seekg(0, stream::start);
	IFF::fourcc sig1, sig2;
	uint32_t len1;
	input
		>> fixedLength(sig1, 4)
		>> u32le(len1)
		>> fixedLength(sig2, 4)
	;

	// Invalid RIFF signature
	// TESTED BY: mus_dsm_dsik_isinstance_c01
	if (sig1.compare(FOURCC_RIFF) != 0) return MusicType::DefinitelyNo;

	// Invalid DSMF signature
	// TESTED BY: mus_dsm_dsik_isinstance_c02
	if (sig2.compare(FOURCC_DSMF) != 0) return MusicType::DefinitelyNo;

	// File truncated
	// TESTED BY: mus_dsm_dsik_isinstance_c03
	if (len1 + 8 != input->size()) return MusicType::DefinitelyNo;

	// TESTED BY: mus_dsm_dsik_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_DSM::read(stream::input_sptr input, SuppData& suppData) const
{
	MusicPtr music(new Music());
	music->patches.reset(new PatchBank());

	// All DSM files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 4;
	music->ticksPerTrack = 64;
	music->loopDest = -1; // no loop

	IFFReader iff(input, IFF::Filetype_RIFF);
	IFF::fourcc type;
	iff.open(FOURCC_RIFF, &type);
	if (type.compare(FOURCC_DSMF) != 0) throw stream::error("This is not a DSMF file.");

	for (unsigned int i = 0; i < DSM_CHANNEL_COUNT; i++) {
		TrackInfo t;
		t.channelType = TrackInfo::PCMChannel;
		t.channelIndex = i;
		music->trackInfo.push_back(t);
	}

	Tempo lastTempo = music->initialTempo;

	std::vector<IFF::fourcc> chunks = iff.list();
	int j = 0;
	for (std::vector<IFF::fourcc>::const_iterator
		i = chunks.begin(); i != chunks.end(); i++, j++
	) {
		stream::len lenChunk = iff.seek(j);
		if (i->compare("SONG") == 0) {
			// Load song
			uint8_t volGlobal, volMaster, initialSpeed, initialBPM;
			uint16_t version, flags, orderCount, instrumentCount, patternCount;
			uint16_t channelCount;
			uint32_t pad;
			input
				>> nullPadded(music->metadata[Metadata::Title], 28)
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
			input->read(channelMap, 16);
			input->read(orders, 128);

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

		} else if (i->compare("INST") == 0) {
			PCMPatchPtr p(new PCMPatch());
			uint8_t volDefault;
			uint16_t flags, period;
			uint32_t address;
			std::string filename;
			input
				>> nullPadded(filename, 13)
				>> u16le(flags)
				>> u8(volDefault)
				>> u32le(p->lenData)
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
			p->data.reset(new uint8_t[p->lenData]);
			input->read(p->data.get(), p->lenData);

			// Convert from signed 8-bit to unsigned
			if (flags & 2) {
				uint8_t *data = p->data.get();
				for (unsigned long i = 0; i < p->lenData; i++) {
					*data = ((int8_t *)data)[0] + 128;
					data++;
				}
			}
			music->patches->push_back(p);

		} else if (i->compare("PATT") == 0) {
			// Skip redundant length field
			input->seekg(2, stream::cur);

			PatternPtr pattern(new Pattern());
			unsigned int lastRow[DSM_CHANNEL_COUNT];
			unsigned int lastInstrument[DSM_CHANNEL_COUNT];
			for (unsigned int track = 0; track < DSM_CHANNEL_COUNT; track++) {
				lastRow[track] = 0;
				lastInstrument[track] = 0;

				TrackPtr t(new Track());
				pattern->push_back(t);
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
					input >> u8(what);
					lenRead++;
					unsigned int channel = what & 0x0F;
					TrackPtr& track = pattern->at(channel);
					uint8_t note = 0, instrument = 0, volume = 255, command = 0, info = 0;
					if (what & 0x80) {
						input >> u8(note);
						lenRead++;
					}
					if (what & 0x40) {
						input >> u8(instrument);
						lenRead++;
					}
					if (what & 0x20) {
						input >> u8(volume);
						lenRead++;
					}
					if (what & 0x10) {
						input
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
							TrackEvent te;
							te.delay = row - lastRow[channel];
							NoteOffEvent *ev = new NoteOffEvent();
							te.event.reset(ev);
							track->push_back(te);
							lastRow[channel] = row;
						} else {
							TrackEvent te;
							te.delay = row - lastRow[channel];
							NoteOnEvent *ev = new NoteOnEvent();
							te.event.reset(ev);

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
							track->push_back(te);
							lastRow[channel] = row;
						}
					} else if (what & 0x20) { // 0x80 unset because of 'else'
						// Volume change
						TrackEvent te;
						te.delay = row - lastRow[channel];
						EffectEvent *ev = new EffectEvent();
						te.event.reset(ev);
						ev->type = EffectEvent::Volume;
						if (volume == 64) ev->data = 255;
						else ev->data = (volume << 2) | (volume >> 4);
						track->push_back(te);
						lastRow[channel] = row;
					}
					if (what & 0x10) {
						switch (command) {
							case 0x0c: { // volume change
								// Volume change
								TrackEvent te;
								te.delay = row - lastRow[channel];
								EffectEvent *ev = new EffectEvent();
								te.event.reset(ev);
								ev->type = EffectEvent::Volume;
								if (info >= 64) ev->data = 255;
								else ev->data = (info << 2) | (info >> 4);
								track->push_back(te);
								lastRow[channel] = row;
								break;
							}
							case 0x0f: { // set speed
								if (info == 0) break; // ignore
								TrackEvent te;
								te.delay = row - lastRow[channel];
								TempoEvent *ev = new TempoEvent();
								te.event.reset(ev);
								ev->tempo = lastTempo;
								if (info < 32) {
									ev->tempo.module(info, lastTempo.module_tempo());
								} else {
									ev->tempo.module(lastTempo.module_speed(), info);
								}
								track->push_back(te);
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
			music->patterns.push_back(pattern);

		} else {
			std::cout << "DSM: Skipping unknown chunk type " << *i << "\n";
		}
	}

	return music;
}

void MusicType_DSM::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	std::string strTitle;
	Metadata::TypeMap::const_iterator itTitle
		= music->metadata.find(Metadata::Title);
	if (itTitle != music->metadata.end()) {
		if (itTitle->second.length() > 28) {
			throw format_limitation("The metadata element 'Title' is longer than "
				"the maximum 28 character length.");
		}
		strTitle = itTitle->second;
	}

	if (music->trackInfo.size() > DSM_CHANNEL_COUNT) {
		throw format_limitation("This format has a maximum of 16 channels.");
	}

	if (music->patternOrder.size() > 128) {
		throw format_limitation("This format has a maximum of 128 entries in the "
			"order list.");
	}

	unsigned long tempo = music->initialTempo.module_tempo();
	if (tempo > 255) {
		throw stream::error(createString("Tempo is too fast for DSM file!  "
			"Calculated value is " << tempo << " but max permitted value is 255."));
	}

	IFFWriter iff(output, IFF::Filetype_RIFF);
	iff.begin(FOURCC_RIFF, FOURCC_DSMF);

	uint8_t speed = music->initialTempo.module_speed();
	iff.begin("SONG");
	output
		<< nullPadded(strTitle, 28)
		<< u16le(0) // version
		<< u16le(0) // flags
		<< u32le(0) // pad
		<< u16le(music->patternOrder.size())
		<< u16le(music->patches->size())
		<< u16le(music->patterns.size())
		<< u16le(music->trackInfo.size())
		<< u8(63) // global volume
		<< u8(63) // master volume
		<< u8(speed)
		<< u8(tempo)
	;
	// Default panning
	for (unsigned int i = 0; i < DSM_CHANNEL_COUNT; i++) {
		if (i % 2) {
			output << u8(0x80);
		} else {
			output << u8(0);
		}
	}
	uint8_t orders[128];
	memset(orders, 0xFF, sizeof(orders));
	unsigned int j = 0;
	for (std::vector<unsigned int>::const_iterator
		i = music->patternOrder.begin(); i != music->patternOrder.end(); i++, j++
	) {
		orders[j] = *i;
	}
	output->write(orders, 128);
	iff.end(); // SONG

	for (PatchBank::const_iterator
		i = music->patches->begin(); i != music->patches->end(); i++
	) {
		iff.begin("INST");

		PCMPatchPtr p = boost::dynamic_pointer_cast<PCMPatch>(*i);
		unsigned int flags = 0;
		if (p->loopEnd != 0) flags |= 1;
		unsigned int period = 8363 * 428 / p->sampleRate;
		uint8_t defVolume = p->defaultVolume >> 2; // 0..255 -> 0..63
		output
			<< nullPadded(std::string(), 13)
			<< u16le(flags)
			<< u8(defVolume)
			<< u32le(p->lenData)
			<< u32le(p->loopStart)
			<< u32le(p->loopEnd)
			<< u32le(0) // address pointer
			<< u16le(p->sampleRate)
			<< u16le(period)
			<< nullPadded(p->name, 28)
		;
		output->write(p->data.get(), p->lenData);

		iff.end(); // INST
	}

	// Write out the patterns
	EventConverter_DSM conv(&iff, output, music->patches);
	conv.lastTempo = music->initialTempo;
	conv.handleAllEvents(EventHandler::Pattern_Row_Track, music);

	iff.end(); // RIFF

	// Set final filesize to this
	output->truncate_here();
	return;
}

SuppFilenames MusicType_DSM::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_DSM::getMetadataList() const
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	return types;
}


EventConverter_DSM::EventConverter_DSM(IFFWriter *iff,
	stream::output_sptr output, const PatchBankPtr& patches)
	:	iff(iff),
		output(output),
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
	this->output << u16le(valLengthField);
	this->output->write(this->patternBuffer, lenUsed);
	this->iff->end(); // PATT
	this->patBufPos = this->patternBuffer;
	this->curRow = 0;
	return;
}

void EventConverter_DSM::handleEvent(unsigned long delay,
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
	return;
}

void EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	this->writeDelay(delay);
	uint8_t midiNote;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &midiNote, &bend, 0xFF);

	if (midiNote <= 11) {
		std::cerr << "DSM: Dropping note in octave -1.\n";
		return;
	}

	if (ev->instrument >= this->patches->size()) {
		std::cerr << "DSM: Dropping note with out-of-range instrument #"
			<< ev->instrument << "\n";
		return;
	}
	uint8_t velFlag = 0;
	if (
		(ev->velocity >= 0) &&
		((unsigned int)ev->velocity != this->patches->at(ev->instrument)->defaultVolume)
	) velFlag = 0x20;
	*this->patBufPos++ = (trackIndex & 0x1F) | 0xC0 | velFlag; // event with note+inst
	*this->patBufPos++ = midiNote - 11;
	*this->patBufPos++ = ev->instrument + 1; // +1 because 0 means no/prev instrument
	if (velFlag) *this->patBufPos++ = ev->velocity >> 2;
	return;
}

void EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->writeDelay(delay);
	*this->patBufPos++ = (trackIndex & 0x1F) | 0x80; // event with note+inst
	*this->patBufPos++ = 0xFE; // note off
	*this->patBufPos++ = 0x00; // no instrument

	return;
}

void EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->type) {
		case EffectEvent::PitchbendNote:
			//if (this->flags & IntegerNotesOnly) return;
			std::cout << "DSM: TODO - implement pitch bends\n";
			break;
		case EffectEvent::Volume:
			*this->patBufPos++ = (trackIndex & 0x1F) | 0x20; // event with volume only
			*this->patBufPos++ = ev->data >> 2; // volume value
			break;
	}
	return;
}

void EventConverter_DSM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->configType) {
		case ConfigurationEvent::None:
			break;
		case ConfigurationEvent::EnableOPL3:
		case ConfigurationEvent::EnableDeepTremolo:
		case ConfigurationEvent::EnableDeepVibrato:
		case ConfigurationEvent::EnableRhythm:
		case ConfigurationEvent::EnableWaveSel:
			throw format_limitation("This format cannot store OPL events.");
	}
	return;
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
