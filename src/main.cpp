/**
 * @file   main.cpp
 * @brief  Entry point for libgamemusic.
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

#include <camoto/gamemusic/manager.hpp>

// Include all the file formats for the Manager to load
#include "mus-imf-idsoftware.hpp"
#include "mus-dro-dosbox-v1.hpp"
#include "mus-dro-dosbox-v2.hpp"
#include "mus-got.hpp"
#include "mus-raw-rdos.hpp"
#include "mus-cmf-creativelabs.hpp"
#include "mus-cdfm-zone66.hpp"
#include "mus-klm-wacky.hpp"
#include "mus-mid-type0.hpp"
#include "mus-ibk-instrumentbank.hpp"

namespace camoto {
namespace gamemusic {

class ActualManager: virtual public Manager
{
	public:
		ActualManager();
		~ActualManager();

		const MusicTypePtr getMusicType(unsigned int index) const;
		const MusicTypePtr getMusicTypeByCode(const std::string& code) const;

	private:
		MusicTypeVector musicTypes;  ///< List of available music types
};

const ManagerPtr getManager()
{
	return ManagerPtr(new ActualManager());
}

ActualManager::ActualManager()
{
	this->musicTypes.push_back(MusicTypePtr(new MusicType_CMF()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_CDFM()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_DRO_v1()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_DRO_v2()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_GOT()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IBK()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IMF_Type0()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IMF_Type1()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_WLF_Type0()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_WLF_Type1()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_IMF_Duke2()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_KLM()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_MID_Type0()));
	this->musicTypes.push_back(MusicTypePtr(new MusicType_RAW()));
}

ActualManager::~ActualManager()
{
}

const MusicTypePtr ActualManager::getMusicType(unsigned int index) const
{
	if (index >= this->musicTypes.size()) return MusicTypePtr();
	return this->musicTypes[index];
}

const MusicTypePtr ActualManager::getMusicTypeByCode(const std::string& code)
	const
{
	for (MusicTypeVector::const_iterator
		i = this->musicTypes.begin(); i != this->musicTypes.end(); i++
	) {
		if ((*i)->getCode().compare(code) == 0) return *i;
	}
	return MusicTypePtr();
}

} // namespace gamegraphics
} // namespace camoto
