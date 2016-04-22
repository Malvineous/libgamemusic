/**
 * @file  mus-tbsa-doofus.hpp
 * @brief Support for The Bone Shaker Architect used in Doofus.
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
#include <camoto/stream_string.hpp>
#include <camoto/util.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp> // midiToFreq
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-pcm.hpp>
#include <camoto/gamemusic/util-opl.hpp>
#include "mus-tbsa-doofus.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Number of storage channels in TBSA file
#define TBSA_CHANNEL_COUNT 11

/// Fixed module tempo/bpm for all songs (but module 'speed' can change)
#define TBSA_TEMPO 66

/// Number of rows in a normal (full-length) pattern
#define TBSA_PATTERN_LENGTH 63

// Safety limits to avoid infinite loops
#define TBSA_MAX_ORDERS 256
#define TBSA_MAX_INSTS  256
#define TBSA_MAX_PATTS  4096
#define TBSA_MAX_ORD_LEN 256
#define TBSA_MAX_PATSEG_LEN 4096


class EventConverter_TBSA: virtual public EventHandler
{
	public:
		/// Prepare to convert events into TBSA data sent to a stream.
		/**
		 * @param content
		 *   Output stream the TBSA data will be written to.
		 *
		 * @param music
		 *   Song parameters (patches, initial tempo, etc.)  Events are not read
		 *   from here, the EventHandler is used for that.
		 */
		EventConverter_TBSA(stream::output& content, const Music& music);

		/// Destructor.
		virtual ~EventConverter_TBSA();

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

		std::vector<stream::pos> offPatSeg; ///< Offset of each pattern

	protected:
		stream::output& content;     ///< Where to write data
		const Music& music;          ///< Song being written
		stream::string cachedEvent;  ///< Event to be written out later
		unsigned int cachedDelay;    ///< Delay to write before next cachedEvent flush
		unsigned int curAdvance;     ///< Current advancement amount (rows incremented per event)
		uint8_t curVolume;           ///< Current volume on this track/channel
		uint8_t curPatch;            ///< Current instrument on this track/channel
		int curFinetune;             ///< Current finetune value

		/// Write out the current delay
		void flushEvent(bool final);

		/// Write out a volume-change event
		void setVolume(uint8_t newVolume);
};


std::string MusicType_TBSA::code() const
{
	return "tbsa-doofus";
}

std::string MusicType_TBSA::friendlyName() const
{
	return "The Bone Shaker Architect";
}

std::vector<std::string> MusicType_TBSA::fileExtensions() const
{
	return {
		"bsa",
	};
}

MusicType::Caps MusicType_TBSA::caps() const
{
	return
		Caps::InstOPL
		| Caps::HasEvents
		| Caps::HasPatterns
		| Caps::HardwareOPL2
	;
}

MusicType::Certainty MusicType_TBSA::isInstance(stream::input& content) const
{
	content.seekg(0, stream::start);
	std::string sig;
	content >> fixedLength(sig, 8);

	// Invalid signature
	// TESTED BY: mus_tbsa_doofus_isinstance_c01
	if (sig.compare("TBSA0.01") != 0) return Certainty::DefinitelyNo;

	// TESTED BY: mus_tbsa_doofus_isinstance_c00
	return Certainty::DefinitelyYes;
}

