/**
 * @file  main.cpp
 * @brief Entry point for libgamemusic.
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

#include <camoto/gamemusic/manager.hpp>

// Include all the file formats for the Manager to load
#include "ins-ins-adlib.hpp"
#include "mus-imf-idsoftware.hpp"
#include "mus-dro-dosbox-v1.hpp"
#include "mus-dro-dosbox-v2.hpp"
#include "mus-got.hpp"
#include "mus-raw-rdos.hpp"
#include "mus-cmf-creativelabs.hpp"
#include "mus-cdfm-zone66.hpp"
#include "mus-cdfm-zone66-gus.hpp"
#include "mus-dsm-dsik.hpp"
#include "mus-klm-wacky.hpp"
#include "mus-mid-type0.hpp"
#include "mus-mus-dmx.hpp"
#include "mus-mus-vinyl.hpp"
#include "mus-ibk-instrumentbank.hpp"
#include "mus-s3m-screamtracker.hpp"
#include "mus-tbsa-doofus.hpp"

using namespace camoto::gamemusic;

namespace camoto {

template <>
const std::vector<std::shared_ptr<const MusicType> > CAMOTO_GAMEMUSIC_API
	FormatEnumerator<MusicType>::formats()
{
	std::vector<std::shared_ptr<const MusicType> > list;
	FormatEnumerator<MusicType>::addFormat<
		// Instruments/instrument banks
		MusicType_INS_AdLib,

		// Songs
		MusicType_CMF,
		MusicType_CDFM,
		MusicType_CDFM_GUS,
		MusicType_DRO_v1,
		MusicType_DRO_v2,
		MusicType_DSM,
		MusicType_GOT,
		MusicType_IBK,
		MusicType_IMF_Type0,
		MusicType_IMF_Type1,
		MusicType_WLF_Type0,
		MusicType_WLF_Type1,
		MusicType_IMF_Duke2,
		MusicType_KLM,
		MusicType_MID_Type0,
		MusicType_MUS,
		MusicType_MUS_Raptor,
		MusicType_MUS_Vinyl,
		MusicType_RAW,
		MusicType_S3M,
		MusicType_TBSA
	>(list);
	return list;
}

namespace gamemusic {

constexpr CAMOTO_GAMEMUSIC_API const char* const MusicType::obj_t_name;

} // namespace gamemusic

} // namespace camoto
