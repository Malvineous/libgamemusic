/**
 * @file   encode-midi.hpp
 * @brief  Function to convert a Music instance into raw MIDI data.
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

#ifndef _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_

#include <boost/function.hpp>
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
 * @param music
 *   Song to write out as MIDI data.
 *
 * @param midiFlags
 *   One or more flags.  Use MIDIFlags::Default unless the MIDI
 *   data is unusual in some way.
 *
 * @param channelsUsed
 *   Pointer to an array of MIDI_CHANNEL_COUNT entries of bool.  On return, this each
 *   entry is set to true where that MIDI channel was used.  Set to NULL if this
 *   information is not required.
 *
 * @param eventOrder
 *   Flag to indicate the order of event processing.
 *   EventHandler::Order_Row_Track will produce a single stream of MIDI events
 *   suitable for a type-0 MIDI file (where cbEndOfTrack could be NULL) while
 *   EventHandler::Order_Track_Row will produce multiple streams of MIDI events
 *   split up by libgamemusic tracks, with cbEndOfTrack called at the end of
 *   each libgamemusic track - more suited to type-1 MIDI files.
 *
 * @param cbEndOfTrack
 *   Callback notified at the end of each track.  May be NULL.
 */
void DLL_EXPORT midiEncode(stream::output_sptr output, ConstMusicPtr music,
	unsigned int midiFlags, bool *channelsUsed,
	EventHandler::EventOrder eventOrder, boost::function<void()> cbEndOfTrack
);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_ENCODE_MIDI_HPP_
