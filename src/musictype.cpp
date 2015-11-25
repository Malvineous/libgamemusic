/**
 * @file  musictype.cpp
 * @brief Utility functions for MusicType.
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
#include <camoto/gamemusic/musictype.hpp>

using namespace camoto;
using namespace camoto::gamemusic;

CAMOTO_GAMEMUSIC_API std::ostream& camoto::gamemusic::operator<< (
	std::ostream& s, const MusicType::Certainty& r)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch-enum"
	s << "MusicType::Certainty::";
	switch (r) {
		case MusicType::Certainty::DefinitelyNo: s << "DefinitelyNo"; break;
		case MusicType::Certainty::Unsure: s << "Unsure"; break;
		case MusicType::Certainty::PossiblyYes: s << "PossiblyYes"; break;
		case MusicType::Certainty::DefinitelyYes: s << "DefinitelyYes"; break;
		default: s << "???"; break;
	}
#pragma GCC diagnostic pop
	return s;
}