std::unique_ptr<Music> MusicType_TBSA::read(stream::input& content,
	SuppData& suppData) const
{
	auto music = std::make_unique<Music>();
	music->patches = std::make_shared<PatchBank>();

	// All TBSA files seem to be in 4/4 time?
	music->initialTempo.beatsPerBar = 4;
	music->initialTempo.beatLength = 4;
	music->initialTempo.ticksPerBeat = 2;
	music->ticksPerTrack = 64;

	for (unsigned int c = 0; c < TBSA_CHANNEL_COUNT; c++) {
		TrackInfo t;
		if (c < 6) {
			t.channelType = TrackInfo::ChannelType::OPL;
			t.channelIndex = c;
		} else if (c < 11) {
			t.channelType = TrackInfo::ChannelType::OPLPerc;
			t.channelIndex = 4 - (c - 6);
		} else {
			t.channelType = TrackInfo::ChannelType::Unused;
			t.channelIndex = c - 11;
		}
		music->trackInfo.push_back(t);
	}

	content.seekg(8, stream::start);
	uint16_t offOrderPtrs, offUnknown2, offUnknown3, offUnknown4, offInstPtrs;
	uint16_t offPattPtrs;
	content
		>> u16le(offOrderPtrs)
		>> u16le(offUnknown2)
		>> u16le(offUnknown3)
		>> u16le(offUnknown4)
		>> u16le(offInstPtrs)
		>> u16le(offPattPtrs)
	;
	music->initialTempo.module(6, TBSA_TEMPO);

	// Read the order list pointers
	std::vector<stream::pos> offOrderLists;
	content.seekg(offOrderPtrs, stream::start);
	for (unsigned int i = 0; i < TBSA_MAX_ORDERS; i++) {
		stream::pos p;
		content >> u16le(p);
		if (p == 0xFFFF) break;
		offOrderLists.push_back(p);
	}

	// Read the instrument pointers
	std::vector<stream::pos> offInsts;
	content.seekg(offInstPtrs, stream::start);
	for (unsigned int i = 0; i < TBSA_MAX_INSTS; i++) {
		stream::pos p;
		content >> u16le(p);
		if (p == 0xFFFF) break;
		offInsts.push_back(p);
	}

	// Read the pattern-segment pointers
	std::vector<stream::pos> offPatts;
	content.seekg(offPattPtrs, stream::start);
	for (unsigned int i = 0; i < TBSA_MAX_PATTS; i++) {
		stream::pos p;
		content >> u16le(p);
		if (p == 0xFFFF) break;
		offPatts.push_back(p);
	}

	// Read the order pointers
	std::vector<stream::pos> offOrders;
	for (auto& i : offOrderLists) {
		content.seekg(i, stream::start);
		uint8_t orderCount, unknown;
		content
			>> u8(orderCount)
			>> u8(unknown)
		;
		for (unsigned int j = 0; j < orderCount; j++) {
			stream::pos p;
			content >> u16le(p);
			offOrders.push_back(p);
		}
	}

	// Read the instruments
	for (auto& i : offInsts) {
		content.seekg(i, stream::start);
		uint8_t inst[20];
		content.read(inst, 20);

		auto patch = std::make_shared<OPLPatch>();
		patch->defaultVolume = 255;

		patch->m.enableTremolo = inst[9] & 1;
		patch->m.enableVibrato = inst[6] & 1;
		patch->m.enableSustain = inst[2] & 1;
		patch->m.enableKSR     = inst[3] & 1;
		patch->m.freqMult      = inst[4] & 0x0F;
		patch->m.scaleLevel    = inst[8] & 3;
		patch->m.outputLevel   = inst[7] + 2;
		patch->m.attackRate    = inst[0] >> 4;
		patch->m.decayRate     = inst[0] & 0x0F;
		patch->m.sustainRate   = inst[1] >> 4;
		patch->m.releaseRate   = inst[1] & 0x0F;
		patch->m.waveSelect    = inst[11] & 0x07;

		patch->c.enableTremolo = (inst[15] >> 3) & 1; // use overflow from KSR field
		patch->c.enableVibrato = (inst[15] >> 2) & 1; // use overflow from KSR field
		patch->c.enableSustain = inst[14] & 1;
		patch->c.enableKSR     = inst[15] & 1;
		patch->c.freqMult      = inst[16] & 0x0F;
		patch->c.scaleLevel    = inst[18] & 3;
		patch->c.outputLevel   = inst[17] + 2;
		patch->c.attackRate    = inst[12] >> 4;
		patch->c.decayRate     = inst[12] & 0x0F;
		patch->c.sustainRate   = inst[13] >> 4;
		patch->c.releaseRate   = inst[13] & 0x0F;
		patch->c.waveSelect    = inst[19] & 0x07;

		patch->feedback         = inst[5] & 0x07;
		patch->connection       = inst[10] & 1;
		patch->rhythm           = OPLPatch::Rhythm::Melodic;

		music->patches->push_back(patch);
	}

	// Read the orders and patterns
	std::map<std::string, unsigned int> patternCodes;

	bool finished = false;
	for (unsigned int order = 0; order < TBSA_MAX_ORD_LEN; order++) {
		std::vector<uint8_t> patsegList;
		for (auto& i : offOrders) {
			content.seekg(i + order, stream::start);
			uint8_t patsegIndex;
			content >> u8(patsegIndex);
			if (patsegIndex == 0xFE) {
				finished = true;
				break;
			}
			patsegList.push_back(patsegIndex);
		}
		if (finished) break;

		// See if this combination of pattern segments has been loaded before
		std::string key;
		for (auto& p : patsegList) {
			key.append((char)p, 1);
		}
		auto match = patternCodes.find(key);
		if (match != patternCodes.end()) {
			// Yes it has, so just reference that pattern instead
			music->patternOrder.push_back(match->second);
			continue;
		}
		// No, so add it to the list in case it gets referenced again
		unsigned long patternIndex = music->patterns.size();
		patternCodes[key] = patternIndex;
		music->patternOrder.push_back(patternIndex);

		music->patterns.emplace_back();
		auto& pattern = music->patterns.back();

		// patsegList now contains a list of all the segments, one per track, that
		// make up this pattern.
		unsigned int trackIndex = 0;
		unsigned int patternLength = 0;
		for (auto& p : patsegList) {
			if (p >= offPatts.size()) {
				std::cerr << "TBSA: Tried to load pattern segment " << p
					<< " but there are only " << offPatts.size()
					<< " segments in the file!";
				continue;
			}
			pattern.emplace_back();
			auto& t = pattern.back();

			if ((patternIndex == 0) && (trackIndex == 0)) {
				// Set standard settings
				// OPL3 is off.  We don't add an EnableOPL3 event with the value set
				// to zero as that event requires an OPL3 to be present.
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::Type::EnableDeepTremolo;
					ev->value = 0;
					t.push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::Type::EnableDeepVibrato;
					ev->value = 0;
					t.push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::Type::EnableWaveSel;
					ev->value = 1;
					t.push_back(te);
				}
				{
					TrackEvent te;
					te.delay = 0;
					ConfigurationEvent *ev = new ConfigurationEvent();
					te.event.reset(ev);
					ev->configType = ConfigurationEvent::Type::EnableRhythm;
					ev->value = 1;
					t.push_back(te);
				}
			}

			// Decode the pattern segment
			content.seekg(offPatts[p], stream::start);
			int lastVolume = -1, lastInstrument = 0, lastIncrement = 1;
			double lastShift = 0.0;
			int delay = 0;
			bool noteOn = false;
			unsigned int patsegLength = 0;
			for (int row = 0; row < TBSA_MAX_PATSEG_LEN; row++) {
				uint8_t code;
				content >> u8(code);
				if (code == 0xFF) break;
				uint8_t command = code >> 5;
				uint8_t value = code & 0x1F;

				switch (command) {
					case 0:   // 0x,1x
					case 1:   // 2x,3x
					case 2: { // 4x,5x: note
						auto ev = std::make_shared<NoteOnEvent>();
						ev->instrument = lastInstrument;
						ev->milliHertz = midiToFreq(code+12+lastShift);
						ev->velocity = lastVolume;

						TrackEvent te;
						te.delay = delay;
						patsegLength += delay;
						delay = lastIncrement;
						te.event = ev;
						t.push_back(te);

						noteOn = true;
						break;
					}
					//case 3:   // 6x,7x: ?
					//	break;
					case 4:   // 8x,9x: set instrument
						lastInstrument = value;
						break;
					case 5:   // Ax,Bx: set increment
						lastIncrement = value + 1;
						break;
					case 6:   // Cx,Dx: set increment
						lastIncrement = 32+value + 1;
						break;
					case 7:   // Ex,Fx: ?
						switch (code) {
							case 0xF4: // fine tune down
							case 0xF5:
							case 0xF6:
							case 0xF7:
							case 0xF8:
							case 0xF9:
							case 0xFA:
							case 0xFB:
							case 0xFC:
								lastShift = 0.1 * (code - 0xFD) * 0.25;
								break;
							case 0xFD: // set volume
								content >> u8(value);
								lastVolume = log_volume_to_lin_velocity(value, 127);
								if (noteOn) {
									auto ev = std::make_shared<EffectEvent>();
									ev->type = EffectEvent::Type::Volume;
									ev->data = lastVolume;
									TrackEvent te;
									te.delay = delay;
									patsegLength += delay;
									delay = 0;
									te.event = ev;
									t.push_back(te);
								}
								break;
							case 0xFE: { // note off
								auto ev = std::make_shared<NoteOffEvent>();

								TrackEvent te;
								te.delay = delay;
								patsegLength += delay;
								delay = lastIncrement;
								te.event = ev;
								t.push_back(te);

								noteOn = false;
								break;
							}
							default:
								delay += lastIncrement;
								std::cout << "TBSA: Unrecognised extended command: "
									<< std::hex << (int)code << std::dec << "\n";
								break;
						}
						break;
					default:
						std::cout << "TBSA: Unrecognised command: "
							<< std::hex << (int)code << std::dec << "\n";
						break;
				}
			} // for each byte in the pattern segment data

			// There could be a trailing delay here, but it's not actually used
			// unless there's another event (so technically it's not really a
			// trailing delay.)  Since we're at the end of the track, there's no
			// other event coming so we can disregard this delay.
			// If we don't do this, the pattern-break events after every pattern
			// in Doofus episode 2 won't appear and there will be a two-row delay
			// after every pattern.

			if (patsegLength > patternLength) patternLength = patsegLength;
			trackIndex++;
		} // for each pattern segment

		// If this pattern is short, add a pattern-break event
		if (patternLength < TBSA_PATTERN_LENGTH) {
			if (pattern.size() > 0) {
				auto& t = pattern.at(0);

				// Find out how long all the events in the track go for
				unsigned int totalDelay = 0;
				for (auto& te : t) {
					totalDelay += te.delay;
				}

				// If this assertion fails, then somehow we didn't add up the length of
				// the track correctly, as patternLength is supposed to be the largest
				// value of all the pattern segment lengths.
				assert(patternLength >= totalDelay);

				// Now add a pattern break with enough of a delay that it will trigger
				// the right amount of time after the last event on this track.
				auto ev = std::make_shared<GotoEvent>();
				ev->type = GotoEvent::Type::NextPattern;
				ev->targetRow = 0;

				TrackEvent te;
				te.delay = patternLength - totalDelay;
				te.event = ev;

				t.push_back(te);
			}
		}
	} // for each entry in the order list
	oplDenormalisePerc(*music, OPLNormaliseType::CarFromMod);

	return music;
}

