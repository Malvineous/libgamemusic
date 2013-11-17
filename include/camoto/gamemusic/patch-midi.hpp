/**
 * @file   patch-midi.hpp
 * @brief  Derived Patch for MIDI patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCH_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_PATCH_MIDI_HPP_

#include <camoto/gamemusic/patch.hpp>
#include <camoto/stream.hpp>
#include <stdint.h>

namespace camoto {
namespace gamemusic {

/// Descendent of Patch for storing MIDI instrument settings.
struct DLL_EXPORT MIDIPatch: public Patch
{
	/// Default constructor sets everything to zero/defaults.
	MIDIPatch();

	/// 7-bit number for MIDI instrument patch (0-127 inclusive)
	uint8_t midiPatch;

	/// If true, %midiPatch is a note number on MIDI channel 10.
	bool percussion;

};

inline bool operator== (const MIDIPatch& a, const MIDIPatch& b)
{
	return
		(a.midiPatch == b.midiPatch)
		&& (a.percussion == b.percussion)
	;
}

/// Shared pointer to a Patch.
typedef boost::shared_ptr<MIDIPatch> MIDIPatchPtr;

} // namespace gamemusic
} // namespace camoto

/// ostream handler for printing patches as ASCII
std::ostream& operator << (std::ostream& s, const camoto::gamemusic::MIDIPatch& p);

/// ostream handler for printing patches as ASCII
std::ostream& operator << (std::ostream& s, const camoto::gamemusic::MIDIPatchPtr p);

#endif // _CAMOTO_GAMEMUSIC_PATCH_MIDI_HPP_
