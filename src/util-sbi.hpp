/**
 * @file  util-sbi.hpp
 * @brief Utility functions for working with SBI format instrument data.
 *
 * Copyright (C) 2010-2017 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_UTIL_SBI_HPP_
#define _CAMOTO_GAMEMUSIC_UTIL_SBI_HPP_

#include <camoto/stream.hpp>
#include <camoto/gamemusic/patch-opl.hpp>

namespace camoto {
namespace gamemusic {

struct sbi_instrument_read {
	sbi_instrument_read(OPLPatch& r);
	void read(stream::input& s) const;

	private:
		OPLPatch& r;
};

struct sbi_instrument_write {
	sbi_instrument_write(const OPLPatch& r);
	void write(stream::output& s) const;

	private:
		const OPLPatch& r;
};

struct sbi_instrument_const: public sbi_instrument_write {
	sbi_instrument_const(const OPLPatch& r);
};

/**
 * instrumentSBI will read/write 16 bytes to/from a stream, converting between
 * an OPLPatch variable and a stream.
 *
 * @code
 * OPLPatch p;
 * file >> instrumentSBI(p);  // read 16 bytes
 * file << instrumentSBI(p);  // write 16 bytes
 * @endcode
 */
struct sbi_instrument: public sbi_instrument_read, public sbi_instrument_write {
	sbi_instrument(OPLPatch& r);
};

// If you get an error related to the next line (e.g. no match for operator >>)
// it's because you're trying to read a value into a const variable.
inline stream::input& operator >> (stream::input& s, const sbi_instrument_read& n) {
	n.read(s);
	return s;
}

inline stream::output& operator << (stream::output& s, const sbi_instrument_write& n) {
	n.write(s);
	return s;
}

inline sbi_instrument instrumentSBI(OPLPatch& r)
{
	return sbi_instrument(r);
}
inline sbi_instrument_const instrumentSBI(const OPLPatch& r)
{
	return sbi_instrument_const(r);
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_UTIL_SBI_HPP_
