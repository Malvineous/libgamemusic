/**
 * @file   mus-klm-wacky.cpp
 * @brief  Support for Wacky Wheels Adlib (.klm) format.
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
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/util-opl.hpp>
#include "mus-klm-wacky.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Adlib -> Hz conversion factor to use
#define KLM_FNUM_CONVERSION  OPL_FNUM_DEFAULT

#define KLM_CHANNEL_COUNT (6+5)

class EventConverter_KLM: virtual public EventHandler
{
	public:
		/// Prepare to convert events into KLM data sent to a stream.
		/**
		 * @param output
		 *   Output stream the KLM data will be written to.
		 */
		EventConverter_KLM(stream::output_sptr output);

		/// Destructor.
		virtual ~EventConverter_KLM();

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

	protected:
		stream::output_sptr output;          ///< Where to write KLM file
		unsigned long lastDelay;             ///< Tick value from previous event
		uint8_t patchMap[KLM_CHANNEL_COUNT]; ///< Which instruments are in use on which channel
		uint8_t volMap[KLM_CHANNEL_COUNT];   ///< Volume set on each channel
		bool atStart;                        ///< At time=0?

		/// Write out the current delay, if there is one (this->lastDelay != 0)
		void writeDelay();
};

std::string MusicType_KLM::getCode() const
{
	return "klm-wacky";
}

std::string MusicType_KLM::getFriendlyName() const
{
	return "Wacky Wheels Adlib Music File";
}

std::vector<std::string> MusicType_KLM::getFileExtensions() const
{
	std::vector<std::string> vcExtensions;
	vcExtensions.push_back("klm");
	return vcExtensions;
}

unsigned long MusicType_KLM::getCaps() const
{
	return
		InstOPL
		| HasEvents
	;
}

MusicType::Certainty MusicType_KLM::isInstance(stream::input_sptr input) const
{
	stream::pos lenFile = input->size();
	// Too short
	// TESTED BY: mus_klm_wacky_isinstance_c09
	if (lenFile < 5) return MusicType::DefinitelyNo;

	uint16_t offMusic;
	input->seekg(3, stream::start);
	input >> u16le(offMusic);

	// Make sure the instruments are of the correct length
	// TESTED BY: mus_klm_wacky_isinstance_c01
	if ((offMusic - 5) % 11 != 0) return MusicType::DefinitelyNo;

	// Make sure the song data offset is contained within the file
	// TESTED BY: mus_klm_wacky_isinstance_c02
	if (offMusic >= lenFile) return MusicType::DefinitelyNo;

	// Read each instrument and pick out invalid values
	int instRead = 5;
	while (instRead < offMusic) {
		assert(lenFile > 11);  // if this fails, the logic above is wrong
		uint8_t instrument[11];
		input->read((char *)instrument, 11);
		instRead += 11;

		// Upper five bits of base register 0xE0 are not used
		// TESTED BY: mus_klm_wacky_isinstance_c06
		if (instrument[8] & 0xF8) return MusicType::DefinitelyNo;
		// TESTED BY: mus_klm_wacky_isinstance_c07
		if (instrument[9] & 0xF8) return MusicType::DefinitelyNo;

		// Upper two bits of base register 0xC0 are not used
//		if (instrument[10] & 0xC0) return MusicType::DefinitelyNo;

		// If we're here, no invalid bits were set
		// TESTED BY: mus_klm_wacky_isinstance_c08
	}

	// This should always work unless there was an obscure I/O error
	assert(input->tellg() == offMusic);
	lenFile -= instRead;

	// Skim-read data until EOF
	while (lenFile) {
		uint8_t code;
		uint8_t lenEvent;
		input >> u8(code);
		lenFile--;
		switch (code & 0xF0) {
			case 0x00: lenEvent = 0; break;
			case 0x10:
				if (code < 0x17) lenEvent = 2;
				else lenEvent = 0;
				break;
			case 0x20: lenEvent = 1; break;
			case 0x30: lenEvent = 1; break;
			case 0x40: lenEvent = 0; break;
			case 0xF0:
				switch (code) {
					case 0xFD: lenEvent = 1; break;
					case 0xFE: lenEvent = 2; break;
					case 0xFF: lenEvent = 0; break;
					default:
						// Invalid 0xF0 event type
						// TESTED BY: mus_klm_wacky_isinstance_c03
						return MusicType::DefinitelyNo;
				}
				break;
			default:
				// Invalid event type
				// TESTED BY: mus_klm_wacky_isinstance_c04
				return MusicType::DefinitelyNo;
		}
		// Truncated event
		// TESTED BY: mus_klm_wacky_isinstance_c05
		if (lenFile < lenEvent) return MusicType::DefinitelyNo;
		if (code == 0xFF) break;
		input->seekg(lenEvent, stream::cur);
		lenFile -= lenEvent;
	}

	// TESTED BY: mus_klm_wacky_isinstance_c00
	return MusicType::DefinitelyYes;
}

