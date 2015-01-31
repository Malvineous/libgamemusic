/**
 * @file  mus-s3m-screamtracker.cpp
 * @brief Support for the ScreamTracker S3M format.
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
#include <camoto/util.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp> // freqToMIDI
#include "mus-s3m-screamtracker.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define S3M_CHANNEL_COUNT 32 ///< Number of storage channels in S3M file

/// Maximum number of bytes needed to store one complete packed pattern
#define S3M_MAX_PACKED_PATTERN_SIZE (64*(32*6+1))

/// Number of rows in every pattern
#define S3M_ROWS_PER_PATTERN 64

/// Calculate number of bytes to add to len to bring it up to a parapointer
/// boundary (multiple of 16.)
#define PP_PAD(len) ((16 - (len % 16)) % 16)

class EventConverter_S3M: virtual public EventHandler
{
	public:
		/// Prepare to convert events into KLM data sent to a stream.
		/**
		 * @param output
		 *   Output stream the S3M data will be written to.
		 *
		 * @param patches
		 *   Patch bank to write.
		 */
		EventConverter_S3M(stream::output_sptr output, const PatchBankPtr& patches);

		/// Destructor.
		virtual ~EventConverter_S3M();

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
			unsigned int patternIndex, const GotoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

		std::vector<stream::len> lenPattern; ///< Offset of each pattern
		Tempo lastTempo; ///< Last tempo set by song (or initial tempo)

	protected:
		stream::output_sptr output;  ///< Where to write data
		uint8_t patternBuffer[S3M_MAX_PACKED_PATTERN_SIZE]; ///< Store pattern before writing to file
		uint8_t *patBufPos;          ///< Current position in patternBuffer
		unsigned int curRow;         ///< Current row in pattern (0-63 inclusive)
		const PatchBankPtr patches;  ///< Copy of the patches for the song

		/// Write out the current delay
		void writeDelay(unsigned long curTick);
};

std::string MusicType_S3M::getCode() const
{
	return "s3m-screamtracker";
}

std::string MusicType_S3M::getFriendlyName() const
{
	return "ScreamTracker 3 Module";
}

std::vector<std::string> MusicType_S3M::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("s3m");
	return vcExtensions;
}

unsigned long MusicType_S3M::getCaps() const
{
	return
		InstEmpty
		| InstOPL
		| InstPCM
		| HasEvents
		| HasPatterns
	;
}

