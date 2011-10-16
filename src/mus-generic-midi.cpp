/**
 * @file   mus-generic-midi.cpp
 * @brief  MusicReader and MusicWriter classes to process MIDI data.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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

#include <math.h> // pow
#include <camoto/iostream_helpers.hpp>
#include "mus-generic-midi.hpp"

using namespace camoto::gamemusic;

/// Frequency to use for playing channel 10 percussion
const int PERC_FREQ = 440000;

unsigned long camoto::gamemusic::midiToFreq(double midi)
{
	return 440000 * pow(2, (midi - 69.0) / 12.0);
}

void camoto::gamemusic::freqToMIDI(unsigned long milliHertz, uint8_t *note,
	int16_t *bend, uint8_t curNote)
{
	// Lower bound is clamped to MIDI note #0.  Could probably get lower with a
	// pitchbend but the difference is unlikely to be audible (8Hz is pretty much
	// below human hearing anyway.)
	if (milliHertz <= 8175) {
		*note = 0;
		*bend = 0;
		return;
	}
	double val = 12.0 * log2((double)milliHertz / 440000.0) + 69.0;
	// round to three decimal places
	val = round(val * 1000.0) / 1000.0;
	if (curNote == 0xFF) *note = round(val);
	else *note = curNote;
	*bend = (val - *note) * 4096;

	// If the pitchbend is out of range, just clamp it
	if (*bend < -8192) *bend = -8192;
	if (*bend > 8191) *bend = 8191;

	if (*note > 0x7F) {
		std::cout << "Error: Frequency " << (double)milliHertz / 1000 << " is too "
			"high (requires MIDI note " << (int)*note << ")" << std::endl;
		*note = 0x7F;
	}
	/// @todo Take into account current range of pitchbend and allow user to
	/// extend it to prevent clamping.

	/// @todo Figure out if this is a currently playing note, and try to maintain
	/// the bend to avoid playing a new note.  Maybe only necessary in the
	/// pitchbend callback rather than the noteon one.
	return;
}

MusicReader_GenericMIDI::MusicReader_GenericMIDI(MIDIFlags::Type midiFlags)
	throw () :
		tick(0),
		lastEvent(0),
		setTempo(false),
		midiFlags(midiFlags),
		ticksPerQuarterNote(192), // suitable default
		usPerQuarterNote(500000)  // MIDI default
{
	memset(this->patchMap, 0xFF, sizeof(this->patchMap));
	memset(this->percMap, 0xFF, sizeof(this->percMap));
	memset(this->currentInstrument, 0, sizeof(this->currentInstrument));
	memset(this->currentPitchbend, 0, sizeof(this->currentPitchbend));
	memset(this->activeNotes, 0, sizeof(this->activeNotes));
}

MusicReader_GenericMIDI::~MusicReader_GenericMIDI()
	throw ()
{
}

PatchBankPtr MusicReader_GenericMIDI::getPatchBank()
	throw ()
{
	if (this->patches) return this->patches;

	// Scan through the MIDI data and pick out the instruments used.
	memset(this->patchMap, 0xFF, sizeof(this->patchMap));
	memset(this->percMap, 0xFF, sizeof(this->percMap));
	int instCount = 0, percCount = 0;
	bool eof = false;

	this->rewind();
	do {

		uint32_t delay;
		try {
			this->readMIDINumber();
		} catch (const stream::incomplete_read&) {
			eof = true;
			break;
		}

		uint8_t event, evdata;
		this->midi >> u8(event);
		if (event & 0x80) {
			// If the high bit is set it's a normal event
			if ((event & 0xF0) != 0xF0) {
				// 0xF0 events do not change the running status
				this->lastEvent = event;
			}
			this->midi >> u8(evdata);
		} else {
			// The high bit is unset, so this is actually the first data
			// byte for a new event, of the same type as the last event.
			evdata = event;
			event = this->lastEvent;
		}

		// Handle short aftertouch events if the flag was given
		if (this->midiFlags & MIDIFlags::ShortAftertouch) {
			// Change key aftertouch to channel aftertouch
			if ((event & 0xF0) == 0xA0) event = 0xD0 | (event & 0x0F);
		}

		uint8_t midiChannel = event & 0x0F;
		switch (event & 0xF0) {
			case 0x90:   // Note on (two data bytes)
				if ((this->midiFlags & MIDIFlags::Channel10Perc) && (midiChannel == 10)) {
					// Use note number instead

					// See if we've used this instrument before
					bool found = false;
					for (int i = 0; i < percCount; i++) {
						if (this->percMap[i] == evdata) {
							found = true;
							break;
						}
					}
					if (!found) {
						// We haven't, so add it to the list of instruments)
						this->percMap[percCount++] = evdata;
					}
					// fall through to skip the velocity byte
				} // else fall through
			case 0x80:   // Note off (two data bytes)
			case 0xA0:   // Polyphonic key pressure (two data bytes)
			case 0xB0:   // Controller (two data bytes)
			case 0xE0:   // Pitch bend (two data bytes)
				this->midi->seekg(1, stream::cur);
				break;
			case 0xC0: { // Instrument change (one data byte)
				// See if we've used this instrument before
				bool found = false;
				for (int i = 0; i < instCount; i++) {
					if (this->patchMap[i] == evdata) {
						found = true;
						break;
					}
				}
				if (!found) {
					// We haven't, so add it to the list of instruments)
					this->patchMap[instCount++] = evdata;
				}
				break;
			}
			case 0xD0:   // Channel pressure (one data byte)
				break;
			case 0xF0:   // System message (arbitrary data bytes)
				switch (event) {
					case 0xF0: { // Sysex
						uint8_t iNextByte;
						do {
							this->midi >> u8(iNextByte);
						} while ((iNextByte & 0x80) == 0);
						// This will have read in the terminating EOX (0xF7) message too
						break;
					}
					case 0xF1: // MIDI Time Code Quarter Frame
						this->midi->seekg(1, stream::cur); // message data (ignored)
						break;
					case 0xF2: // Song position pointer
						this->midi->seekg(2, stream::cur); // message data (ignored)
						break;
					case 0xF3: // Song select
						this->midi->seekg(1, stream::cur); // message data (ignored)
						std::cout << "Warning: MIDI Song Select is not implemented." << std::endl;
						break;
					case 0xF6: // Tune request
						break;
					case 0xF7: // End of System Exclusive (EOX) - should never be read, should be absorbed by Sysex handling code
						break;

					// These messages are "real time", meaning they can be sent between the bytes of other messages - but we're
					// lazy and don't handle these here (hopefully they're not necessary in a MIDI file, and even less likely to
					// occur in a CMF.)
					case 0xF8: // Timing clock (sent 24 times per quarter note, only when playing)
					case 0xFA: // Start
					case 0xFB: // Continue
					case 0xFE: // Active sensing (sent every 300ms or MIDI connection assumed lost)
						break;
					case 0xFC: // Stop
						eof = true;
						break;
					case 0xFF: { // System reset, used as meta-events in a MIDI file
						uint8_t iEvent;
						this->midi >> u8(iEvent);
						uint32_t len;
						try {
							len = this->readMIDINumber();
						} catch (const stream::incomplete_read&) {
							eof = true;
							break;
						}
						this->midi->seekg(len, stream::cur); // message data (ignored)
						break;
					}
					default:
						break;
				}
				break;
			default:
				break;
		}
	} while (!eof);
	this->rewind();

	// Make sure there's at least one instrument set
	if ((instCount == 0) && (percCount == 0)) {
		this->patchMap[0] = 0; // grand piano
		instCount++;
	}

	this->patches.reset(new MIDIPatchBank());
	patches->setPatchCount(instCount + percCount);
	for (int i = 0; i < instCount; i++) {
		MIDIPatchPtr p(new MIDIPatch());
		p->midiPatch = this->patchMap[i];
		p->percussion = false;
		this->patches->setPatch(i, p);
	}
	for (int i = 0; i < percCount; i++) {
		MIDIPatchPtr p(new MIDIPatch());
		p->midiPatch = this->percMap[i];
		p->percussion = true;
		this->patches->setPatch(instCount + i, p);
	}
	return this->patches;
}

EventPtr MusicReader_GenericMIDI::readNextEvent()
	throw (stream::error)
{
	EventPtr gev;

	if (!this->setTempo) {
		TempoEvent *ev = new TempoEvent();
		gev.reset(ev);
		ev->channel = 0; // global event (all channels)
		ev->absTime = 0;
		ev->usPerTick = this->usPerQuarterNote / this->ticksPerQuarterNote;
		this->setTempo = true;
		return gev;
	}

	do {
		uint32_t delay = this->readMIDINumber();
		this->tick += delay;


		uint8_t event, evdata;
		this->midi >> u8(event);
		if (event & 0x80) {
			// If the high bit is set it's a normal event
			if ((event & 0xF0) != 0xF0) {
				// 0xF0 events do not change the running status
				this->lastEvent = event;
			}
			this->midi >> u8(evdata);
		} else {
			// The high bit is unset, so this is actually the first data
			// byte for a new event, of the same type as the last event.
			evdata = event;
			event = this->lastEvent;
		}

		// Handle short aftertouch events if the flag was given
		if (this->midiFlags & MIDIFlags::ShortAftertouch) {
			// Change key aftertouch to channel aftertouch
			if ((event & 0xF0) == 0xA0) event = 0xD0 | (event & 0x0F);
		}

		uint8_t midiChannel = event & 0x0F;
		switch (event & 0xF0) {
			case 0x80: { // Note off (two data bytes)
				assert(evdata < MIDI_NOTES);

				uint8_t velocity;
				this->midi >> u8(velocity);

				// Only generate a note-off event if the note was actually on
				if (this->activeNotes[midiChannel][evdata]) {
					NoteOffEvent *ev = new NoteOffEvent();
					gev.reset(ev);
					ev->channel = midiChannel + 1;

					// Record this note as inactive on the channel
					this->activeNotes[midiChannel][evdata] = false;
				}
				break;
			}
			case 0x90: { // Note on (two data bytes)
				assert(evdata < MIDI_NOTES);

				uint8_t velocity;
				this->midi >> u8(velocity);

				if (velocity == 0) { // note off
					// Only generate a note-off event if the note was actually on
					if (this->activeNotes[midiChannel][evdata]) {
						NoteOffEvent *ev = new NoteOffEvent();
						gev.reset(ev);
						ev->channel = midiChannel + 1;
						/// @todo Handle multiple notes on the same channel

						// Record this note as inactive on the channel
						this->activeNotes[midiChannel][evdata] = false;
					}
				} else {
					bool abort = false;
					for (int n = 0; n < MIDI_NOTES; n++) {
						if (this->activeNotes[midiChannel][n]) {
							std::cout << "Polyphonic MIDI channels not yet supported (only "
								"one note at a time on each channel)" << std::endl;
							abort = true;
							break;
						}
					}
					if (abort) break;

					NoteOnEvent *ev = new NoteOnEvent();
					gev.reset(ev);
					ev->channel = midiChannel + 1;
					ev->velocity = velocity;
					if ((this->midiFlags & MIDIFlags::Channel10Perc) && (midiChannel == 10)) {
						ev->milliHertz = PERC_FREQ;

						bool found = false;
						for (int i = 0; i < MIDI_NOTES; i++) {
							if (this->percMap[i] == evdata) {
								ev->instrument = i;
								found = true;
								break;
							}
						}
						// Make sure the instrument has already been mapped by getPatchBank()
						assert(found);

					} else {
						ev->milliHertz = midiToFreq(evdata);
						ev->instrument = this->currentInstrument[midiChannel];
					}
					// Make sure the instrument has already been mapped by getPatchBank()
					assert(ev->instrument != 0xFF);

					/// @todo Handle multiple notes on the same channel

					// Record this note as active on the channel
					this->activeNotes[midiChannel][evdata] = true;
				}
				break;
			}
			case 0xA0: { // Polyphonic key pressure (two data bytes)
				uint8_t pressure;
				// note in is evdata
				this->midi >> u8(pressure);
				std::cout << "Key pressure not yet implemented!" << std::endl;
				break;
			}
			case 0xB0: { // Controller (two data bytes)
				uint8_t value;
				// controller index is in evdata
				this->midi >> u8(value);
				switch (evdata) {
					case 0x67: {
						ConfigurationEvent *ev = new ConfigurationEvent();
						gev.reset(ev);
						ev->channel = 0; // this event is global
						ev->configType = ConfigurationEvent::EnableRhythm;
						ev->value = value;
						break;
					}
					default:
						std::cout << "Ignoring unknown MIDI controller 0x" << std::hex
							<< (int)evdata << std::dec << std::endl;
						break;
				}
				break;
			}
			case 0xC0: { // Instrument change (one data byte)
				bool found = false;
				for (int i = 0; i < MIDI_PATCHES; i++) {
					if (this->patchMap[i] == evdata) {
						this->currentInstrument[midiChannel] = i;
						found = true;
						break;
					}
				}
				// Make sure the instrument has already been mapped by getPatchBank()
				assert(found);
				break;
			}
			case 0xD0: { // Channel pressure (one data byte)
				// pressure is in evdata
				std::cout << "Channel pressure not yet implemented!" << std::endl;
				break;
			}
			case 0xE0: { // Pitch bend (two data bytes)
				uint8_t msb;
				this->midi >> u8(msb);
				// Only lower seven bits are used in each byte
				// 8192 is middle, 0 is -2 semitones, 16384 is +2 semitones
				int16_t value = -8192 + ((msb & 0x7F) << 7) | (evdata & 0x7F);

				if (value != this->currentPitchbend[midiChannel]) {
					// If there are any active notes on this channel, generate pitchbend
					// events for them.
					for (int i = 0; i < MIDI_NOTES; i++) {
						if (this->activeNotes[midiChannel][i]) {
							PitchbendEvent *ev = new PitchbendEvent();
							gev.reset(ev);
							ev->channel = midiChannel + 1;
							ev->milliHertz = midiToFreq(i + (double)value / 4096.0);

							/// @todo Handle multiple notes on this channel
							/// @todo Handle MIDI events to set pitchbend range
							break;
						}
					}
					this->currentPitchbend[midiChannel] = value;
				}
				break;
			}
			case 0xF0: // System message (arbitrary data bytes)
				switch (event) {
					case 0xF0: { // Sysex
						while ((evdata & 0x80) == 0) this->midi >> u8(evdata);
						// This will have read in the terminating EOX (0xF7) message too
						break;
					}
					case 0xF1: // MIDI Time Code Quarter Frame
						this->midi->seekg(1, stream::cur); // message data (ignored)
						break;
					case 0xF2: // Song position pointer
						this->midi->seekg(2, stream::cur); // message data (ignored)
						break;
					case 0xF3: // Song select
						this->midi->seekg(1, stream::cur); // message data (ignored)
						std::cout << "Warning: MIDI Song Select is not implemented." << std::endl;
						break;
					case 0xF6: // Tune request
						break;
					case 0xF7: // End of System Exclusive (EOX) - should never be read, should be absorbed by Sysex handling code
						break;

					// These messages are "real time", meaning they can be sent between the bytes of other messages - but we're
					// lazy and don't handle these here (hopefully they're not necessary in a MIDI file, and even less likely to
					// occur in a CMF.)
					case 0xF8: // Timing clock (sent 24 times per quarter note, only when playing)
					case 0xFA: // Start
					case 0xFB: // Continue
					case 0xFE: // Active sensing (sent every 300ms or MIDI connection assumed lost)
						break;
					case 0xFC: // Stop
						//std::cout << "Received Real Time Stop message (0xFC)" << std::endl;
						return EventPtr();
					case 0xFF: { // System reset, used as meta-events in a MIDI file
						uint32_t len;
						try {
							len = this->readMIDINumber();
						} catch (...) {
							std::cerr << "Warning: Input data truncated within MIDI "
								"meta-event" << std::endl;
							return EventPtr();
						}

						switch (evdata) {
							case 0x2F: // end of track
								return EventPtr();
							case 0x51: { // set tempo
								if (len != 3) {
									std::cerr << "Set tempo event had invalid length!" << std::endl;
									this->midi->seekg(len, stream::cur); // message data (ignored)
									break;
								}
								this->usPerQuarterNote = 0;
								uint8_t n;
								this->midi >> u8(n); this->usPerQuarterNote  = n << 16;
								this->midi >> u8(n); this->usPerQuarterNote |= n << 8;
								this->midi >> u8(n); this->usPerQuarterNote |= n;
								TempoEvent *ev = new TempoEvent();
								gev.reset(ev);
								ev->channel = 0; // global event (all channels)
								ev->absTime = 0;
								ev->usPerTick = this->usPerQuarterNote / this->ticksPerQuarterNote;
								break;
							}
							default:
								std::cout << "Unknown MIDI meta-event 0x" << std::hex
									<< (int)evdata << std::dec << std::endl;
								this->midi->seekg(len, stream::cur); // message data (ignored)
								break;
						}
						break;
					}
					default:
						std::cout << "Unknown MIDI system command 0x" << std::hex
							<< (int)event << std::dec << std::endl;
						break;
				}
				break;
			default:
				std::cout << "Unknown MIDI command 0x" << std::hex << (int)event
					<< std::dec << std::endl;
				break;
		}
	} while (!gev);

	gev->absTime = this->tick;

	return gev;
}

void MusicReader_GenericMIDI::setMIDIStream(stream::input_sptr data)
	throw (stream::error)
{
	this->midi = data;
	return;
}

void MusicReader_GenericMIDI::setTicksPerQuarterNote(unsigned long ticks)
	throw ()
{
	// Make sure readNextEvent() hasn't been called yet.
	assert(!this->setTempo);

	this->ticksPerQuarterNote = ticks;
	return;
}

void MusicReader_GenericMIDI::setusPerQuarterNote(unsigned long us)
	throw ()
{
	// Make sure readNextEvent() hasn't been called yet.
	assert(!this->setTempo);

	this->usPerQuarterNote = us;
	return;
}

uint32_t MusicReader_GenericMIDI::readMIDINumber()
	throw (stream::error)
{
	// Make sure this->setMIDIStream() has been called
	assert(this->midi);

	uint32_t val = 0;
	for (int i = 0; i < 4; i++) {
		uint8_t n;
		this->midi >> u8(n);
		val <<= 7;
		val |= (n & 0x7F); // ignore the MSB
		if (!(n & 0x80)) break; // last byte has the MSB unset
	}
	return val;
}


MusicWriter_GenericMIDI::MusicWriter_GenericMIDI(MIDIFlags::Type midiFlags)
	throw () :
		lastTick(0),
		midiFlags(midiFlags),
		lastCommand(0xFF),
		ticksPerQuarterNote(192), // suitable default
		usPerQuarterNote(500000), // MIDI default
		deepTremolo(false),
		deepVibrato(false),
		updateDeep(false)
{
	memset(this->currentPatch, 0xFF, sizeof(this->currentPatch));
	memset(this->currentInternalPatch, 0xFF, sizeof(this->currentInternalPatch));
	memset(this->currentPitchbend, 0, sizeof(this->currentPitchbend));
	memset(this->activeNote, 0xFF, sizeof(this->activeNote));
	memset(this->lastEvent, 0xFF, sizeof(this->lastEvent));
	memset(this->channelMap, 0xFF, sizeof(this->channelMap));
}

MusicWriter_GenericMIDI::~MusicWriter_GenericMIDI()
	throw ()
{
}

void MusicWriter_GenericMIDI::setPatchBank(const PatchBankPtr& instruments)
	throw (EBadPatchType)
{
	this->patches = boost::dynamic_pointer_cast<MIDIPatchBank>(instruments);
	if (!this->patches) {
		// Patch bank isn't a MIDI one, see if it can be converted into a
		// MIDIPatchBank.  May throw EBadPatchType.
		this->patches = MIDIPatchBankPtr(new MIDIPatchBank(*instruments));
	}
	return;
}

void MusicWriter_GenericMIDI::finish()
	throw (stream::error)
{
	// Write delay then EOF meta-event
	this->midi->write("\x00\xFF\x2F\x00", 4);
	return;
}

void MusicWriter_GenericMIDI::handleEvent(TempoEvent *ev)
	throw (stream::error)
{
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
	return;
}

void MusicWriter_GenericMIDI::handleEvent(NoteOnEvent *ev)
	throw (stream::error, EChannelMismatch)
{
	// Make sure a patch bank has been set
	assert(this->patches);

	/// @todo If instrument is rhythm use channel 10, with override func so CMF
	/// can pick other channels based on value
	uint32_t delay = ev->absTime - this->lastTick;
	this->writeMIDINumber(delay);

	if (this->updateDeep) {
		// Need to set CMF controller 0x63
		uint8_t val = (this->deepTremolo ? 1 : 0) | (this->deepVibrato ? 1 : 0);
		this->writeCommand(0xB0);
		this->midi
			<< u8(0x63)
			<< u8(val)
			<< u8(0) // delay until next event
		;
		this->updateDeep = false;
	}

	MIDIPatchPtr patch = this->patches->getTypedPatch(ev->instrument);
	uint8_t midiChannel, note;
	if (patch->percussion) {
		midiChannel = 9;
		note = patch->midiPatch;
		this->channelMap[ev->channel] = 9;
	} else {
		midiChannel = this->getMIDIchannel(ev->channel, MIDI_CHANNELS);
		int16_t bend;
		freqToMIDI(ev->milliHertz, &note, &bend, 0xFF);
		if (!(this->flags & IntegerNotesOnly)) {
			if (bend != this->currentPitchbend[midiChannel]) {
				bend += 8192;
				uint8_t msb = (bend >> 7) & 0x7F;
				uint8_t lsb = bend & 0x7F;
				this->writeCommand(0xE0 | midiChannel);
				this->midi
					<< u8(msb)
					<< u8(lsb)
					<< u8(0) // delay until next event
				;
				this->currentPitchbend[midiChannel] = bend;
			}
		}
		if (ev->instrument != this->currentPatch[midiChannel]) {
			assert(ev->instrument < MIDI_PATCHES);
			// Instrument has changed
			this->writeCommand(0xC0 | midiChannel);
			this->midi
				<< u8(ev->instrument)
				<< u8(0) // delay until next event
			;
			this->currentPatch[midiChannel] = ev->instrument;
		}
	}

	// Use 64 as the default velocity, otherwise squish it into a 7-bit value.
	uint8_t velocity = (ev->velocity == 0) ? 64 : (ev->velocity >> 1);

	this->writeCommand(0x90 | midiChannel);
	this->midi
		<< u8(note)
		<< u8(velocity)
	;

	this->activeNote[ev->channel] = note;

	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void MusicWriter_GenericMIDI::handleEvent(NoteOffEvent *ev)
	throw (stream::error)
{
	// No need to use the mapping logic here as a channel must always be mapped,
	// otherwise we will be processing a note-off with no associated note-on.
	uint8_t midiChannel = this->channelMap[ev->channel];
	assert(midiChannel != 0xFF);
	//this->getMIDIchannel(ev->channel, MIDI_CHANNELS);

	if (this->activeNote[ev->channel] != 0xFF) {
		uint32_t delay = ev->absTime - this->lastTick;
		this->writeMIDINumber(delay);
		if (this->lastCommand == (0x90 | midiChannel)) {
			// Since the last event was a note-on, it will be more efficient to
			// write the note-off as a zero-velocity note-on, as we won't have to
			// write the actual 'note on' byte thanks to MIDI running-status.
			this->midi
				<< u8(this->activeNote[ev->channel])
				<< u8(0) // velocity
			;
		} else {
			this->writeCommand(0x80 | midiChannel);
			this->midi
				<< u8(this->activeNote[ev->channel])
				<< u8(64) // velocity
			;
		}
		this->activeNote[ev->channel] = 0xFF;
	} else {
		std::cerr << "Warning: Got note-off event for channel #" << ev->channel
			<< " but there was no note playing!" << std::endl;
	}

	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void MusicWriter_GenericMIDI::handleEvent(PitchbendEvent *ev)
	throw (stream::error)
{
	if (this->flags & IntegerNotesOnly) return;

	uint8_t midiChannel = this->getMIDIchannel(ev->channel, MIDI_CHANNELS);

	uint8_t note;
	int16_t bend;
	freqToMIDI(ev->milliHertz, &note, &bend, this->activeNote[ev->channel]);
	/// @todo Handle pitch bending further than one note or if 'note' comes out
	/// differently to the currently playing note
	if (bend != this->currentPitchbend[midiChannel]) {
		uint32_t delay = ev->absTime - this->lastTick;
		this->writeMIDINumber(delay);
		bend += 8192;
		uint8_t msb = (bend >> 7) & 0x7F;
		uint8_t lsb = bend & 0x7F;
		this->writeCommand(0xE0 | midiChannel);
		this->midi
			<< u8(lsb)
			<< u8(msb)
		;
		this->currentPitchbend[midiChannel] = bend;
	}
	this->lastTick = ev->absTime;
	this->lastEvent[midiChannel] = ev->absTime;
	return;
}

void MusicWriter_GenericMIDI::handleEvent(ConfigurationEvent *ev)
	throw (stream::error)
{
	uint32_t delay = ev->absTime - this->lastTick;

	switch (ev->configType) {
		case ConfigurationEvent::EnableRhythm:
			this->writeMIDINumber(delay);
			this->writeCommand(0xB0);
			this->midi
				<< u8(0x67) // Controller 0x67 (CMF rhythm)
				<< u8(ev->value)
			;
			break;
		case ConfigurationEvent::EnableDeepTremolo:
			this->deepTremolo = ev->value;
			this->updateDeep = true;
			break;
		case ConfigurationEvent::EnableDeepVibrato:
			this->deepVibrato = ev->value;
			this->updateDeep = true;
			break;
		default:
			break;
	}
	this->lastTick = ev->absTime;
	return;
}

void MusicWriter_GenericMIDI::setMIDIStream(stream::output_sptr data)
	throw (stream::error)
{
	this->midi = data;
	return;
}

unsigned long MusicWriter_GenericMIDI::getTicksPerQuarterNote()
	throw ()
{
	return this->ticksPerQuarterNote;
}

unsigned long MusicWriter_GenericMIDI::getusPerQuarterNote()
	throw ()
{
	return this->usPerQuarterNote;
}

void MusicWriter_GenericMIDI::writeMIDINumber(uint32_t value)
	throw (stream::error)
{
	// Make sure this->setMIDIStream() has been called
	assert(this->midi);

	// Make sure the value will fit in MIDI's 28-bit limit.
	assert((value >> 28) == 0);

	// Write the three most-significant bytes as 7-bit bytes, with the most
	// significant bit set.
	uint8_t next;
	if (value & (0x7F << 28)) { next = 0x80 | (value >> 28); this->midi << u8(next); }
	if (value & (0x7F << 14)) { next = 0x80 | (value >> 14); this->midi << u8(next); }
	if (value & (0x7F <<  7)) { next = 0x80 | (value >>  7); this->midi << u8(next); }

	// Write the least significant 7-bits last, with the high bit unset to
	// indicate the end of the variable-length stream.
	next = (value & 0x7F);
	this->midi << u8(next);

	return;
}

void MusicWriter_GenericMIDI::writeCommand(uint8_t cmd)
	throw (stream::error)
{
	assert(cmd < 0xF0); // these commands are not part of the running status
	if (this->lastCommand == cmd) return;
	this->midi << u8(cmd);
	this->lastCommand = cmd;
	return;
}

uint8_t MusicWriter_GenericMIDI::getMIDIchannel(int channel, int numMIDIchans)
	throw ()
{
	assert(channel < MAX_CHANNELS);
	assert(numMIDIchans <= MIDI_CHANNELS);

	if (this->channelMap[channel] == 0xFF) {
		// This is the first event on this channel, try to find a spare space
		bool available[MIDI_CHANNELS];
		for (int m = 0; m < numMIDIchans; m++) available[m] = true;
		for (int o = 0; o < MAX_CHANNELS; o++) {
			if (this->channelMap[o] != 0xFF) {
				available[this->channelMap[o]] = false;
			}
		}
		bool mapped = false;
		unsigned long earliestTime = this->lastEvent[0];
		uint8_t earliestTimeChannel = 0;
		for (int m = 0; m < numMIDIchans; m++) {
			if (m == 9) m++; // skip percussion channel
			if (available[m]) {
				this->channelMap[channel] = m;
				mapped = true;
				break;
			}
			// While we're here, find the channel with the longest time since the
			// last event, in case all channels are in use.
			if (this->lastEvent[m] < earliestTime) {
				earliestTime = this->lastEvent[m];
				earliestTimeChannel = m;
			}
		}
		if (!mapped) {
			// All MIDI channels are in use!  Use the one that has gone the longest
			// with no instruments playing.
			this->channelMap[channel] = earliestTimeChannel;
		}
	}
	return this->channelMap[channel];
}