void MusicType_TBSA::write(stream::output& content, SuppData& suppData,
	const Music& music, WriteFlags flags) const
{
	requirePatches<OPLPatch>(*music.patches);
	if (music.patches->size() >= 31) {
		throw bad_patch("TBSA files have a maximum of 31 instruments.");
	}

	// Swap operators for required percussive patches
	std::shared_ptr<PatchBank> normPatches = oplNormalisePerc(music, OPLNormaliseType::CarFromMod);

	content
		<< nullPadded("TBSA0.01", 8)
		<< u16le(0xabcd) // placeholder
		<< u16le(0xabcd) // placeholder
		<< u16le(0xabcd) // placeholder
		<< u16le(0xabcd) // placeholder
		<< u16le(0xabcd) // placeholder
		<< u16le(0xabcd) // placeholder
	;

	// Order list
	stream::pos offOrderPtrs = content.tellp();
	content
		<< u16le(0xabcd) // placeholder
		<< u16le(0xFFFF) // end-of-list
	;

	// Unknown list 1
	stream::pos offUnknown1 = content.tellp();
	content << u16le(0xFFFF); // end-of-list

	// Unknown list 2
	stream::pos offUnknown2 = content.tellp();
	content << u16le(0xFFFF); // end-of-list

	// Unknown list 3
	stream::pos offUnknown3 = content.tellp();
	content << u16le(0xFFFF); // end-of-list

	// Instrument pointers
	stream::pos offInstPtrs = content.tellp();
	for (auto& i : *music.patches) {
		content << u16le(0xaaaa); // placeholder
	}
	content << u16le(0xFFFF); // end-of-list

	unsigned int numPatterns = music.patterns.size();
	unsigned int numTracks = music.patterns[0].size();

	// Pattern segment pointers
	stream::pos offPattPtrs = content.tellp();
	unsigned long numPatternSegments = numPatterns * numTracks;
	for (unsigned int i = 0; i < numPatternSegments; i++) {
		content << u16le(0xbbbb); // placeholder
	}
	content << u16le(0xFFFF); // end-of-list

	// Order pointers (which patsegs to play for each track)
	stream::pos offOrderList = content.tellp();
	content
		<< u8(numTracks)
		<< u8(0)           // unknown
	;
	for (unsigned int i = 0; i < numTracks; i++) {
		content << u16le(0xcccc); // placeholder
	}

	// Instruments
	std::vector<stream::pos> offInsts;
	for (auto& i : *normPatches) {
		auto oplPatch = dynamic_cast<const OPLPatch*>(i.get());
		if (!oplPatch) continue;

		offInsts.push_back(content.tellp());
		uint8_t inst[20];
		inst[0] = (oplPatch->m.attackRate << 4) | oplPatch->m.decayRate;
		inst[1] = (oplPatch->m.sustainRate << 4) | oplPatch->m.releaseRate;
		inst[2] = oplPatch->m.enableSustain ? 1 : 0;
		inst[3] = oplPatch->m.enableKSR ? 1 : 0;
		inst[4] = oplPatch->m.freqMult;
		inst[5] = oplPatch->feedback;
		inst[6] = oplPatch->m.enableVibrato;
		inst[7] = (oplPatch->m.outputLevel > 2) ? oplPatch->m.outputLevel - 2 : 0;
		inst[8] = oplPatch->m.scaleLevel;
		inst[9] = oplPatch->m.enableTremolo ? 1 : 0;
		inst[10] = oplPatch->connection;
		inst[11] = oplPatch->m.waveSelect;
		inst[12] = (oplPatch->c.attackRate << 4) | oplPatch->c.decayRate;
		inst[13] = (oplPatch->c.sustainRate << 4) | oplPatch->c.releaseRate;
		inst[14] = oplPatch->c.enableSustain ? 1 : 0;
		inst[15] = oplPatch->c.enableKSR ? 1 : 0;
		inst[16] = oplPatch->c.freqMult;
		inst[17] = (oplPatch->c.outputLevel > 2) ? oplPatch->c.outputLevel - 2 : 0;
		inst[18] = oplPatch->c.scaleLevel;
		inst[19] = oplPatch->c.waveSelect;
		content.write(inst, 20);
	}

	// Track order numbers
	std::vector<stream::pos> orderPointers;
	for (unsigned int trackIndex = 0; trackIndex < numTracks; trackIndex++) {
		// Write all the order index numbers for this track

		orderPointers.push_back(content.tellp());

		for (auto& patternIndex : music.patternOrder) {
			unsigned int targetPatSeg = patternIndex * numTracks + trackIndex;
			content << u8(targetPatSeg);
		}
		content << u8(0xFE);
	}

	// Write out all the pattern segments
	EventConverter_TBSA conv(content, music);
	conv.offPatSeg.push_back(content.tellp());
	conv.handleAllEvents(EventHandler::Pattern_Track_Row, music);
	// Remove last element (it will point to EOF and we don't need to write it)
	conv.offPatSeg.pop_back();

	content.truncate_here();

	// Go back and write out all the file pointers
	content.seekp(8, stream::start);
	content
		<< u16le(offOrderPtrs)
		<< u16le(offUnknown1)
		<< u16le(offUnknown2)
		<< u16le(offUnknown3)
		<< u16le(offInstPtrs)
		<< u16le(offPattPtrs)
	;

	content.seekp(offOrderPtrs, stream::start);
	content << u16le(offOrderList);

	content.seekp(offInstPtrs, stream::start);
	for (std::vector<stream::pos>::const_iterator
		i = offInsts.begin(); i != offInsts.end(); i++
	) {
		content << u16le(*i);
	}

	content.seekp(offOrderList + 2, stream::start);
	for (std::vector<stream::pos>::const_iterator
		i = orderPointers.begin(); i != orderPointers.end(); i++
	) {
		content << u16le(*i);
	}

	content.seekp(offPattPtrs, stream::start);
	for (std::vector<stream::pos>::const_iterator
		i = conv.offPatSeg.begin(); i != conv.offPatSeg.end(); i++
	) {
		content << u16le(*i);
	}
	return;
}