MusicType::Certainty MusicType_S3M::isInstance(stream::input_sptr input) const
{
	stream::pos len = input->size();
	// Too short
	// TESTED BY: mus_s3m_screamtracker_isinstance_c03
	if (len < 30) return MusicType::DefinitelyNo;

	input->seekg(28, stream::start);
	uint8_t sig1, sig2;
	input
		>> u8(sig1)
		>> u8(sig2)
	;
	// Invalid signature bytes
	// TESTED BY: mus_s3m_screamtracker_isinstance_c01
	if ((sig1 != 0x1A) || (sig2 != 0x10)) return MusicType::DefinitelyNo;

	input->seekg(28+4+6*2, stream::start);
	std::string sig3;
	input >> fixedLength(sig3, 4);
	// Invalid signature tag
	// TESTED BY: mus_s3m_screamtracker_isinstance_c02
	if (sig3.compare("SCRM") != 0) return MusicType::DefinitelyNo;

	// TESTED BY: mus_s3m_screamtracker_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_S3M::read(stream::input_sptr input, SuppData& suppData) const
{
	MusicPtr music(new Music());
	music->patches.reset(new PatchBank());

	// All S3M files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 4;
	music->ticksPerTrack = 64;
	music->loopDest = -1; // no loop

	input->seekg(0, stream::start);

	input >> nullPadded(music->metadata[Metadata::Title], 28);

	uint8_t sig1, type, globalVolume, masterVolume, framesPerRow, framesPerSecond;
	uint8_t ultraClickRemoval, defaultPan;
	uint16_t reserved, orderCount, instrumentCount, patternCount, flags;
	uint16_t trackerVersion, sampleType, ptrSpecial;
	input
		>> u8(sig1)
		>> u8(type)
		>> u16le(reserved)
		>> u16le(orderCount)
		>> u16le(instrumentCount)
		>> u16le(patternCount)
		>> u16le(flags)
		>> u16le(trackerVersion)
		>> u16le(sampleType)
	;
	input->seekg(4, stream::cur); // "SCRM" signature
	input
		>> u8(globalVolume)
		>> u8(framesPerRow)
		>> u8(framesPerSecond)
		>> u8(masterVolume)
		>> u8(ultraClickRemoval)
		>> u8(defaultPan)
	;
	input->seekg(8, stream::cur); // padding
	input
		>> u16le(ptrSpecial)
	;
	if (type != 0x10) {
		throw stream::error(createString("S3M: Unknown type " << (int)type));
	}

	int adlibTrack1 = -1; // index of first OPL track
	uint8_t channelSettings[S3M_CHANNEL_COUNT];
	input->read(channelSettings, S3M_CHANNEL_COUNT);
	for (unsigned int i = 0; i < S3M_CHANNEL_COUNT; i++) {
		TrackInfo t;
		const uint8_t& c = channelSettings[i];
		if (c < 16) {
			t.channelType = TrackInfo::PCMChannel;
			// 0,1,2...8,9,10 -> 0,2,4...1,3,5 [L1,R1,L2,R2,...]
			t.channelIndex = (c % 8) * 2 + (c >> 3);
		} else if (c < 25) {
			t.channelType = TrackInfo::OPLChannel;
			t.channelIndex = c - 16;
			if (adlibTrack1 < 0) adlibTrack1 = i;
		} else if (c < 30) {
			t.channelType = TrackInfo::OPLPercChannel;
			/// @todo: Make sure this correctly maps to the right perc instrument
			t.channelIndex = 4 - (c - 25);
		} else {
			t.channelType = TrackInfo::UnusedChannel;
			t.channelIndex = c - 30;
		}
		music->trackInfo.push_back(t);
	}

	for (unsigned int i = 0; i < orderCount; i++) {
		uint8_t order;
		input >> u8(order);
		if (order < 0xFE) {
			music->patternOrder.push_back(order);
		} else if (order == 0xFE) {
			std::cout << "S3M TODO: Don't ignore marker pattern\n";
		}
	}

	std::vector<uint16_t> ptrInstruments;
	ptrInstruments.reserve(instrumentCount);
	for (unsigned int i = 0; i < instrumentCount; i++) {
		uint16_t ptrInstrument;
		input >> u16le(ptrInstrument);
		ptrInstruments.push_back(ptrInstrument);
	}

	std::vector<uint16_t> ptrPatterns;
	ptrPatterns.reserve(patternCount);
	for (unsigned int i = 0; i < patternCount; i++) {
		uint16_t ptrPattern;
		input >> u16le(ptrPattern);
		ptrPatterns.push_back(ptrPattern);
	}

	if (framesPerRow == 0) framesPerRow = 1;
	music->initialTempo.module(framesPerRow, framesPerSecond);

	// Read instruments
	music->patches->reserve(instrumentCount);
	for (std::vector<uint16_t>::const_iterator
		i = ptrInstruments.begin(); i != ptrInstruments.end(); i++
	) {
		// Jump to the parapointer destination
		input->seekg(*i << 4, stream::start);

		uint8_t type;
		std::string title;
		input
			>> u8(type)
			>> nullPadded(title, 12)
		;
		if (!title.empty()) std::cout << "Ignoring S3M instrument filename field: " << title << "\n";
		switch (type) {
			case 0: {
				// Read anyway as it wouldn't be listed if there was no significance.
				// It might be supposed to be a blank line.
				PatchPtr p(new Patch());
				input->seekg(1+2+4+4+4+1+1+1+1+4+12, stream::cur);
				input >> nullPadded(p->name, 28);
				music->patches->push_back(p);
				break;
			}
			case 1: {
				PCMPatchPtr p(new PCMPatch());
				uint32_t ppSample;
				uint8_t ppSampleHigh, volume, reserved, pack, flags;
				input
					>> u8(ppSampleHigh)
					>> u16le(ppSample)
					>> u32le(p->lenData)
					>> u32le(p->loopStart)
					>> u32le(p->loopEnd)
					>> u8(volume)
					>> u8(reserved)
					>> u8(pack)
					>> u8(flags)
					>> u32le(p->sampleRate)
				;
				ppSample |= (uint32_t)ppSampleHigh << 16;
				if (pack != 0) {
					throw stream::error("Unsupported sample compression - please report "
						"this problem!");
				}
				if (!(flags & 1)) {
					// Loop off
					p->loopStart = 0;
					p->loopEnd = 0;
				}
				p->numChannels = (flags & 2) ? 2 : 1;
				p->bitDepth = (flags & 4) ? 16 : 8;
				if (volume > 63) volume = 63;
				p->defaultVolume = (volume << 2) | (volume >> 4); // 0-63 -> 0-255

				input->seekg(12, stream::cur);
				input >> nullPadded(p->name, 28);

				// Read the PCM data
				input->seekg(ppSample << 4, stream::start);
				p->data.reset(new uint8_t[p->lenData]);
				input->read(p->data.get(), p->lenData);

				// Convert from little-endian to host byte order if 16-bit
				if (p->bitDepth == 16) {
					int16_t *data = (int16_t *)p->data.get();
					for (unsigned long i = 0; i < p->lenData / 2; i++) {
						*data = le16toh(*data);
						data++;
					}
				}

				music->patches->push_back(p);
				break;
			}
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7: {
				input->seekg(3, stream::cur);

				OPLPatchPtr p(new OPLPatch());
				uint8_t inst[11];
				input->read(inst, 11);

				OPLOperator *o = &p->m;
				for (int op = 0; op < 2; op++) {
					o->enableTremolo = (inst[0 + op] >> 7) & 1;
					o->enableVibrato = (inst[0 + op] >> 6) & 1;
					o->enableSustain = (inst[0 + op] >> 5) & 1;
					o->enableKSR     = (inst[0 + op] >> 4) & 1;
					o->freqMult      =  inst[0 + op] & 0x0F;
					o->scaleLevel    =((inst[2 + op] >> 7) & 1) |
					                  ((inst[2 + op] >> 5) & 2);
					o->outputLevel   =  inst[2 + op] & 0x3F;
					o->attackRate    =  inst[4 + op] >> 4;
					o->decayRate     =  inst[4 + op] & 0x0F;
					o->sustainRate   =  inst[6 + op] >> 4;
					o->releaseRate   =  inst[6 + op] & 0x0F;
					o->waveSelect    =  inst[8 + op] & 0x07;
					o = &p->c;
				}
				p->feedback    = (inst[10] >> 1) & 0x07;
				p->connection  =  inst[10] & 1;
				p->rhythm      = OPLPatch::Melodic;

				input->seekg(1, stream::cur);
				uint8_t volume;
				input >> u8(volume);
				p->defaultVolume = (volume << 2) | (volume >> 4);
				input->seekg(1+2, stream::cur);
				uint32_t c2spd;
				input >> u32le(c2spd);
/*
C-4 note
4363 = 135411 mHz
5363 = 166514 mHz
6363 = 197616 mHz
7363 = 228719 mHz
8363 = 259443 mHz
9363 = 290546 mHz
10363= 322028 mHz
*/
				std::cout << "S3M: Adlib instrument has c2spd of " << c2spd << "\n";
				if (c2spd != 8363) throw stream::error("S3M Adlib instrument has fine "
					"tuning value - these are unimplemented!  Please report this problem.");

				input->seekg(12, stream::cur);
				input >> nullPadded(p->name, 28);
				music->patches->push_back(p);
				break;
			}
			default:
				throw stream::error(createString("Unknown S3M instrument type "
					<< (int)type));
		}
	}

	// Read the song data
	music->patterns.reserve(patternCount);
	bool firstPattern = true;
	unsigned int patternIndex = 0;
	const unsigned int& firstOrder = music->patternOrder[0];
	for (std::vector<uint16_t>::const_iterator
		i = ptrPatterns.begin(); i != ptrPatterns.end(); i++, patternIndex++
	) {
		// Jump to the parapointer destination
		input->seekg(*i << 4, stream::start);
		PatternPtr pattern(new Pattern());
		for (unsigned int track = 0; track < S3M_CHANNEL_COUNT; track++) {
			TrackPtr t(new Track());
			pattern->push_back(t);
			if (
				firstPattern
				&& (patternIndex == firstOrder)
				&& ((signed int)track == adlibTrack1)
			) { // first OPL track in pattern being played first in order list
				// Set standard settings
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableOPL3;
					ev->value = 0;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableDeepTremolo;
					ev->value = 1;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableDeepVibrato;
					ev->value = 1;
					t->push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::EnableWaveSel;
					ev->value = 1;
					t->push_back(te);
				}

				firstPattern = false;
			}
		}

		unsigned int lastRow[S3M_CHANNEL_COUNT];
		unsigned int lastInstrument[S3M_CHANNEL_COUNT];
		for (unsigned int r = 0; r < S3M_CHANNEL_COUNT; r++) {
			lastRow[r] = 0;
			lastInstrument[r] = 0;
		}

		// Skip length field
		uint16_t lenPackedRow;
		input >> u16le(lenPackedRow);
		unsigned int lenRead = 2;

		Tempo lastTempo = music->initialTempo;
		for (unsigned int row = 0; row < S3M_ROWS_PER_PATTERN; row++) {
			uint8_t what;
			do {
				input >> u8(what);
				lenRead++;
				unsigned int channel = what & 0x1F;
				TrackPtr& track = pattern->at(channel);
				uint8_t note, instrument, volume, command, info;
				if (what & 0x20) {
					input
						>> u8(note)
						>> u8(instrument)
					;
					lenRead += 2;
				}
				if (what & 0x40) {
					input
						>> u8(volume)
					;
					lenRead++;
				}
				if (what & 0x80) {
					input
						>> u8(command)
						>> u8(info)
					;
					lenRead += 2;
				}
				if (what & 0x20) {
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

						ev->instrument = instrument;
						ev->milliHertz = midiToFreq(((note >> 4) + 1) * 12 + (note & 0x0F));

						if ((what & 0x40) && (volume < 65)) {
							if (volume == 64) ev->velocity = 255;
							else ev->velocity = (volume << 2) | (volume >> 4);
						} else { // missing or =255
							ev->velocity = DefaultVelocity; // instrument default
						}
						track->push_back(te);
						lastRow[channel] = row;
					}
				} else if (what & 0x40) { // 0x20 unset because of 'else'
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
				if (what & 0x80) {
					switch (command) {
						case 0x01: { // A: set speed
							TrackEvent te;
							te.delay = row - lastRow[channel];
							TempoEvent *ev = new TempoEvent();
							te.event.reset(ev);
							ev->tempo = lastTempo;
							ev->tempo.module(info, lastTempo.module_tempo());
							track->push_back(te);
							lastRow[channel] = row;
							lastTempo = ev->tempo;
							break;
						}
						case 0x02: { // B: Jump to order
							TrackEvent te;
							te.delay = row - lastRow[channel];
							GotoEvent *ev = new GotoEvent();
							te.event.reset(ev);
							ev->type = GotoEvent::SpecificOrder;
							ev->loop = 0;
							ev->targetOrder = info;
							ev->targetRow = 0;
							track->push_back(te);
							lastRow[channel] = row;
							break;
						}
						case 0x03: { // C: Jump to row
							TrackEvent te;
							te.delay = row - lastRow[channel];
							GotoEvent *ev = new GotoEvent();
							te.event.reset(ev);
							ev->type = GotoEvent::NextPattern;
							ev->loop = 0;
							ev->targetOrder = 0;
							ev->targetRow = ((info & 0xf0) >> 4) * 10 + (info & 0x0f);
							track->push_back(te);
							lastRow[channel] = row;
							break;
						}
						case 0x20: { // T: set tempo
							TrackEvent te;
							te.delay = row - lastRow[channel];
							TempoEvent *ev = new TempoEvent();
							te.event.reset(ev);
							ev->tempo = lastTempo;
							ev->tempo.module(lastTempo.module_speed(), info);
							track->push_back(te);
							lastRow[channel] = row;
							lastTempo = ev->tempo;
							break;
						}
						default:
							std::cout << "S3M: Disregarding unimplemented effect " << std::hex
								<< (int)command << std::dec << "\n";
							break;
					}
				}
			} while (what);
		}

		if (lenRead != lenPackedRow) {
			std::cerr << "Mismatch between row length field and actual row length "
				"in pattern " << music->patterns.size() << "!  Row was supposed to "
				"have " << lenPackedRow << " bytes but " << lenRead << " bytes were "
				"read." << std::endl;
		}

		music->patterns.push_back(pattern);
	}

	return music;
}

