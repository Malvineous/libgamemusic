/**
 * @file   patch-midi.cpp
 * @brief  Implementation of MIDI Patch class, for managing MIDI patches.
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

#include <camoto/gamemusic/patch-midi.hpp>

using namespace camoto::gamemusic;

std::ostream& operator << (std::ostream& s, const MIDIPatchPtr p)
{
	s << *(p.get());
	return s;
}

std::ostream& operator << (std::ostream& s, const MIDIPatch& p)
{
	s << "[" << (p.percussion ? 'P' : 'N') << std::hex << p.midiPatch
		<< ']' << std::dec
	;
	return s;
}

MIDIPatch::MIDIPatch()
	:	midiPatch(0),
		percussion(false)
{
}