SuppFilenames MusicType_TBSA::getRequiredSupps(stream::input& content,
	const std::string& filename) const
{
	// No supplemental types/empty list
	return {};
}

std::vector<Attribute> MusicType_TBSA::supportedAttributes() const
{
	// No supported metadata
	return std::vector<Attribute>();
}


EventConverter_TBSA::EventConverter_TBSA(stream::output& content,
	const Music& music)
	:	content(content),
		music(music),
		cachedDelay(0),
		curAdvance(255), // These values are all out of range
		curVolume(255),  // so that they'll get set/written
		curPatch(255),   // on the first note in the track
		curFinetune(0)
{
}

EventConverter_TBSA::~EventConverter_TBSA()
{
}

void EventConverter_TBSA::endOfTrack(unsigned long delay)
{
	this->cachedDelay += delay;
	this->flushEvent(false);

	this->content << u8(0xFF);
	this->offPatSeg.push_back(this->content.tellp());

	this->cachedDelay = 0;

	// Reset for next track.  These values are all out of range so the first
	// note in the next track will cause the values to be written out to the file.
	this->curAdvance = 255;
	this->curVolume = 255;
	this->curPatch = 255;
	this->curFinetune = 0;
	return;
}

void EventConverter_TBSA::endOfPattern(unsigned long delay)
{
	return;
}

