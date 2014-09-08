/**
 * @file   track-split.cpp
 * @brief  Split tracks with multiple notes into separate tracks.
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

#include <sstream>
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

void SpecificNoteOffEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

std::string PolyphonicEffectEvent::getContent() const
{
	std::ostringstream ss;
	ss
		<< this->Event::getContent()
		<< "event=effect-polyphonic;type=" << this->type << ";data=" << this->data
	;
	return ss.str();
}

void PolyphonicEffectEvent::processEvent(unsigned long delay, unsigned int trackIndex,
	unsigned int patternIndex, EventHandler *handler)
{
	handler->handleEvent(delay, trackIndex, patternIndex, this);
	return;
}

void camoto::gamemusic::splitPolyphonicTracks(MusicPtr& music)
{
	unsigned int patternIndex = 0;
	// For each pattern
	for (std::vector<PatternPtr>::iterator
		pp = music->patterns.begin(); pp != music->patterns.end(); pp++, patternIndex++
	) {
		PatternPtr& pattern = *pp;

		// For each track
		unsigned long trackIndex = 0;
		for (Pattern::iterator
			pt = pattern->begin(); pt != pattern->end(); trackIndex++
		) {

			TrackPtr main(new Track);
			TrackPtr overflow(new Track);
			bool movedNotes = false; // did we move any notes to the overflow track?
			unsigned long curNoteFreq = 0;
			unsigned long delayMain = 0, delayOverflow = 0;
			for (Track::iterator
				ev = (*pt)->begin(); ev != (*pt)->end(); ev++
			) {
				TrackEvent& te = *ev;
				delayMain += te.delay;
				delayOverflow += te.delay;

				NoteOnEvent *evNoteOn = dynamic_cast<NoteOnEvent *>(te.event.get());
				if (evNoteOn) {
					if (curNoteFreq) {
						// Note is currently playing, move this event to a new channel
						te.delay = delayOverflow;
						delayOverflow = 0;
						overflow->push_back(te);
						movedNotes = true;
					} else {
						// No note is playing, record this one
						curNoteFreq = evNoteOn->milliHertz;
						te.delay = delayMain;
						delayMain = 0;
						main->push_back(te);
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
						main->push_back(te);
						curNoteFreq = 0;
					} else {
						// Might be a note off for one of the overflow notes, move it
						te.delay = delayOverflow;
						delayOverflow = 0;
						overflow->push_back(te);
						// movedNotes is only set for note-on events
					}
					continue;
				}

				// Any other event is just left on the main channel
				te.delay = delayMain;
				delayMain = 0;
				main->push_back(te);
			}

			// Remove the track we have just processed, and replace it with the main
			// one we've just generated.
			std::swap(*pt, main);

			if (movedNotes) {
				// Notes were moved to the overflow track, so we need to process that
				// too.  Insert the overflow track after the current one so that it
				// will be processed in the next loop iteration.
				pt = pattern->insert(pt + 1, overflow);

				// Duplicate the TrackInfo structure too
				music->trackInfo.insert(music->trackInfo.begin() + trackIndex + 1,
					music->trackInfo[trackIndex]);
			} else {
				// No inserted track, check the next one
				pt++;
			}

		} // for (each track in the pattern)
	} // for (each pattern)

	// Remember to duplicate pitchbend events on a track if needed by notes
	// Remove any unused tracks
	return;
}