void MusicType_S3M::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	Metadata::TypeMap::const_iterator itTitle
		= music->metadata.find(Metadata::Title);
	if (itTitle != music->metadata.end()) {
		if (itTitle->second.length() > 28) {
			throw format_limitation("The metadata element 'Title' is longer than "
				"the maximum 28 character length.");
		}
		output << nullPadded(itTitle->second, 28);
	} else {
		output << nullPadded(std::string(), 28);
	}
	unsigned long tempo = music->initialTempo.module_tempo();
	if (tempo > 255) {
		throw format_limitation(createString("Tempo is too fast for S3M file!  "
			"Calculated value is " << tempo << " but max permitted value is 255."));
	}
	output
		<< u8(0x1A)
		<< u8(0x10)  // ScreamTracker 3
		<< u16le(0x0000) // reserved
		<< u16le(music->patternOrder.size() + 1) // +1 makes room for an EOF value
		<< u16le(music->patches->size())
		<< u16le(music->patterns.size())
		<< u16le(0) // flags
		<< u16le(0xCA00) // tracker version
		<< u16le(0x0002) // unsigned samples
		<< nullPadded("SCRM", 4)
		<< u8(64) // global volume range
		<< u8(music->initialTempo.module_speed())
		<< u8(tempo)
		<< u8(0x30) // mixing volume (SB only)
		<< u8(0x10) // GUS click removal (seems to be what ST3 puts here, probably unused today)
		<< u8(0) // don't use pan values in header
		<< nullPadded(std::string(), 8+2) // reserved
	;

	// Map all the tracks to S3M channels
	// libgamemusic track -> S3M channel -> S3M target (OPL, Left PCM 1, etc.)
	if (music->trackInfo.size() > S3M_CHANNEL_COUNT) {
		throw format_limitation("Too many channels!  S3M has a maximum of "
			TOSTRING(S3M_CHANNEL_COUNT) " channels.");
	}
	for (TrackInfoVector::const_iterator
		ti = music->trackInfo.begin(); ti != music->trackInfo.end(); ti++
	) {
		uint8_t chanAlloc = 255;
		switch (ti->channelType){
			case TrackInfo::UnusedChannel:
				chanAlloc = 255;
				break;
			case TrackInfo::AnyChannel:
				throw stream::error("S3M writer was given an AnyChannel track!  This "
					"is not permitted and is a bug.");
			case TrackInfo::OPLChannel:
				if (ti->channelIndex > 8) {
					throw stream::error("Got a track on OPL channel > 8.  S3M only "
						"supports one OPL chip.");
				}
				chanAlloc = 16 + ti->channelIndex;
				break;
			case TrackInfo::OPLPercChannel:
				assert(ti->channelIndex < 5);
				chanAlloc = 25 + (4 - ti->channelIndex);
				break;
			case TrackInfo::MIDIChannel:
				throw stream::error("S3M files cannot store MIDI instruments.");
			case TrackInfo::PCMChannel:
				// 0,1,2,3,4,5 -> 0,8,1,9,2,10 [L1,R1,L2,R2,...]
				chanAlloc = (ti->channelIndex % 2) * 8 + (ti->channelIndex >> 1);
				break;
		}
		// Record where this S3M channel is going to be played
		output << u8(chanAlloc);
	}

	// Pad up to 32 channels
	for (int i = music->trackInfo.size(); i < S3M_CHANNEL_COUNT; i++) {
		output << u8(255);
	}

	// Order count
	for (std::vector<unsigned int>::const_iterator
		i = music->patternOrder.begin(); i != music->patternOrder.end(); i++
	) {
		output << u8(*i);
	}
	// Order list finishes off with two EOF values
	output << u8(0xFF);

	// Work out where the first parapointer will point to
	stream::pos offPatternPtrs = 0x60 + (music->patternOrder.size() + 1)*1
		+ music->patches->size()*2;
	stream::pos nextPP = offPatternPtrs + music->patterns.size()*2;
	int finalPad = PP_PAD(nextPP);
	nextPP += finalPad;
	stream::pos firstPP = nextPP;

	// Instrument pointers
	std::vector<stream::pos> instOffsets;
	for (PatchBank::const_iterator
		i = music->patches->begin(); i != music->patches->end(); i++
	) {
		// Since we're using parapointers the lower four bits of the offset
		// must always be zero as those bits aren't saved.
		assert((nextPP & 0x0F) == 0);

		output << u16le(nextPP >> 4);
		instOffsets.push_back(nextPP);

		// Figure out how big this instrument is for the next offset
		nextPP += 0x50;
		PCMPatchPtr pcmPatch = boost::dynamic_pointer_cast<PCMPatch>(*i);
		if (pcmPatch) {
			nextPP += pcmPatch->lenData;
			// Round up to the nearest parapointer boundary
			if (nextPP % 16) {
				nextPP += PP_PAD(nextPP);
			}
		}
	}

	// Pattern pointers
	assert((nextPP & 0x0F) == 0);
	assert(output->tellp() == offPatternPtrs);
	stream::pos offFirstPattern = nextPP;
	if (music->patterns.size()) {
		output << u16le(offFirstPattern >> 4);
		// Reserve some space for all the other pattern pointers to update later
		output << nullPadded(std::string(), (music->patterns.size() - 1) * 2 /* sizeof(uint16le) */);
	}

	// Default pan positions are unused, so omitted
	//output << nullPadded(std::string(), numChannels);

	// Pad up to first parapointer position.  ST3 uses 0x80, we'll use 0x00.
	output << nullPadded(std::string(), finalPad);
	assert(output->tellp() == firstPP);

	// Write out the instruments
	std::vector<stream::pos>::iterator nextInstOffset = instOffsets.begin();
	for (PatchBank::const_iterator
		i = music->patches->begin(); i != music->patches->end(); i++, nextInstOffset++
	) {
		OPLPatchPtr oplPatch = boost::dynamic_pointer_cast<OPLPatch>(*i);
		if (oplPatch) {
			uint8_t type = 2;
			if (oplPatch->rhythm != 0) type += 6 - oplPatch->rhythm;
			uint32_t c2spd = 8363;
			uint8_t volume = oplPatch->defaultVolume >> 2; // 0-255 -> 0-63

			uint8_t inst[11];
			assert(oplPatch);
			OPLOperator *o = &oplPatch->m;
			for (int op = 0; op < 2; op++) {
				inst[0 + op] =
					((o->enableTremolo & 1) << 7) |
					((o->enableVibrato & 1) << 6) |
					((o->enableSustain & 1) << 5) |
					((o->enableKSR     & 1) << 4) |
					 (o->freqMult      & 0x0F)
					;
				inst[2 + op] = ((o->scaleLevel&1) << 7) | ((o->scaleLevel&2) << 5) | (o->outputLevel & 0x3F);
				//inst[2 + op] = (o->scaleLevel << 6) | (o->outputLevel & 0x3F);
				inst[4 + op] = (o->attackRate << 4) | (o->decayRate & 0x0F);
				inst[6 + op] = (o->sustainRate << 4) | (o->releaseRate & 0x0F);
				inst[8 + op] =  o->waveSelect & 7;

				o = &oplPatch->c;
			}
			inst[10] = ((oplPatch->feedback & 7) << 1) | (oplPatch->connection & 1);

			output
				<< u8(type)
				<< nullPadded(std::string(), 12) // blank filename
				<< nullPadded(std::string(), 3)
			;
			output->write(inst, 11);
			output
				<< u8(0)
				<< u8(volume)
				<< nullPadded(std::string(), 3)
				<< u32le(c2spd)
				<< nullPadded(std::string(), 12)
				<< nullPadded(oplPatch->name.substr(0, 28), 28) /// @todo Write instrument w/ 28+29 char name to test
				<< nullPadded("SCRI", 4)
			;
		} else {
			PCMPatchPtr pcmPatch = boost::dynamic_pointer_cast<PCMPatch>(*i);
			if (pcmPatch) {
				if ((pcmPatch->bitDepth != 8) && (pcmPatch->bitDepth != 16)) {
					throw format_limitation("This file format can only store 8-bit and "
						"16-bit PCM instruments.");
				}
				if ((pcmPatch->numChannels != 1) && (pcmPatch->numChannels != 2)) {
					throw format_limitation("This file format only supports mono and "
						"stereo instruments.");
				}
				if (pcmPatch->numChannels != 1) {
					throw format_limitation("Only mono PCM instruments have been "
						"implemented in the S3M writer.");
				}

				uint8_t flags = 0;
				if (pcmPatch->loopEnd != 0) flags |= 1; // loop on
				if (pcmPatch->bitDepth == 16) flags |= 4;
				uint8_t volume = pcmPatch->defaultVolume >> 2; // 0-255 -> 0-63

				stream::pos sampleOffset = *nextInstOffset;
				assert(output->tellp() == sampleOffset);
				sampleOffset += 0x50; // skip instrument header
				sampleOffset >>= 4; // it's a parapointer
				output
					<< u8(1) // PCM sample
					<< nullPadded(std::string(), 12) // blank filename
					<< u8(sampleOffset >> 16)
					<< u16le(sampleOffset & 0xFFFF)
					<< u32le(pcmPatch->lenData)
					<< u32le(pcmPatch->loopStart)
					<< u32le(pcmPatch->loopEnd)
					<< u8(volume)
					<< u8(0)
					<< u8(0) // unpacked
					<< u8(flags)
					<< u32le(pcmPatch->sampleRate)
					<< nullPadded(std::string(), 12)
					<< nullPadded(pcmPatch->name.substr(0, 28), 28) /// @todo Write instrument w/ 28+29 char name to test
					<< nullPadded("SCRS", 4)
				;
				output->write(pcmPatch->data.get(), pcmPatch->lenData);

				// Pad up to parapointer boundary.  This is easier since the sample data
				// always starts on a parapointer boundary.
				output << nullPadded(std::string(), PP_PAD(pcmPatch->lenData));
			} else {
				MIDIPatchPtr midiPatch = boost::dynamic_pointer_cast<MIDIPatch>(*i);
				if (midiPatch) {
					throw format_limitation("This file format can only store OPL and PCM "
						"instruments.");
				}

				// Otherwise write a blank, it's a placeholder
				output
					<< u8(0) // type
					<< nullPadded(std::string(), 12 + 3 + 11 + 1)
				;
				output
					<< u8(63) // default volume
					<< nullPadded(std::string(), 3)
					<< u32le(8363) // c2spd
					<< nullPadded(std::string(), 12)
					<< nullPadded((*i)->name.substr(0, 28), 28) /// @todo Write instrument w/ 28+29 char name to test
					<< nullPadded("SCRS", 4)
				;
			}
		}
	}

	// Write out the patterns
	EventConverter_S3M conv(output, music->patches);
	conv.lastTempo = music->initialTempo;
	conv.handleAllEvents(EventHandler::Pattern_Row_Track, music);

	// Set final filesize to this
	output->truncate_here();

	// Go back and write the pattern offsets except for the last one which is EOF
	assert(music->patterns.size() == conv.lenPattern.size());
	if (conv.lenPattern.size()) {
		output->seekp(offPatternPtrs + 2, stream::start);
		stream::pos offNextPattern = offFirstPattern;
		for (std::vector<stream::len>::const_iterator
			i = conv.lenPattern.begin(); i != conv.lenPattern.end() - 1; i++
		) {
			offNextPattern += *i;
			output << u16le(offNextPattern >> 4);
		}
	}
	return;
}