MusicPtr MusicType_KLM::read(stream::input_sptr input, SuppData& suppData) const
{
	// Make sure we're at the start, as we'll often be near the end if
	// isInstance() was just called.
	input->seekg(0, stream::start);

	MusicPtr music(new Music());
	PatchBankPtr patches(new PatchBank());
	music->patches = patches;

	uint8_t patchMap[KLM_CHANNEL_COUNT]; ///< Which instruments are in use on which channel
	int freqMap[KLM_CHANNEL_COUNT];      ///< Frequency set on each channel
	uint8_t volMap[KLM_CHANNEL_COUNT];   ///< Volume set on each channel
	bool noteOn[KLM_CHANNEL_COUNT];      ///< Is a note playing on this channel?

	memset(patchMap, 0xFF, sizeof(patchMap));
	memset(freqMap, 0, sizeof(freqMap));
	memset(volMap, 0xFF, sizeof(volMap));
	memset(noteOn, 0, sizeof(noteOn));

	freqMap[12] = 142;
	freqMap[13] = 142;

	uint16_t tempo;
	uint8_t unk1;
	uint16_t offSong;
	input
		>> u16le(tempo)
		>> u8(unk1)
		>> u16le(offSong)
	;
	music->initialTempo.hertz(tempo);
	music->initialTempo.ticksPerQuarterNote(128); ///< @todo see if this value is stored in the file somewhere

	// Read the instruments
	int numInstruments = (offSong - 5) / 11;
	patches->reserve(numInstruments);
	for (int i = 0; i < numInstruments; i++) {
		OPLPatchPtr patch(new OPLPatch());
		uint8_t inst[11];
		input->read((char *)inst, 11);

		OPLOperator *o = &patch->m;
/*
off cmf klmoff
0   20  6
1   23   7
2   40  0
3   43   1
4   60  2
5   63   3
6   80  4
7   83   5
8   E0  8
9   E3   9
10  C0  10

off klm
0   40
1   43
2   60
3   63
4   80
5   83
6   20
7   23
8   E0
9   E3
10  C0
 */
		for (int op = 0; op < 2; op++) {
			o->enableTremolo = (inst[6 + op] >> 7) & 1;
			o->enableVibrato = (inst[6 + op] >> 6) & 1;
			o->enableSustain = (inst[6 + op] >> 5) & 1;
			o->enableKSR     = (inst[6 + op] >> 4) & 1;
			o->freqMult      =  inst[6 + op] & 0x0F;
			o->scaleLevel    =  inst[0 + op] >> 6;
			o->outputLevel   =  inst[0 + op] & 0x3F;
			o->attackRate    =  inst[2 + op] >> 4;
			o->decayRate     =  inst[2 + op] & 0x0F;
			o->sustainRate   =  inst[4 + op] >> 4;
			o->releaseRate   =  inst[4 + op] & 0x0F;
			o->waveSelect    =  inst[8 + op] & 0x07;
			o = &patch->c;
		}
		patch->feedback    = (inst[10] >> 1) & 0x07;
		patch->connection  =  inst[10] & 1;
		patch->rhythm      = OPLPatch::Melodic;

///TEMP
		switch (i) {
			case 7: // snare->hihat instrument
//				patch->c = patch->m;
//				patch->rhythm = 4;
				//memset(&patch->m, 0, sizeof(patch->m));
				//memset(&patch->c, 0, sizeof(patch->c));
				break;
/*			case 6:
				memset(&patch->m, 0, sizeof(patch->m));
				memset(&patch->c, 0, sizeof(patch->c));
				patch->rhythm = 1;
				break;*/
		}
///ENDTEMP
		patches->push_back(patch);
	}

	// Make sure we've reached the start of the song data
	assert(input->tellg() == offSong);

	unsigned long lastDelay[KLM_CHANNEL_COUNT];
	PatternPtr pattern(new Pattern());
	for (unsigned int c = 0; c < KLM_CHANNEL_COUNT; c++) {
		TrackInfo t;
		if (c < 6) {
			t.channelType = TrackInfo::OPLChannel;
			t.channelIndex = c;
		} else if (c < 11) {
			t.channelType = TrackInfo::OPLPercChannel;
			t.channelIndex = 4 - (c - 6);
		} else {
			t.channelType = TrackInfo::UnusedChannel;
			t.channelIndex = c - 11;
		}
		music->trackInfo.push_back(t);

		TrackPtr track(new Track());
		pattern->push_back(track);
		lastDelay[c] = 0;
	}
	music->patterns.push_back(pattern);
	music->patternOrder.push_back(0);

	TrackPtr configTrack = pattern->at(0);
	// Set rhythm mode
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableRhythm;
		ev->value = 1;
		configTrack->push_back(te);
	}

	// Set wavesel
	{
		TrackEvent te;
		te.delay = 0;
		ConfigurationEvent *ev = new ConfigurationEvent();
		te.event.reset(ev);
		ev->configType = ConfigurationEvent::EnableWaveSel;
		ev->value = 1;
		configTrack->push_back(te);
	}

	unsigned long totalTicks = 0;
	bool eof = false;
	while (!eof) {
		uint8_t code;
		try {
			input >> u8(code);
		} catch (const stream::incomplete_read&) {
			// no point setting eof=true here
			break;
		}

		uint8_t channel = code & 0x0F;
//std::cout << "Got event 0x" << std::hex << (int)code << "\n";
		if ((code < 0xF0) && (channel >= KLM_CHANNEL_COUNT)) throw stream::error("Channel too large");
//		if ((code < 0xF0) && (channel == 6)) throw stream::error("Invalid channel");

		// Convert "KLM" channels into libgamemusic ones (order of rhythm percussion
		// is opposite.)  Note that KLM treats the bass drum as a normal instrument
		// (on KLM channel 0x06, i.e. with frequency bytes) whereas it still needs
		// to be moved to libgamemusic channel 10 to trigger the right OPL keyon bit.
		// 6 -> 10, 7 -> 9 ... 10 -> 6
		//if (channel > 5) channel = 10 - (channel - 6);
		//if (channel > 5) channel = 9+ (channel - 6);
		/// @todo Change this to update the instrument patches instead of having magic channel numbers
/*
		switch (channel) {
			case 6: gmchannel = 13; break;
			case 7: gmchannel = 12; break;
			case 8: gmchannel = 11; break;
			case 9: gmchannel = 10; break;
			case 10: gmchannel = 9; break;
			default: gmchannel = channel + 1; break;
		}
*/
		switch (code & 0xF0) {
			case 0x00: { // note off
				TrackEvent te;
				te.delay = lastDelay[channel];
				lastDelay[channel] = 0;
				NoteOffEvent *ev = new NoteOffEvent();
				te.event.reset(ev);
				pattern->at(channel)->push_back(te);
				noteOn[channel] = false;
				break;
			}
			case 0x10: { // set frequency
				uint8_t a0, b0;
				bool doNoteOn = true;
				if (code < 0x17) { // Chans 0-5 + bass drum 6
					input >> u8(a0) >> u8(b0);
					int fnum = ((b0 & 0x03) << 8) | a0;
					int block = (b0 >> 2) & 0x07;
					freqMap[channel] = fnumToMilliHertz(fnum, block, KLM_FNUM_CONVERSION);
					doNoteOn = b0 & 0x20; // keyon bit
				}
				if (doNoteOn) {
					if (noteOn[channel]) {
						// Note is already playing, do a pitchbend
						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						EffectEvent *ev = new EffectEvent();
						te.event.reset(ev);
						ev->type = EffectEvent::PitchbendNote;
						ev->data = freqMap[channel];
						pattern->at(channel)->push_back(te);
					} else {
						// Note is not playing, do a note-on
						TrackEvent te;
						te.delay = lastDelay[channel];
						lastDelay[channel] = 0;
						NoteOnEvent *ev = new NoteOnEvent();
						te.event.reset(ev);
						ev->instrument = patchMap[channel];
						ev->milliHertz = freqMap[channel];
						ev->velocity = volMap[channel];
						pattern->at(channel)->push_back(te);
						noteOn[channel] = true;
					}
				} else {
					TrackEvent te;
					te.delay = lastDelay[channel];
					lastDelay[channel] = 0;
					NoteOffEvent *ev = new NoteOffEvent();
					te.event.reset(ev);
					pattern->at(channel)->push_back(te);
					noteOn[channel] = false;
					break;
				}
				break;
			}
			case 0x20: { // set volume
				uint8_t newVol;
				input >> u8(newVol);
				if (newVol > 127) throw stream::error("Velocity out of range");
				newVol = log_volume_to_lin_velocity(newVol, 127);
				volMap[channel] = newVol;
// TODO: Figure out which operator this affects on the percussive channels
				TrackEvent te;
				te.delay = lastDelay[channel];
				lastDelay[channel] = 0;
				EffectEvent *ev = new EffectEvent();
				te.event.reset(ev);
				ev->type = EffectEvent::Volume;
				ev->data = newVol;
				pattern->at(channel)->push_back(te);
				break;
			}
			case 0x30: { // set instrument
				uint8_t data;
				input >> u8(data);
				patchMap[channel] = data;
				break;
			}
			case 0x40: { // note on
				TrackEvent te;
				te.delay = lastDelay[channel];
				lastDelay[channel] = 0;
				NoteOnEvent *ev = new NoteOnEvent();
				te.event.reset(ev);
				ev->instrument = patchMap[channel];
				ev->milliHertz = freqMap[channel];
				ev->velocity = volMap[channel];
				pattern->at(channel)->push_back(te);
				noteOn[channel] = true;
				break;
			}
			case 0xF0: {
				switch (code) {
					case 0xFD: { // normal delay (0..255)
						uint8_t data;
						input >> u8(data);
						for (unsigned int c = 0; c < KLM_CHANNEL_COUNT; c++) {
							lastDelay[c] += data;
						}
						totalTicks += data;
						break;
					}
					case 0xFE: { // large delay (0..65535)
						uint16_t data;
						input >> u16le(data);
						for (unsigned int c = 0; c < KLM_CHANNEL_COUNT; c++) {
							lastDelay[c] += data;
						}
						totalTicks += data;
						break;
					}
					case 0xFF: // end of song
						eof = true;
						continue;
					default:
						std::cerr << "Invalid delay event type 0x" << std::hex << (int)code
							<< std::dec << std::endl;
						throw stream::error("Invalid delay event type");
				}
				continue; // get next event, don't append one this iteration
			}
			default:
				std::cerr << "Invalid event type 0x" << std::hex << (int)code
					<< std::dec << std::endl;
				throw stream::error("Invalid event type");
		}
	}

	// Put dummy events at the end of each track so we don't lose any final delays
	for (unsigned int channel = 0; channel < KLM_CHANNEL_COUNT; channel++) {
		if (lastDelay[channel]) {
			TrackPtr t = pattern->at(channel);
			if (t->size() == 0) continue; // unused channel
			TrackEvent te;
			te.delay = lastDelay[channel];
			lastDelay[channel] = 0;
			ConfigurationEvent *ev = new ConfigurationEvent();
			te.event.reset(ev);
			ev->configType = ConfigurationEvent::EmptyEvent;
			ev->value = 0;
			t->push_back(te);
		}
	}

	music->ticksPerTrack = totalTicks;

	return music;
}

