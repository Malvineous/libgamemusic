/**
 * @file  track-split.cpp
 * @brief Split tracks with multiple notes into separate tracks.
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

#include <sstream>
#include <camoto/gamemusic/eventconverter-midi.hpp> // freqToMIDI etc.
#include "track-split.hpp"

using namespace camoto::gamemusic;

std::string SpecificNoteOffEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=note-off-specific;freq=" << this->milliHertz
	;
	return ss.str();
}

void SpecificNoteOffEvent::processEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, EventHandler *handler) const
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string SpecificNoteEffectEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=effect-specific;freq=" << this->milliHertz
	;
	return ss.str();
}

void SpecificNoteEffectEvent::processEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, EventHandler *handler) const
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string PolyphonicEffectEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=effect-polyphonic;type=" << (int)this->type
		<< ";data=" << this->data
	;
	return ss.str();
}

void PolyphonicEffectEvent::processEvent(unsigned long delay,
	unsigned int trackIndex, unsigned int patternIndex, EventHandler *handler) const
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

void camoto::gamemusic::splitPolyphonicTracks(Music& music)
{
	unsigned int patternIndex = 0;
	// For each pattern
	for (auto& pattern : music.patterns) {

		if (patternIndex > 0) {
			throw format_limitation("splitPolyphonicTracks() can't yet work with multipattern data");
			// TODO: If this ever comes up, the way trackInfo is handled will need to
			// be changed so it doesn't get duplicated for every single pattern,
			// ending up with dozens more instances than we are supposed to have.
			// Maybe instead, run through each track one by one, across all patterns?
		}

		// For each track
		unsigned long trackIndex = 0;
		for (auto pt = pattern.begin(); pt != pattern.end(); trackIndex++) {
			assert(pattern.size() == music.trackInfo.size());

			Track main;
			Track overflow;
			bool movedNotes = false; // did we move any notes to the overflow track?
			unsigned long curNoteFreq = 0;
			unsigned long delayMain = 0, delayOverflow = 0;
			double curBend = 0; // current pitchbend in semitones
			for (auto& te : *pt) {
				delayMain += te.delay;
				delayOverflow += te.delay;

				NoteOnEvent *evNoteOn = dynamic_cast<NoteOnEvent *>(te.event.get());
				if (evNoteOn) {
					if (curNoteFreq) {
						// Note is currently playing, move this event to a new channel
						te.delay = delayOverflow;
						delayOverflow = 0;
						overflow.push_back(te);
						movedNotes = true;
					} else {
						// No note is playing, record this one
						curNoteFreq = evNoteOn->milliHertz;
						te.delay = delayMain;
						delayMain = 0;

						if (curBend != 0) {
							// Convert the milliHertz value for this note back to a semitone
							// number, add the pitchbend (in semitones), then convert back
							// to a frequency.
							double targetNote = curBend + freqToMIDI(evNoteOn->milliHertz);
							evNoteOn->milliHertz = midiToFreq(targetNote);
						}

						main.push_back(te);
					}
					continue;
				}

				SpecificNoteOffEvent *evSpecNoteOff =
					dynamic_cast<SpecificNoteOffEvent *>(te.event.get());
				if (evSpecNoteOff) {
					if (evSpecNoteOff->milliHertz == curNoteFreq) {
						// This is a note-off for the current note, replace the specific
						// note-off event with a normal track-wide note-off.
						te.delay = delayMain;
						delayMain = 0;
						NoteOffEvent *evNoteOff = new NoteOffEvent();
						te.event.reset(evNoteOff);
						main.push_back(te);
						curNoteFreq = 0;
					} else {
						// Might be a note off for one of the overflow notes, move it
						te.delay = delayOverflow;
						delayOverflow = 0;
						overflow.push_back(te);
						// movedNotes is only set for note-on events
					}
					continue;
				}

				SpecificNoteEffectEvent *evSpecNoteEffect =
					dynamic_cast<SpecificNoteEffectEvent *>(te.event.get());
				if (evSpecNoteEffect) {
					if (evSpecNoteEffect->milliHertz == curNoteFreq) {
						// This is an effect for the current note, replace the specific
						// effect event with a normal track-wide effect.
						te.delay = delayMain;
						delayMain = 0;
						EffectEvent *evEffect = new EffectEvent();
						*evEffect = *evSpecNoteEffect;
						te.event.reset(evEffect);
						main.push_back(te);
						curNoteFreq = 0;
					} else {
						// Might be an effect for one of the overflow notes, move it
						te.delay = delayOverflow;
						delayOverflow = 0;
						overflow.push_back(te);
						// movedNotes is only set for note-on events
					}
					continue;
				}

				NoteOffEvent *evNoteOff =
					dynamic_cast<NoteOffEvent *>(te.event.get());
				if (evNoteOff) {
					// This is a channel-wide note off, so take note there is no longer
					// a note playing.
					curNoteFreq = 0;

					// Leave on main channel
					te.delay = delayMain;
					delayMain = 0;
					main.push_back(te);
					continue;
				}

				PolyphonicEffectEvent *evPolyEffect =
					dynamic_cast<PolyphonicEffectEvent *>(te.event.get());
				if (evPolyEffect) {
					switch ((PolyphonicEffectEvent::Type)evPolyEffect->type) {
						case PolyphonicEffectEvent::Type::PitchbendChannel:
							curBend = midiPitchbendToSemitones(evPolyEffect->data);

							// Create a normal pitchbend if there is a note currently playing
							if (curNoteFreq) {
								auto evEffect = std::make_shared<EffectEvent>();
								evEffect->type = EffectEvent::Type::PitchbendNote;
								double targetNote = curBend + freqToMIDI(curNoteFreq);
								evEffect->data = midiToFreq(targetNote);

								TrackEvent teCopy;
								teCopy.delay = delayMain;
								teCopy.event = evEffect;
								delayMain = 0;
								main.push_back(teCopy);
							}
							break;

						case PolyphonicEffectEvent::Type::VolumeChannel:
							// Just convert the event to a normal volume change
							auto evEffect = std::make_shared<EffectEvent>();
							evEffect->type = EffectEvent::Type::Volume;
							evEffect->data = evPolyEffect->data;

							TrackEvent teCopy;
							teCopy.delay = delayMain;
							delayMain = 0;
							teCopy.event = evEffect;
							main.push_back(teCopy);
							break;
					}
					// Move the polyphonic event onto the overflow channel in case there
					// are other notes playing.  (If not, no harm done).
					te.delay = delayOverflow;
					delayOverflow = 0;
					overflow.push_back(te);

					// Don't want these events added to the main channel below
					continue;
				}

				// Any other event is just left on the main channel
				te.delay = delayMain;
				delayMain = 0;
				main.push_back(te);
			} // for (each event on the track)

			// Remove the track we have just processed, and replace it with the main
			// one we've just generated.
			std::swap(*pt, main);

			if (movedNotes) {
				// Notes were moved to the overflow track, so we need to process that
				// too.  Insert the overflow track after the current one so that it
				// will be processed in the next loop iteration.
				pt = pattern.insert(pt + 1, overflow);

				// Duplicate the TrackInfo structure too
				assert(trackIndex < music.trackInfo.size());
				music.trackInfo.insert(music.trackInfo.begin() + trackIndex + 1,
					music.trackInfo[trackIndex]);
			} else {
				// No inserted track, check the next one
				pt++;
			}

		} // for (each track in the pattern)
		patternIndex++;
	} // for (each pattern)

	// Remember to duplicate pitchbend events on a track if needed by notes
	// Remove any unused tracks
	return;
}
