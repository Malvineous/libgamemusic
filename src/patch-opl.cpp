/**
 * @file   patch-opl.cpp
 * @brief  Implementation of OPL Patch class, for managing OPL patches.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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

OPLOperator::OPLOperator()
	throw () :
		enableTremolo(false),
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
	throw () :
		feedback(0),
		connection(false),
		rhythm(0),
		deepTremolo(false),
		deepVibrato(false)
{
}
