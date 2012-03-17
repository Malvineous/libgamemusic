/**
 * @file   main.cpp
 * @brief  Entry point for libgamemusic.
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

#include <string>
#include <camoto/debug.hpp>
#include <camoto/gamemusic.hpp>

// Include all the file formats for the Manager to load
#include "mus-imf-idsoftware.hpp"
#include "mus-dro-dosbox-v1.hpp"
#include "mus-dro-dosbox-v2.hpp"
#include "mus-raw-rdos.hpp"
#include "mus-rawmidi.hpp"
#include "mus-cmf-creativelabs.hpp"
#include "mus-klm-wacky.hpp"
#include "mus-mid-type0.hpp"

namespace camoto {
namespace gamemusic {

ManagerPtr getManager()
	throw ()
{
	return ManagerPtr(new Manager());
}

Manager::Manager()
	throw ()
{
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IMF_Type0()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IMF_Type1()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_WLF_Type0()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_WLF_Type1()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IMF_Duke2()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_DRO_v1()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_DRO_v2()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_RAW()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_RawMIDI()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_CMF()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_KLM()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_MID_Type0()));
}

Manager::~Manager()
	throw ()
{
}

MusicTypePtr Manager::getMusicType(unsigned int index)
	throw ()
{
	if (index >= this->musicTypes.size()) return MusicTypePtr();
	return this->musicTypes[index];
}

MusicTypePtr Manager::getMusicTypeByCode(const std::string& code)
	throw ()
{
	for (VC_MUSICTYPE::const_iterator i = this->musicTypes.begin(); i != this->musicTypes.end(); i++) {
		if ((*i)->getCode().compare(code) == 0) return *i;
	}
	return MusicTypePtr();
}

} // namespace gamegraphics
} // namespace camoto
