/**
 * @file   test-mus-raw-rdos.cpp
 * @brief  Test code for Rdos raw Adlib capture files.
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

#define INITIAL_TEMPO 1785
#define INSTRUMENT_TYPE  0 // OPL

#define testdata_noteonoff \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\xae\x20\x7f\x40\xed\x60\xcb\x80\x06\xe0" \
	"\xa7\x23\x1f\x43\x65\x63\x43\x83\x02\xe3\x34\xc0" \
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
	\
	"\x39\xc7" \
	\
	"\x44\xa7" \
	"\x12\xb7" \
	\
	"\x21\xbd" "\x10\x00" \
	"\x20\xbd" \
	"\xff\xff"

#define testdata_rhythm_cymbal \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\xae\x35" \
	"\x7f\x55" \
	"\xed\x75" \
	"\xcb\x95" \
	"\x06\xf5" \
	\
	"\x44\xa8" \
	"\x12\xb8" \
	\
	"\x22\xbd" "\x10\x00" \
	"\x20\xbd" \
	"\xff\xff"

#define testdata_rhythm_tom \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\xae\x32" \
	"\x7f\x52" \
	"\xed\x72" \
	"\xcb\x92" \
	"\x06\xf2" \
	\
	"\x39\xc8" \
	\
	"\x44\xa8" \
	"\x12\xb8" \
	\
	"\x24\xbd" "\x10\x00" \
	"\x20\xbd" \
	"\xff\xff"

#define testdata_rhythm_snare \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\xae\x34" \
	"\x7f\x54" \
	"\xed\x74" \
	"\xcb\x94" \
	"\x06\xf4" \
	\
	"\x44\xa7" \
	"\x12\xb7" \
	\
	"\x28\xbd" "\x10\x00" \
	"\x20\xbd" \
	"\xff\xff"

#define testdata_rhythm_bassdrum \
	"RAWADATA" "\x50\x08" \
	"\x00\x02\x50\x08" \
	"\xae\x30" \
	"\x7f\x50" \
	"\xed\x70" \
	"\xcb\x90" \
	"\x06\xf0" \
	\
	"\xae\x33" \
	"\x7f\x53" \
	"\xed\x73" \
	"\xcb\x93" \
	"\x06\xf3" \
	\
	"\x39\xc6" \
	\
	"\x44\xa6" \
	"\x12\xb6" \
	\
	"\x30\xbd" "\x10\x00" \
	"\x20\xbd" \
	"\xff\xff"

#define MUSIC_CLASS fmt_mus_raw_rdos
#define MUSIC_TYPE  "raw-rdos"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Wrong signature
ISINSTANCE_TEST(c01,
	"RAWADATO"
	,
	MusicType::DefinitelyNo
);