void EventConverter_TBSA::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	this->cachedDelay += delay;
	std::cerr << "TBSA: Does not support tempo changes\n";
	return;
}

void EventConverter_TBSA::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	this->cachedDelay += delay;
	this->flushEvent(false);

	double midiNote = freqToMIDI(ev->milliHertz);

	if (midiNote < 12) midiNote = 0;
	else if (midiNote > 127) midiNote = 127;
	else midiNote -= 12;

	// Handle any pitchbend
	unsigned int midiNoteMain = ceil(midiNote);
	double midiFrac = midiNote - midiNoteMain; // negative
	midiFrac *= 40;
	int step = round(midiFrac);
	assert(step <= 0);
	if ((step > -10) && (step < 0) && (step != this->curFinetune)) {
		this->content << u8(0xFD + step);
		this->curFinetune = step;
	}

	// Set patch
	if (this->curPatch != ev->instrument) {
		this->content << u8(0x80 | ev->instrument);
		this->curPatch = ev->instrument;
	}

	assert(ev->velocity < 256);
	this->setVolume(ev->velocity); // no-op if there's no change

	this->cachedEvent
		<< u8(midiNoteMain)
	;
	return;
}

void EventConverter_TBSA::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	this->cachedDelay += delay;
	this->flushEvent(false);

	this->cachedEvent
		<< u8(0xFE)
	;
	return;
}

