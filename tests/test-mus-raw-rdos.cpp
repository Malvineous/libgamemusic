/**
 * @file   test-mus-raw-rdos.cpp
 * @brief  Test code for Rdos raw Adlib capture files.
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

#define INITIAL_TEMPO 1785

#define testdata_noteonoff \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\x44\xa0" \
	"\x32\xb0" \
	"\x10\x00" \
	"\x12\xb0" \
	"\xff\xff"

#define HAS_OPL_RHYTHM_INSTRUMENTS

#define testdata_rhythm_hihat \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\xae\x31" \
	"\x7f\x51" \
	"\xed\x71" \
	"\xcb\x91" \
	"\x06\xf1" \
	"\x21\xbd" "\x10\x00" \
	"\x20\xbd" \
	"\xff\xff"

#define MUSIC_CLASS fmt_mus_raw_rdos
#define MUSIC_TYPE  "raw-rdos"
#include "test-musicreader.hpp"
#include "test-musicwriter.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Wrong signature
ISINSTANCE_TEST(c01,
	"RAWADATO"
	,
	gm::EC_DEFINITELY_NO
);
