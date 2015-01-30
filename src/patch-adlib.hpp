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

/// Read 13 elements of type T from the stream and populate an OPL operator.
template <typename T>
void readAdLibOperator(stream::input_sptr ins, OPLOperator *o,
	uint8_t *feedback, bool *connection)
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
	ins->read((uint8_t *)&inst[0], sizeof(inst));
	o->enableTremolo = INS_AM ? 1 : 0;
	o->enableVibrato = INS_VIB ? 1 : 0;
	o->enableSustain = INS_EG ? 1 : 0;
	o->enableKSR     = INS_KSR ? 1 : 0;
	o->freqMult      = INS_MULTIPLE & 0x0f;
	o->scaleLevel    = INS_KSL & 0x03;
	o->outputLevel   = INS_LEVEL & 0x3f;
	o->attackRate    = INS_ATTACK & 0x0f;
	o->decayRate     = INS_DECAY & 0x0f;
	o->sustainRate   = INS_SUSTAIN & 0x0f;
	o->releaseRate   = INS_RELEASE & 0x0f;
//	o->waveSelect    = INS_WAVESEL & 0x07;
	if (feedback) *feedback = INS_FEEDBACK & 7;
	if (connection) *connection = INS_CON ? 0 : 1;
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

/// Read one element of type T from the stream and populate an OPL operator.
template <typename T>
void readAdLibWaveSel(stream::input_sptr ins, OPLOperator *o)
{
//#define INS_WAVESEL  host_from<T, little_endian>(inst[26 + op])
	T waveSel;
	ins >> number_format<T, little_endian, T>(waveSel);
	o->waveSelect = waveSel & 0x07;
	return;
}

/// Read 28 elements of type T from the stream and populate an OPL patch.
template <typename T>
void readAdLibPatch(stream::input_sptr ins, OPLPatch *oplPatch)
{
	// The instruments store both a Modulator and a Carrier value for the
	// Feedback and Connection, but the OPL only uses one value for each
	// Modulator+Carrier pair.  Both values often seem to be set the same,
	// however the official docs say to use op0 and ignore the op1 value.
	// same, so we can just pick the Modulator's value here.
	readAdLibOperator<T>(ins, &oplPatch->m, &oplPatch->feedback, &oplPatch->connection);
	readAdLibOperator<T>(ins, &oplPatch->c, NULL, NULL);
	readAdLibWaveSel<T>(ins, &oplPatch->m);
	readAdLibWaveSel<T>(ins, &oplPatch->c);
	return;
}

/// Write 13 elements of type T from the stream and populate an OPL operator.
template <typename T>
void writeAdLibOperator(stream::output_sptr ins, const OPLOperator& o,
	const uint8_t& feedback, const bool& connection)
{
	T inst[13];
	inst[0] = host_to<T, little_endian>(o.scaleLevel);  // KSL
	inst[1] = host_to<T, little_endian>(o.freqMult);    // MULTIPLE
	inst[2] = host_to<T, little_endian>(feedback);       // FEEDBACK
	inst[3] = host_to<T, little_endian>(o.attackRate);  // ATTACK
	inst[4] = host_to<T, little_endian>(o.sustainRate); // SUSTAIN
	inst[5] = host_to<T, little_endian>(o.enableSustain ? 1 : 0); // EG
	inst[6] = host_to<T, little_endian>(o.decayRate);   // DECAY
	inst[7] = host_to<T, little_endian>(o.releaseRate); // RELEASE
	inst[8] = host_to<T, little_endian>(o.outputLevel); // LEVEL
	inst[9] = host_to<T, little_endian>(o.enableTremolo ? 1 : 0); // AM
	inst[10] = host_to<T, little_endian>(o.enableVibrato ? 1 : 0); // VIB
	inst[11] = host_to<T, little_endian>(o.enableKSR ? 1 : 0); // KSR
	inst[12] = host_to<T, little_endian>(connection ? 0 : 1); // CON

	ins->write((uint8_t *)inst, sizeof(inst));
	return;
}

/// Write one element of type T from the stream and populate an OPL operator.
template <typename T>
void writeAdLibWaveSel(stream::output_sptr ins, const OPLOperator& o)
{
	ins << number_format_const<T, little_endian, uint8_t>(o.waveSelect);
	return;
}

/// Write 28 elements of type T from the given OPL patch into the stream.
template <typename T>
void writeAdLibPatch(stream::output_sptr ins, const OPLPatch& oplPatch)
{
	// The instruments store both a Modulator and a Carrier value for the
	// Feedback and Connection, but the OPL only uses one value for each
	// Modulator+Carrier pair.  Both values often seem to be set the same,
	// however the official docs say to use op0 and ignore the op1 value.
	// same, so we can just pick the Modulator's value here.
	writeAdLibOperator<T>(ins, oplPatch.m, oplPatch.feedback, oplPatch.connection);
	writeAdLibOperator<T>(ins, oplPatch.c, oplPatch.feedback, oplPatch.connection);
	writeAdLibWaveSel<T>(ins, oplPatch.m);
	writeAdLibWaveSel<T>(ins, oplPatch.c);
	return;
}

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_PATCH_ADLIB_HPP_
