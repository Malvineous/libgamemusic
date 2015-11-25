/**
 * @file  patch-adlib.hpp
 * @brief Utility functions related to AdLib OPL patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCH_ADLIB_HPP_
#define _CAMOTO_GAMEMUSIC_PATCH_ADLIB_HPP_

#include <camoto/stream.hpp>
#include <camoto/iostream_helpers.hpp>
#include <camoto/gamemusic/patch-opl.hpp>

namespace camoto {
namespace gamemusic {

template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_op_read
{
	adlib_patch_op_read(OPLOperator& r, uint8_t& feedback, bool& connection)
		:	r(r),
			feedback(feedback),
			connection(connection)
	{
	}

	/// Read 13 elements of type T from the stream and populate an OPL operator.
	void read(stream::input& s) const
	{
#define INS_KSL      host_from<T, little_endian>(inst[0])
#define INS_MULTIPLE host_from<T, little_endian>(inst[1])
#define INS_FEEDBACK host_from<T, little_endian>(inst[2])
#define INS_ATTACK   host_from<T, little_endian>(inst[3])
#define INS_SUSTAIN  host_from<T, little_endian>(inst[4])
#define INS_EG       host_from<T, little_endian>(inst[5])
#define INS_DECAY    host_from<T, little_endian>(inst[6])
#define INS_RELEASE  host_from<T, little_endian>(inst[7])
#define INS_LEVEL    host_from<T, little_endian>(inst[8])
#define INS_AM       host_from<T, little_endian>(inst[9])
#define INS_VIB      host_from<T, little_endian>(inst[10])
#define INS_KSR      host_from<T, little_endian>(inst[11])
#define INS_CON      host_from<T, little_endian>(inst[12])
		T inst[13];
		s.read((uint8_t *)&inst[0], sizeof(inst));
		this->r.enableTremolo = INS_AM ? 1 : 0;
		this->r.enableVibrato = INS_VIB ? 1 : 0;
		this->r.enableSustain = INS_EG ? 1 : 0;
		this->r.enableKSR     = INS_KSR ? 1 : 0;
		this->r.freqMult      = INS_MULTIPLE & 0x0f;
		this->r.scaleLevel    = INS_KSL & 0x03;
		this->r.outputLevel   = INS_LEVEL & 0x3f;
		this->r.attackRate    = INS_ATTACK & 0x0f;
		this->r.decayRate     = INS_DECAY & 0x0f;
		this->r.sustainRate   = INS_SUSTAIN & 0x0f;
		this->r.releaseRate   = INS_RELEASE & 0x0f;
//	this->r.waveSelect    = INS_WAVESEL & 0x07;
		this->feedback = INS_FEEDBACK & 7;
		this->connection = INS_CON ? 0 : 1;
#undef INS_KSL
#undef INS_MULTIPLE
#undef INS_FEEDBACK
#undef INS_ATTACK
#undef INS_SUSTAIN
#undef INS_EG
#undef INS_DECAY
#undef INS_RELEASE
#undef INS_LEVEL
#undef INS_AM
#undef INS_VIB
#undef INS_KSR
#undef INS_CON
		return;
	}

	private:
		OPLOperator& r;
		uint8_t& feedback;
		bool& connection;
};

template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_op_write
{
	adlib_patch_op_write(const OPLOperator& r, uint8_t feedback, bool connection)
		:	r(r),
			feedback(feedback),
			connection(connection)
	{
	}

	/// Write 13 elements of type T to the stream from the OPL operator.
	void write(stream::output& s) const
	{
		T inst[13];
		inst[ 0] = host_to<T, little_endian>(this->r.scaleLevel);            // KSL
		inst[ 1] = host_to<T, little_endian>(this->r.freqMult);              // MULTIPLE
		inst[ 2] = host_to<T, little_endian>(this->feedback);                // FEEDBACK
		inst[ 3] = host_to<T, little_endian>(this->r.attackRate);            // ATTACK
		inst[ 4] = host_to<T, little_endian>(this->r.sustainRate);           // SUSTAIN
		inst[ 5] = host_to<T, little_endian>(this->r.enableSustain ? 1 : 0); // EG
		inst[ 6] = host_to<T, little_endian>(this->r.decayRate);             // DECAY
		inst[ 7] = host_to<T, little_endian>(this->r.releaseRate);           // RELEASE
		inst[ 8] = host_to<T, little_endian>(this->r.outputLevel);           // LEVEL
		inst[ 9] = host_to<T, little_endian>(this->r.enableTremolo ? 1 : 0); // AM
		inst[10] = host_to<T, little_endian>(this->r.enableVibrato ? 1 : 0); // VIB
		inst[11] = host_to<T, little_endian>(this->r.enableKSR ? 1 : 0);     // KSR
		inst[12] = host_to<T, little_endian>(this->connection ? 0 : 1);      // CON

		s.write((uint8_t *)inst, sizeof(inst));
		return;
	}

	private:
		const OPLOperator& r;
		uint8_t feedback;
		bool connection;
};

template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_op_const: public adlib_patch_op_write<T>
{
	adlib_patch_op_const(const OPLOperator& r, uint8_t feedback, bool connection)
		:	adlib_patch_op_write<T>(r, feedback, connection)
	{
	}
};

template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_op:
	public adlib_patch_op_read<T>,
	public adlib_patch_op_write<T>
{
	adlib_patch_op(OPLOperator& r, uint8_t& feedback, bool& connection)
		:	adlib_patch_op_read<T>(r, feedback, connection),
			adlib_patch_op_write<T>(r, feedback, connection)
	{
	}
};

template <typename T>
stream::input& operator >> (stream::input& s, const adlib_patch_op_read<T>& n) {
	n.read(s);
	return s;
}

template <typename T>
stream::output& operator << (stream::output& s, const adlib_patch_op_write<T>& n) {
	n.write(s);
	return s;
}

/**
 * adlibOperator will read a single standard AdLib operator from a stream, e.g.
 *
 * @code
 * OPLPatch n;
 * file >> adlibOperator<uint8_t>(n.m);  // read 13-bytes
 * file >> adlibOperator<uint16_t>(n.c); // read 26-bytes
 * @endcode
 *
 * All Adlib operators are 13 fields long, and usually written as bytes, but
 * some earlier formats write them as 16-bit little-endian integers instead.
 * The template type controls the field width.  At present the 16-bit version is
 * little-endian only.  This may change if a file is ever found that stores
 * the values in big-endian.
 *
 * This method can also be used when writing to a stream, e.g.
 *
 * @code
 * OPLPatch n;
 * file << adlibOperator<uint8_t>(n.m);   // write 13 bytes
 * file << adlibOperator<uint16_t>(n.m);  // write 26 bytes
 * @endcode
 */
