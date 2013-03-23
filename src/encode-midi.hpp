/**
 * @file   encode-midi.hpp
 * @brief  Function to convert a Music instance into raw MIDI data.
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
 * @param usPerTick
 *   On return, the initial number of microseconds per tick.  This could
 *   have been changed during the song, but this is the initial value.
 *
 * @param channelsUsed
 *   Pointer to an array of MIDI_CHANNELS entries of bool.  On return, this each
 *   entry is set to true where that MIDI channel was used.  Set to NULL if this
 *   information is not required.
 */
void midiEncode(stream::output_sptr& output, unsigned int midiFlags,
	MusicPtr music,	tempo_t *usPerTick, bool *channelsUsed);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_