SuppFilenames MusicType_S3M::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_S3M::getMetadataList() const
{
	Metadata::MetadataTypes types;
	types.push_back(Metadata::Title);
	return types;
}


EventConverter_S3M::EventConverter_S3M(stream::output_sptr output,
	const PatchBankPtr& patches)
	:	output(output),
		patBufPos(patternBuffer),
		curRow(0),
		patches(patches)
{
}

EventConverter_S3M::~EventConverter_S3M()
{
}

void EventConverter_S3M::endOfTrack(unsigned long delay)
{
	return;
}

void EventConverter_S3M::endOfPattern(unsigned long delay)
{
	this->writeDelay(delay);
	this->writeDelay(64 - this->curRow);
	if (this->curRow != 64) {
		throw stream::error(createString("Tried to write an S3M pattern with "
			<< this->curRow << " rows, but this format only supports 64 rows per "
			"pattern."));
	}

	// Write out the pattern
	unsigned int lenUsed = this->patBufPos - this->patternBuffer;
	uint16_t valLengthField = lenUsed + 2; // include size of length field itself
	this->output << u16le(valLengthField);
	this->output->write(this->patternBuffer, lenUsed);
	unsigned int lenPadding = PP_PAD(lenUsed + 2);
	output << nullPadded(std::string(), lenPadding);
	this->patBufPos = this->patternBuffer;
	this->curRow = 0;

	this->lenPattern.push_back(valLengthField + lenPadding);
	return;
}