void MusicType_KLM::write(stream::output_sptr output, SuppData& suppData,
	MusicPtr music, unsigned int flags) const
{
	requirePatches<OPLPatch>(music->patches);
	if (music->patches->size() > 256) {
		throw format_limitation("KLM files have a maximum of 256 instruments.");
	}

	uint16_t musOffset = 5 + music->patches->size() * 11;
	output
		<< u16le(music->initialTempo.hertz())
		<< u8(1)    // unknown
		<< u16le(musOffset)
	;
	for (PatchBank::const_iterator
		i = music->patches->begin(); i != music->patches->end(); i++
	) {
		uint8_t inst[11];
		OPLPatchPtr patch = boost::dynamic_pointer_cast<OPLPatch>(*i);
		assert(patch);
		OPLOperator *o = &patch->m;
		for (int op = 0; op < 2; op++) {
			inst[6 + op] =
				((o->enableTremolo & 1) << 7) |
				((o->enableVibrato & 1) << 6) |
				((o->enableSustain & 1) << 5) |
				((o->enableKSR     & 1) << 4) |
				 (o->freqMult      & 0x0F)
			;
			inst[0 + op] = (o->scaleLevel << 6) | (o->outputLevel & 0x3F);
			inst[2 + op] = (o->attackRate << 4) | (o->decayRate & 0x0F);
			inst[4 + op] = (o->sustainRate << 4) | (o->releaseRate & 0x0F);
			inst[8 + op] =  o->waveSelect & 7;

			o = &patch->c;
		}
		inst[10] = ((patch->feedback & 7) << 1) | (patch->connection & 1);
		//inst[11] = inst[12] = inst[13] = inst[14] = inst[15] = 0;

		/// @todo handle these
		//patch->deepTremolo = false;
		//patch->deepVibrato = false;

		output->write((char *)inst, 11);
	}
/*
off cmf klmoff
0   20  6
1   23   7
2   40  0
3   43   1
4   60  2
5   63   3
6   80  4
7   83   5
8   E0  8
9   E3   9
10  C0  10

off klm
0   40
1   43
2   60
3   63
4   80
5   83
6   20
7   23
8   E0
9   E3
10  C0
 */
	EventConverter_KLM conv(output);
	conv.handleAllEvents(EventHandler::Pattern_Row_Track, music);

	output << u8(0xFF); // end of song

	// Set final filesize to this
	output->truncate_here();

	return;
}

