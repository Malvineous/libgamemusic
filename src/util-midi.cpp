/**
 * @file  util-midi.cpp
 * @brief Utility functions related to MIDI.
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

#include <cassert>
#include <camoto/gamemusic/util-midi.hpp>

namespace camoto {
namespace gamemusic {

u28_midi_read::u28_midi_read(uint32_t& r, u28_midi_flags flags)
	:	r(r),
		flags(flags)
{
}

void u28_midi_read::read(stream::input& s) const
{
	this->r = 0;

	if (this->flags & u28_midi_flags::AdLibMUS) {
		for (int i = 0; i < 255; i++) { // 255 is just an arbitrary limit
			uint8_t n;
			s >> u8(n);
			if (n == 0xF8) {
				this->r += 240;
			} else {
				this->r += n;
				break;
			}
		}
	} else {
		for (int i = 0; i < 5; i++) {
			uint8_t n;
			s >> u8(n);
			this->r <<= 7;
			this->r |= (n & 0x7F); // ignore the MSB
			if (!(n & 0x80)) break; // last byte has the MSB unset
		}
	}
	return;
}

u28_midi_write::u28_midi_write(const uint32_t& r, u28_midi_flags flags)
	:	r(r),
		flags(flags)
{
}

void u28_midi_write::write(stream::output& s) const
{
	if ((this->r >> 28) > 0) throw stream::error("MIDI numbers cannot be wider than 28-bit");

	// Write the three most-significant bytes as 7-bit bytes, with the most
	// significant bit set.
	uint8_t next;
	if (this->flags & u28_midi_flags::AdLibMUS) {
#warning TODO: Implement writing of AdLibMUS style variable-length integers
		assert(false);
	} else {
		if (this->r & (0x7F << 21)) { next = 0x80 | (this->r >> 21); s << u8(next); }
		if (this->r & (0x7F << 14)) { next = 0x80 | (this->r >> 14); s << u8(next); }
		if (this->r & (0x7F <<  7)) { next = 0x80 | (this->r >>  7); s << u8(next); }

		// Write the least significant 7-bits last, with the high bit unset to
		// indicate the end of the variable-length stream.
		next = (this->r & 0x7F);
		s << u8(next);
	}
	return;
}

u28_midi_const::u28_midi_const(const uint32_t& r, u28_midi_flags flags)
	:	u28_midi_write(r, flags)
{
}

u28_midi::u28_midi(uint32_t& r, u28_midi_flags flags)
	:	u28_midi_read(r, flags),
		u28_midi_write(r, flags)
{
	// Beware: r is not initialised yet, if we are reading from a file
}

} // namespace gamemusic
} // namespace camoto