void EventConverter_S3M::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->writeDelay(delay);
	if (this->lastTempo.module_speed() != ev->tempo.module_speed()) {
		*this->patBufPos++ = (trackIndex & 0x1F) | 0x80; // effect
		*this->patBufPos++ = 0x01; // A: set speed
		*this->patBufPos++ = ev->tempo.module_speed(); // new speed
	} else if (this->lastTempo.module_tempo() != ev->tempo.module_tempo()) {
		*this->patBufPos++ = (trackIndex & 0x1F) | 0x80; // effect
		*this->patBufPos++ = 0x20; // T: set tempo
		*this->patBufPos++ = ev->tempo.module_tempo(); // new tempo
	}
	this->lastTempo = ev->tempo;
	return;
}

void EventConverter_S3M::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	this->writeDelay(delay);
	uint8_t midiNote;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &midiNote, &bend, 0xFF);

	uint8_t note = midiNote % 12;
	uint8_t oct = midiNote / 12;
	if (oct > 0) oct--;
	else {
		std::cerr << "S3M: Dropping note in octave -1.\n";
		return;
	}

	uint8_t velFlag = 0;
	if (
		(ev->velocity >= 0) &&
		((unsigned int)ev->velocity != this->patches->at(ev->instrument)->defaultVolume)
	) velFlag = 0x40;
	*this->patBufPos++ = (trackIndex & 0x1F) | 0x20 | velFlag; // event with note+inst
	*this->patBufPos++ = (oct << 4) | note;
	*this->patBufPos++ = ev->instrument + 1; // +1 because 0 means no/prev instrument
	if (velFlag) *this->patBufPos++ = ev->velocity >> 2;
	return;
}

