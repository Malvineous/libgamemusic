/**
 * @file   decode-midi.hpp
 * @brief  Format decoder for Standard MIDI Format (SMF) MIDI data.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_DECODE_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_DECODE_MIDI_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/stream.hpp>

namespace camoto {
namespace gamemusic {

/// Convert caller-supplied SMF MIDI data into a Music instance.
/**
 * All MIDI files have at least one track, but some have additional tracks
 * with events to be played concurrently.  Each channel within each track
 * is assigned to a unique libgamemusic channel, i.e. events for MIDI
 * channel 5 on track 1 might be returned as channel 0, and events for
 * the same channel 5 on track 2 could be returned as channel 1.
 *
 * The MIDI data will be read from input until either EOF or an end-of-track
 * event is encountered.  If there is a possibility of non-MIDI data following
 * the MIDI data, a substream should be used to prevent the non-MIDI data from
 * being processed.
 *
 * @param input
 *   Data stream containing the MIDI data.
 *
 * @param flags
 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
 *   data is unusual in some way.
 *
 * @param ticksPerQuarterNote
 *   Number of ticks in a quarter-note.  Default is 192
 *   (MIDI_DEF_TICKS_PER_QUARTER_NOTE).  This controls how many notes appear
 *   in each notation bar, among other things.
 *
 * @param usPerQuarterNote
 *   Number of microseconds in a quarter-note.  Defaults to 500,000.
 */
MusicPtr midiDecode(stream::input_sptr& input, unsigned int flags,
	unsigned long ticksPerQuarterNote, unsigned long usPerQuarterNote);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_DECODE_MIDI_HPP_
