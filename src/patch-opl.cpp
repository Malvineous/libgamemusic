/**
 * @file  patch-opl.cpp
 * @brief Implementation of OPL Patch class, for managing OPL patches.
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

#include <iostream>
#include <camoto/gamemusic/patch-opl.hpp>

using namespace camoto::gamemusic;

std::ostream& operator << (std::ostream& s, const OPLPatchPtr p)
{
	s << *(p.get());
	return s;
}

std::ostream& operator << (std::ostream& s, const OPLOperator& o)
{
	s
		<< (o.enableTremolo ? 'T' : 't')
		<< (o.enableVibrato ? 'V' : 'v')
		<< (o.enableSustain ? 'S' : 's')
		<< (o.enableKSR ? 'K' : 'k') << '.'
		<< std::setfill('0') << std::setw(2) << (int)o.freqMult << '.'
		<< std::setfill('0') << std::setw(2) << (int)o.scaleLevel << '-'
		<< std::setfill('0') << std::setw(2) << (int)o.outputLevel << '.'
		<< std::setfill('0') << std::setw(2) << (int)o.attackRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)o.decayRate << '.'
		<< std::setfill('0') << std::setw(2) << (int)o.sustainRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)o.releaseRate << '.'
		<< (int)o.waveSelect
	;
	return s;
}

std::ostream& operator << (std::ostream& s, const OPLPatch& p)
{
	s << "[" << std::hex
		<< (int)p.rhythm << ":"
		<< (int)p.feedback << '.'
		<< (p.connection ? 'C' : 'c')
		<< '/'
	;
	if (!oplModOnly(p.rhythm)) {
		s << p.c;
	}
	if (!oplCarOnly(p.rhythm)) {
		if (!oplModOnly(p.rhythm)) s << '/';
		s << p.m;
	}
	s
		<< ']' << std::dec
	;
	if (oplModOnly(p.rhythm)) {
		s << " {Unused C: " << p.c << "}";
	}
	if (oplCarOnly(p.rhythm)) {
		s << " {Unused M: " << p.m << "}";
	}
	return s;
}

const char *camoto::gamemusic::rhythmToText(OPLPatch::Rhythm rhythm)
{
	switch (rhythm) {
		case OPLPatch::Melodic: return "normal (non-rhythm) instrument";
		case OPLPatch::HiHat: return "hi-hat";
		case OPLPatch::TopCymbal: return "top cymbal";
		case OPLPatch::TomTom: return "tom tom";
		case OPLPatch::SnareDrum: return "snare drum";
		case OPLPatch::BassDrum: return "bass drum";
		default: return "[unknown instrument type]";
	}
}

OPLOperator::OPLOperator()
	:	enableTremolo(false),
		enableVibrato(false),
		enableSustain(false),
		enableKSR(false),
		freqMult(0),
		scaleLevel(0),
		outputLevel(0),
		attackRate(0),
		decayRate(0),
		sustainRate(0),
		releaseRate(0),
		waveSelect(0)
{
}

OPLPatch::OPLPatch()
	:	feedback(0),
		connection(false),
		rhythm(OPLPatch::Melodic)
{
}