void EventConverter_S3M::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->writeDelay(delay);
	*this->patBufPos++ = (trackIndex & 0x1F) | 0x20; // event with note+inst
	*this->patBufPos++ = 0xFE; // note off
	*this->patBufPos++ = 0x00; // no instrument

	return;
}

void EventConverter_S3M::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->type) {
		case EffectEvent::PitchbendNote:
			//if (this->flags & IntegerNotesOnly) return;
			std::cout << "S3M: TODO - implement pitch bends\n";
			break;
		case EffectEvent::Volume:
			*this->patBufPos++ = (trackIndex & 0x1F) | 0x40; // event with volume only
			*this->patBufPos++ = ev->data >> 2; // volume value
			break;
	}
	return;
}

void EventConverter_S3M::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const GotoEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->type) {
		case GotoEvent::NextPattern:
			*this->patBufPos++ = (trackIndex & 0x1F) | 0x80; // event with effect only
			*this->patBufPos++ = 0x03; // C: Jump to row
			*this->patBufPos++ = ((ev->targetRow / 10) << 4) | (ev->targetRow % 10); // decimal as hex
			break;
		case GotoEvent::SpecificOrder:
			*this->patBufPos++ = (trackIndex & 0x1F) | 0x80; // event with effect only
			*this->patBufPos++ = 0x02; // B: Jump to order
			*this->patBufPos++ = ev->targetOrder;
			break;
	}
	return;
}

