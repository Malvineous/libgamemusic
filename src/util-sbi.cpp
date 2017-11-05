/**
 * @file  util-sbi.cpp
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

#include <strings.h>
#include "util-sbi.hpp"

/// Length of each instrument, in bytes
const unsigned int SBI_INST_LEN = 16;

namespace camoto {
namespace gamemusic {

sbi_instrument_read::sbi_instrument_read(OPLPatch& r)
	:	r(r)
{
}

void sbi_instrument_read::read(stream::input& s) const
{
	uint8_t inst[SBI_INST_LEN];
	s.read((char *)inst, SBI_INST_LEN);

	OPLOperator *o = &this->r.m;
	for (int op = 0; op < 2; op++) {
		o->enableTremolo = (inst[0 + op] >> 7) & 1;
		o->enableVibrato = (inst[0 + op] >> 6) & 1;
		o->enableSustain = (inst[0 + op] >> 5) & 1;
		o->enableKSR     = (inst[0 + op] >> 4) & 1;
		o->freqMult      =  inst[0 + op] & 0x0F;
		o->scaleLevel    =  inst[2 + op] >> 6;
		o->outputLevel   =  inst[2 + op] & 0x3F;
		o->attackRate    =  inst[4 + op] >> 4;
		o->decayRate     =  inst[4 + op] & 0x0F;
		o->sustainRate   =  inst[6 + op] >> 4;
		o->releaseRate   =  inst[6 + op] & 0x0F;
		o->waveSelect    =  inst[8 + op] & 0x07;
		o = &this->r.c;
	}
	this->r.feedback    = (inst[10] >> 1) & 0x07;
	this->r.connection  =  inst[10] & 1;
	this->r.rhythm      = OPLPatch::Rhythm::Melodic;
	return;
}

sbi_instrument_write::sbi_instrument_write(const OPLPatch& r)
	:	r(r)
{
}

void sbi_instrument_write::write(stream::output& s) const
{
	uint8_t inst[16];
	bzero(inst, 16);
	auto *o = &this->r.m;
	for (int op = 0; op < 2; op++) {
		inst[0 + op] =
			((o->enableTremolo & 1) << 7) |
			((o->enableVibrato & 1) << 6) |
			((o->enableSustain & 1) << 5) |
			((o->enableKSR     & 1) << 4) |
			(o->freqMult      & 0x0F)
			;
		inst[2 + op] = (o->scaleLevel << 6) | (o->outputLevel & 0x3F);
		inst[4 + op] = (o->attackRate << 4) | (o->decayRate & 0x0F);
		inst[6 + op] = (o->sustainRate << 4) | (o->releaseRate & 0x0F);
		inst[8 + op] =  o->waveSelect & 7;

		o = &this->r.c;
	}
	inst[10] = ((this->r.feedback & 7) << 1) | (this->r.connection & 1);
	inst[11] = inst[12] = inst[13] = inst[14] = inst[15] = 0;

	/// @todo handle these
	//this->r.deepTremolo = false;
	//this->r.deepVibrato = false;

	s.write((char *)inst, 16);
	return;
}

sbi_instrument_const::sbi_instrument_const(const OPLPatch& r)
	:	sbi_instrument_write(r)
{
}

sbi_instrument::sbi_instrument(OPLPatch& r)
	:	sbi_instrument_read(r),
		sbi_instrument_write(r)
{
}

} // namespace gamemusic
} // namespace camoto
