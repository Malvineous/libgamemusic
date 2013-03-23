/**
 * @file   test-mus-dro-dosbox-v1.cpp
 * @brief  Test code for DOSBox raw OPL capture files.
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

#define INITIAL_TEMPO 1000 // Native DRO tempo (makes ticks match data)
#define INSTRUMENT_TYPE  0 // OPL

#define testdata_noteonoff \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x1e\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x20\xae\x40\x7f\x60\xed\x80\xcb\xe0\x06" \
	"\x23\xa7\x43\x1f\x63\x65\x83\x43\xe3\x02\xc0\x34" \
	"\xa0\x44" \
	"\xb0\x32" "\x00\x0f" \
	"\xb0\x12"

#define HAS_OPL_RHYTHM_INSTRUMENTS

#define testdata_rhythm_hihat \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x16\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x31\xae" \
	"\x51\x7f" \
	"\x71\xed" \
	"\x91\xcb" \
	"\xf1\x06" \
	\
	"\xc7\x39" \
	\
	"\xa7\x44" \
	"\xb7\x12" \
	\
	"\xbd\x21" "\x00\x0f" \
	"\xbd\x20"

#define testdata_rhythm_cymbal \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x14\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x35\xae" \
	"\x55\x7f" \
	"\x75\xed" \
	"\x95\xcb" \
	"\xf5\x06" \
	\
	"\xa8\x44" \
	"\xb8\x12" \
	\
	"\xbd\x22" "\x00\x0f" \
	"\xbd\x20"

#define testdata_rhythm_tom \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x16\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x32\xae" \
	"\x52\x7f" \
	"\x72\xed" \
	"\x92\xcb" \
	"\xf2\x06" \
	\
	"\xc8\x39" \
	\
	"\xa8\x44" \
	"\xb8\x12" \
	\
	"\xbd\x24" "\x00\x0f" \
	"\xbd\x20"

#define testdata_rhythm_snare \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x14\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x34\xae" \
	"\x54\x7f" \
	"\x74\xed" \
	"\x94\xcb" \
	"\xf4\x06" \
	\
	"\xa7\x44" \
	"\xb7\x12" \
	\
	"\xbd\x28" "\x00\x0f" \
	"\xbd\x20"

#define testdata_rhythm_bassdrum \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x20\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x30\xae" \
	"\x50\x7f" \
	"\x70\xed" \
	"\x90\xcb" \
	"\xf0\x06" \
	\
	"\x33\xae" \
	"\x53\x7f" \
	"\x73\xed" \
	"\x93\xcb" \
	"\xf3\x06" \
	\
	"\xc6\x39" \
	\
	"\xa6\x44" \
	"\xb6\x12" \
	\
	"\xbd\x30" "\x00\x0f" \
	"\xbd\x20"

#define MUSIC_CLASS fmt_mus_dro_dosbox_v1
#define MUSIC_TYPE  "dro-dosbox-v1"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

TEST_BEFORE_AFTER(delay_combining,
	"DBRAWOPL" "\x00\x00\x01\x00"
	"\x37\x03\x01\x00" "\x2f\x00\x00\x00" "\x00\x00\x00\x00"
	"\x20\xae\x40\x7f\x60\xed\x80\xcb\xe0\x06"
	"\x23\xa7\x43\x1f\x63\x65\x83\x43\xe3\x02\xc0\x34"
	"\xa0\x44\xb0\x32"
	"\x00\x0f" "\x00\x0f"
	"\xb0\x12"
	"\x00\x0f" "\x01\x0f\x02"
	"\xb0\x32"
	"\x01\xf0\x80\x01\x05\x80"
	"\xb0\x12"
	,
	"DBRAWOPL" "\x00\x00\x01\x00"
	"\x37\x03\x01\x00" "\x2a\x00\x00\x00" "\x00\x00\x00\x00"
	"\x20\xae\x40\x7f\x60\xed\x80\xcb\xe0\x06"
	"\x23\xa7\x43\x1f\x63\x65\x83\x43\xe3\x02\xc0\x34"
	"\xa0\x44\xb0\x32"
	"\x00\x1f"
	"\xb0\x12"
	"\x01\x1f\x02"
	"\xb0\x32"
	"\x01\xff\xff\x00\xf6"
	"\xb0\x12"
);

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Wrong signature
ISINSTANCE_TEST(c01,
	"DBRAWOPP" "\x00\x00\x01\x00"
	,
	MusicType::DefinitelyNo
);

// Wrong version
ISINSTANCE_TEST(c02,
	"DBRAWOPL" "\x00\x00\x02\x00"
	,
	MusicType::DefinitelyNo
);