void EventConverter_S3M::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->writeDelay(delay);
	switch (ev->configType) {
		case ConfigurationEvent::EmptyEvent:
			break;
		case ConfigurationEvent::EnableOPL3:
			if (ev->value != 0) std::cerr << "OPL3 cannot be enabled in this format, ignoring event.\n";
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			if (ev->value != 1) std::cerr << "Deep tremolo cannot be disabled in this format, ignoring event.\n";
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			if (ev->value != 1) std::cerr << "Deep vibrato cannot be disabled in this format, ignoring event.\n";
			break;
		case ConfigurationEvent::EnableRhythm:
			// Ignored, will set when rhythm channels are played
			break;
		case ConfigurationEvent::EnableWaveSel:
			if (ev->value != 1) std::cerr << "Wave selection registers cannot be disabled in this format, ignoring event.\n";
			break;
	}
	return;
}

void EventConverter_S3M::writeDelay(unsigned long delay)
{
	if (delay == 0) return; // no delay yet
	assert(delay < 1048576*1024); // safety catch

	if (this->curRow + delay > 64) {
		throw format_limitation(createString("S3M: Tried to write pattern with more "
			"than 64 rows (next row is " << this->curRow + delay << ")."));
	}

	this->curRow += delay;
	while (delay--) {
		*this->patBufPos++ = 0x00; // finish current row
	}
	return;
}