template <typename T>
adlib_patch_op<T> adlibOperator(OPLOperator& r, uint8_t& feedback,
	bool& connection)
{
	return adlib_patch_op<T>(r, feedback, connection);
}

template <typename T>
adlib_patch_op_const<T> adlibOperator(const OPLOperator& r, uint8_t feedback,
	bool connection)
{
	return adlib_patch_op_const<T>(r, feedback, connection);
}



/// @sa adlib_patch
template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_read
{
	adlib_patch_read(OPLPatch& r)
		:	r(r)
	{
	}

	/// Read 28 elements of type T from the stream and populate an OPL patch.
	void read(stream::input& s) const
	{
		// The instruments store both a Modulator and a Carrier value for the
		// Feedback and Connection, but the OPL only uses one value for each
		// Modulator+Carrier pair.  Both values often seem to be set the same,
		// however the official docs say to use op0 and ignore the op1 value,
		// so we can just pick the Modulator's value here.
		uint8_t dummyFeedback;
		bool dummyConnection;
		s
			>> adlibOperator<T>(this->r.m, this->r.feedback, this->r.connection)
			>> adlibOperator<T>(this->r.c, dummyFeedback, dummyConnection)
		;
		this->readWaveSel(s, &this->r.m);
		this->readWaveSel(s, &this->r.c);
		return;
	}

	/// Read one element of type T from the stream and populate an OPL operator.
	void readWaveSel(stream::input& s, OPLOperator *o) const
	{
//#define INS_WAVESEL  host_from<T, little_endian>(inst[26 + op])
		T waveSel;
		s >> number_format<T, little_endian, T>(waveSel);
		o->waveSelect = waveSel & 0x07;
		return;
	}

	private:
		OPLPatch& r;
};

/// @sa adlib_patch
template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_write
{
	adlib_patch_write(const OPLPatch& r)
		:	r(r)
	{
	}

	/// Write 28 elements of type T from the given OPL patch into the stream.
	void write(stream::output& s) const
	{
		s
			<< adlibOperator(this->r.m, this->r.feedback, this->r.connection)
			<< adlibOperator(this->r.c, this->r.feedback, this->r.connection)
		;
		this->writeWaveSel(s, this->r.m);
		this->writeWaveSel(s, this->r.c);
		return;
	}

	/// Write one element of type T to the stream from the OPL operator.
	void writeWaveSel(stream::output& s, const OPLOperator& o) const
	{
		s << number_format_const<T, little_endian, uint8_t>(o.waveSelect);
		return;
	}

	private:
		const OPLPatch& r;
};

/// @sa adlib_patch
template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch_const: public adlib_patch_write<T>
{
	adlib_patch_const(const OPLPatch& r)
		:	adlib_patch_write<T>(r)
	{
	}
};

template <typename T>
struct CAMOTO_GAMEMUSIC_API adlib_patch:
	public adlib_patch_read<T>,
	public adlib_patch_write<T>
{
	adlib_patch(OPLPatch& r)
		:	adlib_patch_read<T>(r),
			adlib_patch_write<T>(r)
	{
	}
};

// If you get an error related to the next line (e.g. no match for operator >>)
// it's because you're trying to read a value into a const variable.
template <typename T>
stream::input& operator >> (stream::input& s, const adlib_patch_read<T>& n) {
	n.read(s);
	return s;
}

template <typename T>
stream::output& operator << (stream::output& s, const adlib_patch_write<T>& n) {
	n.write(s);
	return s;
}

/**
 * adlibPatch will read a standard AdLib patch from a stream, e.g.
 *
 * @code
 * OPLPatch n;
 * file >> adlibPatch<uint8_t>(n);  // read 28-bytes
 * file >> adlibPatch<uint16_t>(n); // read 56-bytes
 * @endcode
 *
 * All Adlib patches are 28 fields long, and usually written as bytes, but some
 * earlier formats write them as 16-bit little-endian integers instead.  The
 * template type controls the field width.  At present the 16-bit version is
 * little-endian only.  This may change if a file is ever found that stores
 * the values in big-endian.
 *
 * This method can also be used when writing to a stream, e.g.
 *
 * @code
 * OPLPatch n;
 * file << adlibPatch<uint8_t>(n);   // write 28 bytes
 * file << adlibPatch<uint16_t>(n);  // write 56 bytes
 * @endcode
 */
template <typename T>
adlib_patch<T> adlibPatch(OPLPatch& r)
{
	return adlib_patch<T>(r);
}

template <typename T>
adlib_patch_const<T> adlibPatch(const OPLPatch& r)
{
	return adlib_patch_const<T>(r);
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCH_ADLIB_HPP_
