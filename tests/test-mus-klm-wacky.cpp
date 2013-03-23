/**
 * @file   test-mus-klm-wacky.cpp
 * @brief  Test code for Wacky Wheels KLM files.
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

#define INITIAL_TEMPO 3571 // Some arbitrary value
#define INSTRUMENT_TYPE  0 // OPL

#define KLM_INSTRUMENT_BLOCK \
	"\x7f\x1f\xed\x65\xcb\x43\xae\xa7\x06\x02\x04"

#define testdata_noteonoff \
	"\x18\x01" "\x01" "\x10\x00" \
	KLM_INSTRUMENT_BLOCK \
	"\x30\x00" \
	"\x10\x44\x32" \
	"\xfd\x10" \
	"\x00" \
	"\xff"

//#define HAS_OPL_RHYTHM_INSTRUMENTS

#define testdata_rhythm_hihat \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x31\xae" \
	"\x51\x7f" \
	"\x71\xed" \
	"\x91\xcb" \
	"\xf1\x06" \
	"\xbd\x21" "\x00\x0f" \
	"\xbd\x20"

#define testdata_rhythm_cymbal \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x35\xae" \
	"\x55\x7f" \
	"\x75\xed" \
	"\x95\xcb" \
	"\xf5\x06" \
	"\xbd\x22" "\x00\x0f" \
	"\xbd\x20"

#define testdata_rhythm_tom \
	"DBRAWOPL" "\x00\x00\x01\x00" \
	"\x10\x00\x00\x00" "\x14\x00\x00\x00" "\x00\x00\x00\x00" \
	"\x32\xae" \
	"\x52\x7f" \
	"\x72\xed" \
	"\x92\xcb" \
	"\xf2\x06" \
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
	"\x10\x00\x00\x00" "\x1e\x00\x00\x00" "\x00\x00\x00\x00" \
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
	"\xa6\x44" \
	"\xb6\x12" \
	\
	"\xbd\x30" "\x00\x0f" \
	"\xbd\x20"

#define MUSIC_CLASS fmt_mus_klm_wacky
#define MUSIC_TYPE  "klm-wacky"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.


// Instrument block length is wrong multiple
ISINSTANCE_TEST(c01,
	"\x18\x01" "\x01" "\x0F\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfd\x10"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// Music offset past EOF
ISINSTANCE_TEST(c02,
	"\x18\x01" "\x01" "\xF0\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfd\x10"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// Invalid 0xF0 event type
ISINSTANCE_TEST(c03,
	"\x18\x01" "\x01" "\x10\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfe\x0f"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// Invalid normal event type
ISINSTANCE_TEST(c04,
	"\x18\x01" "\x01" "\x10\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
	"\x30\x00"
	"\x10\x44\x32"
	"\x55\x0f"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// Truncated event
ISINSTANCE_TEST(c05,
	"\x18\x01" "\x01" "\x10\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
	"\x30\x00"
	"\x10\x44"
	,
	MusicType::DefinitelyNo
);

// Bad instrument in reg 0xE0
ISINSTANCE_TEST(c06,
	"\x18\x01" "\x01" "\x10\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x80\x01\x0A"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfd\x10"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// Bad instrument in 0xE3
ISINSTANCE_TEST(c07,
	"\x18\x01" "\x01" "\x10\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x81\x0A"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfd\x10"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// Bad instrument in 0xC0
ISINSTANCE_TEST(c08,
	"\x18\x01" "\x01" "\x10\x00"
	"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x8A"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfd\x10"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyNo
);

// All valid instrument bits enabled
ISINSTANCE_TEST(c09,
	"\x18\x01" "\x01" "\x10\x00"
	"\xff\xff\xff\xff\xff\xff\xff\xff\x07\x07\x3f"
	"\x30\x00"
	"\x10\x44\x32"
	"\xfd\x10"
	"\x00"
	"\xff"
	,
	MusicType::DefinitelyYes
);