SuppFilenames MusicType_KLM::getRequiredSupps(stream::input_sptr input,
	const std::string& filenameMusic) const
{
	// No supplemental types/empty list
	return SuppFilenames();
}

Metadata::MetadataTypes MusicType_KLM::getMetadataList() const
{
	Metadata::MetadataTypes types;
	return types;
}


EventConverter_KLM::EventConverter_KLM(stream::output_sptr output)
	:	output(output),
		lastDelay(0),
		atStart(true)
{
	memset(this->patchMap, 0xFF, sizeof(this->patchMap));
	memset(this->volMap, 0xFF, sizeof(this->volMap));
}

EventConverter_KLM::~EventConverter_KLM()
{
}

void EventConverter_KLM::endOfTrack(unsigned long delay)
{
	return;
}

void EventConverter_KLM::endOfPattern(unsigned long delay)
{
	this->lastDelay += delay;
	return;
}

void EventConverter_KLM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const TempoEvent *ev)
{
	// TODO: Set current multiplier for future delay modifications
	if (this->atStart) {
		int ticksPerSecond = 1000000 / ev->tempo.usPerTick;
		// First tempo event, update the header
		unsigned int pos = this->output->tellp(); // should nearly always be musOffset from the header
		this->output->seekp(0, stream::start);
		this->output << u16le(ticksPerSecond);
		this->output->seekp(pos, stream::start);
		//this->tempoMultiplier = 1;
		// Don't set this->atStart to false just in case we get multiple initial
		// tempo events
	} else {
		// Tempo has changed, but since this format doesn't support tempo changes
		// we will have to modify all future delay values.
		// TODO
		std::cout << "mus-klm-wacky: TODO - change tempo\n";
	}

/*
	assert(ev->usPerTick > 0);
	unsigned long n = ev->usPerTick * this->ticksPerQuarterNote;
	if (this->usPerQuarterNote != n) {
		if (!(this->midiFlags & MIDIFlags::BasicMIDIOnly)) {
			this->writeMIDINumber(ev->absTime - this->lastTick);
			this->midi->write("\xFF\x51\x03", 3);
			this->midi
				<< u8(this->usPerQuarterNote >> 16)
				<< u8(this->usPerQuarterNote >> 8)
				<< u8(this->usPerQuarterNote & 0xFF)
			;
		}
		this->usPerQuarterNote = n;
	}
*/
	return;
}

