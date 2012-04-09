/**
 * @file   decode-midi.cpp
 * @brief  Format decoder for Standard MIDI Format (SMF) MIDI data.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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

#include <deque>
#include <camoto/stream.hpp>
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include "decode-midi.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

/// Frequency to use for percussion notes.
const unsigned int PERC_FREQ = 440000;

/// Default to a grand piano until a patch change event arrives.
const unsigned int MIDI_DEFAULT_PATCH = 0;

/// MusicReader class that understands MIDI data.
class MIDIDecoder
{
	public:
		/// Constructor
		/**
		 * @param input
		 *   Data stream containing the MIDI data.
		 *
		 * @param midiFlags
		 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
		 *   data is unusual in some way.
		 *
		 * @param ticksPerQuarterNote
		 *   Number of ticks in a quarter-note.  Default is 192
		 *   (MIDI_DEF_TICKS_PER_QUARTER_NOTE).  This controls how many notes appear
		 *   in each notation bar, among other things.
		 *
		 * @param usPerQuarterNote
		 *   Number of microseconds in a quarter-note.  Default is 500,000
		 *   (MIDI_DEF_uS_PER_QUARTER_NOTE).
		 */
		MIDIDecoder(stream::input_sptr& input, unsigned int midiFlags,
			unsigned long ticksPerQuarterNote, unsigned long usPerQuarterNote)
			throw ();

		virtual ~MIDIDecoder()
			throw ();

		/// Read the input data and set events and instruments accordingly.
		/**
		 * This function will call the callback's readNextPair() function repeatedly
		 * until it returns false.  It will populate the events and instruments in
		 * the supplied Music instance.
		 *
		 * @throw stream:error
		 *   If the input data could not be read or converted for some reason.
		 */
		MusicPtr decode()
			throw (stream::error);

	protected:
		stream::input_sptr input;         ///< MIDI data to read
		unsigned long tick;               ///< Time of next event
		uint8_t lastEvent;                ///< Last event (for MIDI running status)
		std::deque<EventPtr> eventBuffer; ///< List of events to return in readNextEvent()
		PatchBankPtr patches;             ///< Cached patches populated by getPatchBank()
		bool setTempo;                    ///< Have we sent the initial tempo event?
		unsigned int midiFlags;           ///< Flags supplied in constructor
		unsigned long ticksPerQuarterNote;///< Current song granularity

		/// For each of the 128 MIDI patches, which instrument are we using?
		//uint8_t patchMap[MIDI_PATCHES];

		/// For each of the percussion notes, which instrument are we using?
		uint8_t percMap[MIDI_NOTES];

		/// Length of a quarter note, in microseconds.
		unsigned long usPerQuarterNote;

		/// Current instrument (not MIDI patch) on each MIDI channel
		uint8_t currentInstrument[MIDI_CHANNELS];

		/// Current pitchbend level on each MIDI channel (-8192 to 8191)
		int16_t currentPitchbend[MIDI_CHANNELS];

		/// List of notes currently being played on each channel
		bool activeNotes[MIDI_CHANNELS][MIDI_NOTES];

		/// Read a variable-length integer from MIDI data.
		/**
		 * Reads up to four bytes from the MIDI stream, returning a 28-bit
		 * integer.
		 */
		uint32_t readMIDINumber()
			throw (stream::error);

		/// Set the MIDI patch in use on the given channel.
		/**
		 * This will add a new entry into the patchbank if the given patch does
		 * not exist.  Otherwise it will reuse an existing entry in the patchbank.
		 *
		 * @param patches
		 *   PatchBank to search within and possibly append to.
		 *
		 * @param midiChannel
		 *   MIDI channel (0-15).
		 *
		 * @param midiPatch
		 *   MIDI instrument number (0-127).
		 */
		void setInstrument(PatchBankPtr& patches, unsigned int midiChannel,
			unsigned int midiPatch)
			throw ();
};

MusicPtr camoto::gamemusic::midiDecode(stream::input_sptr& input,
	unsigned int flags, unsigned long ticksPerQuarterNote,
	unsigned long usPerQuarterNote)
	throw (stream::error)
{
	MIDIDecoder decoder(input, flags, ticksPerQuarterNote, usPerQuarterNote);
	return decoder.decode();
}


MIDIDecoder::MIDIDecoder(stream::input_sptr& input, unsigned int midiFlags,
	unsigned long ticksPerQuarterNote, unsigned long usPerQuarterNote)
	throw ()
	: input(input),
	  tick(0),
	  lastEvent(0),
	  midiFlags(midiFlags),
	  ticksPerQuarterNote(ticksPerQuarterNote),
	  usPerQuarterNote(usPerQuarterNote)
{
	memset(this->percMap, 0xFF, sizeof(this->percMap));
	memset(this->currentInstrument, 0, sizeof(this->currentInstrument));
	memset(this->currentPitchbend, 0, sizeof(this->currentPitchbend));
	memset(this->activeNotes, 0, sizeof(this->activeNotes));
}

