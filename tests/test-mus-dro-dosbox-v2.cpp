/**
 * @file   test-mus-dro-dosbox-v2.cpp
 * @brief  Test code for DOSBox raw OPL capture files.
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

#define INITIAL_TEMPO 1000 // Native DRO tempo (makes ticks match data)

#define testdata_noteonoff \
	"DBRAWOPL" "\x02\x00\x00\x00" \
	"\x04\x00\x00\x00" "\x10\x00\x00\x00" \
	"\x00\x00\x00" \
	"\xfe\xff\x02" \
	"\xa0\xb0" \
	"\x00\x44" \
	"\x01\x32" "\xfe\x0f" \
	"\x01\x12"

#define HAS_OPL_RHYTHM_INSTRUMENTS

#define testdata_rhythm_hihat \
	"DBRAWOPL" "\x02\x00\x00\x00" \
	"\x08\x00\x00\x00" "\x10\x00\x00\x00" \
	"\x00\x00\x00" \
	"\xfe\xff\x06" \
	"\x31\x51\x71\x91\xf1\xbd" \
	"\x00\xae" \
	"\x01\x7f" \
	"\x02\xed" \
	"\x03\xcb" \
	"\x04\x06" \
	"\x05\x21" "\xfe\x0f" \
	"\x05\x20"

#define testdata_rhythm_cymbal \
	"DBRAWOPL" "\x02\x00\x00\x00" \
	"\x08\x00\x00\x00" "\x10\x00\x00\x00" \
	"\x00\x00\x00" \
	"\xfe\xff\x06" \
	"\x35\x55\x75\x95\xf5\xbd" \
	"\x00\xae" \
	"\x01\x7f" \
	"\x02\xed" \
	"\x03\xcb" \
	"\x04\x06" \
	"\x05\x22" "\xfe\x0f" \
	"\x05\x20"

#define testdata_rhythm_tom \
	"DBRAWOPL" "\x02\x00\x00\x00" \
	"\x0a\x00\x00\x00" "\x10\x00\x00\x00" \
	"\x00\x00\x00" \
	"\xfe\xff\x08" \
	"\x32\x52\x72\x92\xf2\xa8\xb8\xbd" \
	"\x00\xae" \
	"\x01\x7f" \
	"\x02\xed" \
	"\x03\xcb" \
	"\x04\x06" \
	"\x05\x44" \
	"\x06\x12" \
	"\x07\x24" "\xfe\x0f" \
	"\x07\x20"

#define testdata_rhythm_snare \
	"DBRAWOPL" "\x02\x00\x00\x00" \
	"\x0a\x00\x00\x00" "\x10\x00\x00\x00" \
	"\x00\x00\x00" \
	"\xfe\xff\x08" \
	"\x34\x54\x74\x94\xf4\xa7\xb7\xbd" \
	"\x00\xae" \
	"\x01\x7f" \
	"\x02\xed" \
	"\x03\xcb" \
	"\x04\x06" \
	"\x05\x44" \
	"\x06\x12" \
	"\x07\x28" "\xfe\x0f" \
	"\x07\x20"

#define testdata_rhythm_bassdrum \
	"DBRAWOPL" "\x02\x00\x00\x00" \
	"\x0f\x00\x00\x00" "\x10\x00\x00\x00" \
	"\x00\x00\x00" \
	"\xfe\xff\x0d" \
	"\x30\x50\x70\x90\xf0" \
	"\x33\x53\x73\x93\xf3" \
	"\xa6\xb6\xbd" \
	"\x00\xae" \
	"\x01\x7f" \
	"\x02\xed" \
	"\x03\xcb" \
	"\x04\x06" \
	"\x05\xae" \
	"\x06\x7f" \
	"\x07\xed" \
	"\x08\xcb" \
	"\x09\x06" \
	"\x0a\x44" \
	"\x0b\x12" \
	"\x0c\x30" "\xfe\x0f" \
	"\x0c\x20"

#define MUSIC_CLASS fmt_mus_dro_dosbox_v2
#define MUSIC_TYPE  "dro-dosbox-v2"
#include "test-musicreader.hpp"
#include "test-musicwriter.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Wrong signature
ISINSTANCE_TEST(c01,
	"DBRAWOPP" "\x00\x00\x02\x00"
	,
	gm::EC_DEFINITELY_NO
);

// Wrong version
ISINSTANCE_TEST(c02,
	"DBRAWOPL" "\x00\x00\x01\x00"
	,
	gm::EC_DEFINITELY_NO
);