void EventConverter_TBSA::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
	this->cachedDelay += delay;

	switch (ev->type) {
		case EffectEvent::Type::PitchbendNote:
			std::cerr << "TBSA: Pitch bends not supported, ignoring event\n";
			break;
		case EffectEvent::Type::Volume:
			this->setVolume(ev->data);
			break;
	}
	return;
}

void EventConverter_TBSA::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const GotoEvent *ev)
{
	this->cachedDelay += delay;
	// Not supported by this format?
	return;
}

void EventConverter_TBSA::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex,
	const ConfigurationEvent *ev)
{
	this->cachedDelay += delay;
	switch (ev->configType) {
		case ConfigurationEvent::Type::EmptyEvent:
			break;
		case ConfigurationEvent::Type::EnableOPL3:
			if (ev->value != 0) std::cerr << "TBSA: OPL3 cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableDeepTremolo:
			if (ev->value != 0) std::cerr << "TBSA: Deep tremolo cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableDeepVibrato:
			if (ev->value != 0) std::cerr << "TBSA: Deep vibrato cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableRhythm:
			if (ev->value != 0) std::cerr << "TBSA: Rhythm mode cannot be enabled, ignoring event.\n";
			break;
		case ConfigurationEvent::Type::EnableWaveSel:
			if (ev->value != 1) std::cerr << "TBSA: Wave selection registers cannot be disabled, ignoring event.\n";
			break;
	}
	return;
}

void EventConverter_TBSA::flushEvent(bool final)
{
	if (this->cachedDelay || final) {
		uint8_t steps;
		if (this->cachedDelay) steps = this->cachedDelay - 1; else steps = 0;
		if (steps > 63) {
			throw format_limitation(createString("TBSA: Cannot handle delays of "
				"more than 64 rows (tried to write delay of " << this->cachedDelay
				<< " rows)."));
		}
		if (steps != this->curAdvance) {
			// Write the new delay before the cached event
			if (steps < 32) {
				this->content << u8(0xA0 | steps);
			} else if (steps < 64) {
				this->content << u8(0xC0 | (steps - 32));
			}
			this->curAdvance = steps;
		}
		this->cachedDelay = 0;

		this->content << this->cachedEvent.data;
		this->cachedEvent.seekp(0, stream::start);
		this->cachedEvent.data.clear();
	}
	return;
}

void EventConverter_TBSA::setVolume(uint8_t newVolume)
{
	newVolume >>= 1;  // 0..255 -> 0..127
	if (newVolume != this->curVolume) {
		this->content
			<< u8(0xFD)
			<< u8(newVolume)
		;
		this->curVolume = newVolume;
	}
	return;
}
