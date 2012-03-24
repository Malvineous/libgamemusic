/**
 * @file   encode-midi.hpp
 * @brief  Function to convert a Music instance into raw MIDI data.
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

#ifndef _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_

#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/stream.hpp>

namespace camoto {
namespace gamemusic {

/// Convert the song's events into SMF (standard MIDI format) data.
/**
 * @param output
 *   The output stream.  MIDI data is written at the current seek pointer, which
 *   need not be at the beginning of the file.
 *
 * @param midiFlags
 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
 *   data is unusual in some way.
 *
 * @param music
 *   Song to write out as MIDI data.  If the song has a MIDI patchback it is
 *   examined to ensure MIDI percusson is written out on channel 10, and any
 *   instrument change events use the MIDI patch number obtained from the
 *   patchbank.  If there is no patchbank or it is not MIDI, the instrument
 *   numbers used for patch changes are unaltered (i.e. those in the source
 *   event.)
 *
 * @param ticksPerQuarterNote
 *   Number of MIDI ticks per quarter-note.  This controls how many notes appear
 *   in each notation bar, among other things.  It has no effect on playback
 *   speed.
 *
 * @param usPerQuarterNote
 *   On return, the initial number of microseconds per quarter-note.  This could
 *   have been overridden during the song, but this is the initial value.
 */
void midiEncode(stream::output_sptr& output, unsigned int midiFlags,
	MusicPtr music, unsigned long ticksPerQuarterNote,
	unsigned long *usPerQuarterNote)
	throw (stream::error, format_limitation);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_
