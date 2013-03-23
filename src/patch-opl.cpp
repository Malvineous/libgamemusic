/**
 * @file   patch-opl.cpp
 * @brief  Implementation of OPL Patch class, for managing OPL patches.
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

#include <camoto/gamemusic/patch-opl.hpp>

using namespace camoto::gamemusic;

std::ostream& operator << (std::ostream& s, const OPLPatchPtr p)
{
	s << *(p.get());
	return s;
}

std::ostream& operator << (std::ostream& s, const OPLPatch& p)
{
	s << "[" << std::hex
		<< (int)p.rhythm << ":"
		<< (int)p.feedback << '.'
		<< (p.connection ? 'C' : 'c')
		<< '/'

		<< (p.c.enableTremolo ? 'T' : 't')
		<< (p.c.enableVibrato ? 'V' : 'v')
		<< (p.c.enableSustain ? 'S' : 's')
		<< (p.c.enableKSR ? 'K' : 'k') << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.freqMult << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.scaleLevel << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.c.outputLevel << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.attackRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.c.decayRate << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.c.sustainRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.c.releaseRate << '.'
		<< (int)p.c.waveSelect << '/'

		<< (p.m.enableTremolo ? 'T' : 't')
		<< (p.m.enableVibrato ? 'V' : 'v')
		<< (p.m.enableSustain ? 'S' : 's')
		<< (p.m.enableKSR ? 'K' : 'k') << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.freqMult << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.scaleLevel << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.m.outputLevel << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.attackRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.m.decayRate << '.'
		<< std::setfill('0') << std::setw(2) << (int)p.m.sustainRate << '-'
		<< std::setfill('0') << std::setw(2) << (int)p.m.releaseRate << '.'
		<< (int)p.m.waveSelect

		<< ']' << std::dec
	;
	return s;
}

const char *camoto::gamemusic::rhythmToText(int rhythm)
{
	switch (rhythm) {
		case 0: return "normal (non-rhythm) instrument";
		case 1: return "hi-hat";
		case 2: return "top cymbal";
		case 3: return "tom tom";
		case 4: return "snare drum";
		case 5: return "bass drum";
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
		rhythm(0)
{
}
