/**
 * @file  camoto/gamemusic/util-midi.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_UTIL_MIDI_HPP_
#define _CAMOTO_GAMEMUSIC_UTIL_MIDI_HPP_

#include <cstdint>
#include <camoto/config.hpp>
#include <camoto/enum-ops.hpp>
#include <camoto/stream.hpp>
#include <camoto/iostream_helpers.hpp>

namespace camoto {
namespace gamemusic {

enum class u28_midi_flags {
	StandardMIDI,  ///< Standard MIDI numbers
	AdLibMUS       ///< AdLib .mus format variable-width numbers
};

IMPLEMENT_ENUM_OPERATORS(u28_midi_flags);

/// @sa u28_midi
struct CAMOTO_GAMEMUSIC_API u28_midi_read {
	u28_midi_read(uint32_t& r, u28_midi_flags flags);
	void read(stream::input& s) const;

	private:
		uint32_t& r;
		u28_midi_flags flags;
};

/// @sa u28_midi
struct CAMOTO_GAMEMUSIC_API u28_midi_write {
	u28_midi_write(const uint32_t& r, u28_midi_flags flags);
	void write(stream::output& s) const;

	private:
		const uint32_t& r;
		u28_midi_flags flags;
};

/// @sa u28_midi
struct CAMOTO_GAMEMUSIC_API u28_midi_const: public u28_midi_write {
	u28_midi_const(const uint32_t& r, u28_midi_flags flags);
};

struct CAMOTO_GAMEMUSIC_API u28_midi: public u28_midi_read, public u28_midi_write {
	u28_midi(uint32_t& r, u28_midi_flags flags);
};

// If you get an error related to the next line (e.g. no match for operator >>)
// it's because you're trying to read a value into a const variable.
inline stream::input& operator >> (stream::input& s, const u28_midi_read& n) {
	n.read(s);
	return s;
}

inline stream::output& operator << (stream::output& s, const u28_midi_write& n) {
	n.write(s);
	return s;
}

/**
 * u28midi will read a variable length MIDI-encoded integer from a stream, e.g.
 *
 * @code
 * uint32_t n;
 * file >> u28midi(n);
 * @endcode
 *
 * Here between one and four bytes (inclusive) will be read from the file.  If
 * no 'final byte' is marked (suggesting the value is longer than four bytes)
 * still only a maximum of four bytes will be read.
 *
 * It can also be used when writing to a stream, e.g.
 *
 * @code
 * uint32_t n = 123456;
 * file << u28midi(n);  // write 3 bytes, 0x87 0xC4 0x40
 * @endcode
 *
 * In this case up to four bytes will be written, and no fewer than one byte.
 * If the value is larger than 28-bits, an exception will be thrown.
 */
inline u28_midi u28midi(uint32_t& r)
{
	return u28_midi(r, u28_midi_flags::StandardMIDI);
}

inline u28_midi_const u28midi(const uint32_t& r)
{
	return u28_midi_const(r, u28_midi_flags::StandardMIDI);
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_UTIL_MIDI_HPP_