MIDIDecoder::~MIDIDecoder()
	throw ()
{
}

MusicPtr MIDIDecoder::decode()
	throw (stream::error)
{
	MusicPtr music(new Music());
	music->patches.reset(new PatchBank());
	music->events.reset(new EventVector());
	music->ticksPerQuarterNote = this->ticksPerQuarterNote;

	EventPtr gev;

	// Add the tempo event first
	TempoEvent *ev = new TempoEvent();
	gev.reset(ev);
	ev->channel = 0; // global event (all channels)
	ev->absTime = 0;
	ev->usPerTick = this->usPerQuarterNote / this->ticksPerQuarterNote;
	music->events->push_back(gev);

	try {
		bool eof = false;
		do {
			uint32_t delay = this->readMIDINumber();
			this->tick += delay;

			uint8_t event, evdata;
			this->input >> u8(event);
			if (event & 0x80) {
				// If the high bit is set it's a normal event
				if ((event & 0xF0) != 0xF0) {
					// 0xF0 events do not change the running status
					this->lastEvent = event;
				}
				this->input >> u8(evdata);
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
					this->input >> u8(velocity);

					// Only generate a note-off event if the note was actually on
					if (this->activeNotes[midiChannel][evdata]) {
						NoteOffEvent *ev = new NoteOffEvent();
						gev.reset(ev);
						ev->channel = midiChannel + 1;

						gev->absTime = this->tick;
						music->events->push_back(gev);

						// Record this note as inactive on the channel
						this->activeNotes[midiChannel][evdata] = false;
					}
					break;
				}
				case 0x90: { // Note on (two data bytes)
					assert(evdata < MIDI_NOTES);

					uint8_t velocity;
					this->input >> u8(velocity);

					if (velocity == 0) { // note off
						// Only generate a note-off event if the note was actually on
						if (this->activeNotes[midiChannel][evdata]) {
							NoteOffEvent *ev = new NoteOffEvent();
							gev.reset(ev);
							ev->channel = midiChannel + 1;
							/// @todo Handle multiple notes on the same channel

							gev->absTime = this->tick;
							music->events->push_back(gev);

							// Record this note as inactive on the channel
							this->activeNotes[midiChannel][evdata] = false;
						}
					} else {
						bool abort = false;
						for (unsigned int n = 0; n < MIDI_NOTES; n++) {
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
						if (((this->midiFlags & MIDIFlags::Channel10NoPerc) == 0) && (midiChannel == 9)) {
							if (this->percMap[evdata] == 0xFF) {
								// Need to allocate a new instrument for this percussion note
								MIDIPatchPtr newPatch(new MIDIPatch());
								newPatch->percussion = true;
								newPatch->midiPatch = evdata;
								unsigned int n = music->patches->getPatchCount();
								music->patches->setPatchCount(n + 1);
								music->patches->setPatch(n, newPatch);
								this->percMap[evdata] = n;
							}
							ev->milliHertz = PERC_FREQ;
							ev->instrument = this->percMap[evdata];
						} else {
							ev->milliHertz = midiToFreq(evdata);
							ev->instrument = this->currentInstrument[midiChannel];
						}
						if (ev->instrument >= music->patches->getPatchCount()) {
							// A note is sounding without a patch change event ever arriving,
							// so use a default instrument
							this->setInstrument(music->patches, midiChannel, MIDI_DEFAULT_PATCH);
						}

						/// @todo Handle multiple notes on the same channel

						gev->absTime = this->tick;
						assert(gev->channel != 0);
						music->events->push_back(gev);

						// Record this note as active on the channel
						this->activeNotes[midiChannel][evdata] = true;
					}
					break;
				}
				case 0xA0: { // Polyphonic key pressure (two data bytes)
					uint8_t pressure;
					// note in is evdata
					this->input >> u8(pressure);
					std::cout << "Key pressure not yet implemented!" << std::endl;
					break;
				}
				case 0xB0: { // Controller (two data bytes)
					uint8_t value;
					// controller index is in evdata
					this->input >> u8(value);
					switch (evdata) {
						case 0x67: {
							ConfigurationEvent *ev = new ConfigurationEvent();
							gev.reset(ev);
							ev->channel = 0; // this event is global
							ev->configType = ConfigurationEvent::EnableRhythm;
							ev->value = value;

							gev->absTime = this->tick;
							music->events->push_back(gev);
							break;
						}
						/// @todo Handle AM/VIB and transpose controllers (and patch change one?)
						default:
							std::cout << "Ignoring unknown MIDI controller 0x" << std::hex
								<< (int)evdata << std::dec << std::endl;
							break;
					}
					break;
				}
				case 0xC0: { // Instrument change (one data byte)
					this->setInstrument(music->patches, midiChannel, evdata);
					break;
				}
				case 0xD0: { // Channel pressure (one data byte)
					// pressure is in evdata
					std::cout << "Channel pressure not yet implemented!" << std::endl;
					break;
				}
				case 0xE0: { // Pitch bend (two data bytes)
					uint8_t msb;
					this->input >> u8(msb);
					// Only lower seven bits are used in each byte
					// 8192 is middle, 0 is -2 semitones, 16384 is +2 semitones
					int16_t value = -8192 + (((msb & 0x7F) << 7) | (evdata & 0x7F));

					if (value != this->currentPitchbend[midiChannel]) {
						// If there are any active notes on this channel, generate pitchbend
						// events for them.
						for (unsigned int i = 0; i < MIDI_NOTES; i++) {
							if (this->activeNotes[midiChannel][i]) {
								PitchbendEvent *ev = new PitchbendEvent();
								gev.reset(ev);
								ev->channel = midiChannel + 1;
								ev->milliHertz = midiToFreq(i + (double)value / 4096.0);

								gev->absTime = this->tick;
								music->events->push_back(gev);

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
							while ((evdata & 0x80) == 0) this->input >> u8(evdata);
							// This will have read in the terminating EOX (0xF7) message too
							break;
						}
						case 0xF1: // MIDI Time Code Quarter Frame
							this->input->seekg(1, stream::cur); // message data (ignored)
							break;
						case 0xF2: // Song position pointer
							this->input->seekg(2, stream::cur); // message data (ignored)
							break;
						case 0xF3: // Song select
							this->input->seekg(1, stream::cur); // message data (ignored)
							std::cout << "Warning: MIDI Song Select is not implemented." << std::endl;
							break;
						case 0xF6: // Tune request
							break;
						case 0xF7: // End of System Exclusive (EOX) - should never be read, should be absorbed by Sysex handling code
							break;

							// These messages are "real time", meaning they can be sent
							// between the bytes of other messages - but we're lazy and don't
							// handle these here (hopefully they're not necessary in a MIDI
							// file, and even less likely to occur in a CMF.)
						case 0xF8: // Timing clock (sent 24 times per quarter note, only when playing)
						case 0xFA: // Start
						case 0xFB: // Continue
						case 0xFE: // Active sensing (sent every 300ms or MIDI connection assumed lost)
							break;
						case 0xFC: // Stop
							eof = true;
							break;
						case 0xFF: { // System reset, used as meta-events in a MIDI file
							uint32_t len = this->readMIDINumber();
							switch (evdata) {
								case 0x2F: // end of track
									eof = true;
									break;
								case 0x51: { // set tempo
									if (len != 3) {
										std::cerr << "Set tempo event had invalid length!" << std::endl;
										this->input->seekg(len, stream::cur); // message data (ignored)
										break;
									}
									this->usPerQuarterNote = 0;
									uint8_t n;
									this->input >> u8(n); this->usPerQuarterNote  = n << 16;
									this->input >> u8(n); this->usPerQuarterNote |= n << 8;
									this->input >> u8(n); this->usPerQuarterNote |= n;
									TempoEvent *ev = new TempoEvent();
									gev.reset(ev);
									ev->channel = 0; // global event (all channels)
									ev->absTime = 0;
									ev->usPerTick = this->usPerQuarterNote / this->ticksPerQuarterNote;

									gev->absTime = this->tick;
									music->events->push_back(gev);
									break;
								}
								default:
									std::cout << "Unknown MIDI meta-event 0x" << std::hex
										<< (int)evdata << std::dec << std::endl;
									this->input->seekg(len, stream::cur); // message data (ignored)
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
		} while (!eof);
	} catch (const stream::incomplete_read&) {
		// reached eof
	}

	return music;
}

uint32_t MIDIDecoder::readMIDINumber()
	throw (stream::error)
{
	// Make sure this->setMIDIStream() has been called
	assert(this->input);

	uint32_t val = 0;
	for (int i = 0; i < 4; i++) {
		uint8_t n;
		this->input >> u8(n);
		val <<= 7;
		val |= (n & 0x7F); // ignore the MSB
		if (!(n & 0x80)) break; // last byte has the MSB unset
	}
	return val;
}

void MIDIDecoder::setInstrument(PatchBankPtr& patches, unsigned int midiChannel,
	unsigned int midiPatch)
	throw ()
{
	bool found = false;
	for (unsigned int i = 0; i < patches->getPatchCount(); i++) {
		MIDIPatchPtr p = boost::dynamic_pointer_cast<MIDIPatch>(patches->getPatch(i));
		if (!p) continue;
		if (p->percussion) continue;
		if (p->midiPatch == midiPatch) {
			this->currentInstrument[midiChannel] = i;
			found = true;
			break;
		}
	}
	if (!found) {
		// Have to allocate a new instrument
		MIDIPatchPtr newPatch(new MIDIPatch());
		newPatch->percussion = false;
		newPatch->midiPatch = midiPatch;
		unsigned int n = patches->getPatchCount();
		patches->setPatchCount(n + 1);
		patches->setPatch(n, newPatch);
		this->currentInstrument[midiChannel] = n;
	}
	return;
}