void EventConverter_KLM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOnEvent *ev)
{
	if (trackIndex >= KLM_CHANNEL_COUNT) throw stream::error("Too many channels");

	this->lastDelay += delay;
	this->writeDelay();

	uint8_t event;

	if (ev->instrument != this->patchMap[trackIndex]) {
		// Change the instrument first
		event = 0x30 | trackIndex;
		this->output
			<< u8(event)
			<< u8(ev->instrument)
		;
		this->patchMap[trackIndex] = ev->instrument;
	}

	if (ev->velocity != this->volMap[trackIndex]) {
		// Change the volume
		event = 0x20 | trackIndex;
		this->output
			<< u8(event)
			<< u8(ev->velocity >> 1)
		;
		this->volMap[trackIndex] = ev->velocity;
	}

	unsigned int fnum, block;
	milliHertzToFnum(ev->milliHertz, &fnum, &block, KLM_FNUM_CONVERSION);

	event = 0x10 | trackIndex;
	uint8_t a0 = fnum & 0xFF;
	uint8_t b0 =
		OPLBIT_KEYON  // keyon enabled
		| (block << 2) // and the octave
		| ((fnum >> 8) & 0x03) // plus the upper two bits of fnum
	;

	// Write lower eight bits of note freq
	this->output
		<< u8(event)
		<< u8(a0)
		<< u8(b0)
	;

	return;
}

void EventConverter_KLM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const NoteOffEvent *ev)
{
	if (trackIndex >= KLM_CHANNEL_COUNT) throw stream::error("Too many channels");
	this->lastDelay += delay;
	this->writeDelay();

	uint8_t code = 0x00 | trackIndex;
	this->output << u8(code);

	return;
}

void EventConverter_KLM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const EffectEvent *ev)
{
/*
	if (this->flags & IntegerNotesOnly) return;

	uint8_t midiChannel = this->getMIDIchannel(ev->channel, MIDI_CHANNEL_COUNT);

	uint8_t note;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &note, &bend, this->activeNote[ev->channel]);
	/// @todo Handle pitch bending further than one note or if 'note' comes out
	/// differently to the currently playing note
	if (bend != this->currentPitchbend[midiChannel]) {
		uint32_t delay = ev->absTime - this->lastTick;
		this->writeMIDINumber(delay);
		uint8_t msb = (bend >> 7) & 0x7F;
		uint8_t lsb = bend & 0x7F;
		this->writeCommand(0xE0 | midiChannel);
		this->midi
			<< u8(lsb)
			<< u8(msb)
		;
		this->currentPitchbend[midiChannel] = bend;
	}
*/
	this->lastDelay += delay;
	//this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void EventConverter_KLM::handleEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, const ConfigurationEvent *ev)
{
	this->lastDelay += delay;
	switch (ev->configType) {
		case ConfigurationEvent::EnableRhythm:
			// Ignored, will set when rhythm channels are played
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			if (ev->value != 1) std::cerr << "Deep tremolo cannot be disabled in this format, ignoring event.\n";
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			if (ev->value != 1) std::cerr << "Deep vibrato cannot be disabled in this format, ignoring event.\n";
			break;
		default:
			break;
	}
	return;
}

void EventConverter_KLM::writeDelay()
{
	this->atStart = false; // no longer at first event
	while (this->lastDelay) {
		uint8_t d;
		if (this->lastDelay > 255) d = 255; else d = this->lastDelay;
		this->output << u8(0xFD) << u8(this->lastDelay);
		this->lastDelay -= d;
	}
	assert(this->lastDelay == 0);
	return;
}
