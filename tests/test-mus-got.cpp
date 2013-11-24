/**
 * @file   test-mus-got.cpp
 * @brief  Test code for Zone 66 CDFM files.
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

#define INITIAL_TEMPO 1000
#define INSTRUMENT_TYPE  0 // OPL

#define testdata_noteonoff \
	"\x01\x00" \
	"\x00\x20\xAE" \
	"\x00\x40\x7F" \
	"\x00\x60\xED" \
	"\x00\x80\xCB" \
	"\x00\xE0\x06" \
	"\x00\x23\xA7" \
	"\x00\x43\x1F" \
	"\x00\x63\x65" \
	"\x00\x83\x43" \
	"\x00\xE3\x02" \
	"\x00\xC0\x34" \
	\
	"\x00\xA0\x44" \
	"\x01\xB0\x32" \
	"\x00\xB0\x12" \
	\
	"\x00\x00\x00" "\x00"

#define DETECTION_UNCERTAIN
#define MUSIC_CLASS fmt_mus_got
#define MUSIC_TYPE  "got"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Too short
ISINSTANCE_TEST(c01,
	"\x01"
	"\x00\x00\x00" "\x00"
	,
	MusicType::DefinitelyNo
);

// Uneven length
ISINSTANCE_TEST(c02,
	"\x01\x00"
	"\x00\x20\xAE"
	"\x00"
	"\x00\x00\x00" "\x00"
	,
	MusicType::DefinitelyNo
);

// Bad signature
ISINSTANCE_TEST(c03,
	"\x02\x00"
	"\x00\x20\xAE"
	"\x00\x40\x7F"
	"\x00\x60\xED"
	"\x00\x80\xCB"
	"\x00\xE0\x06"
	"\x00\x23\xA7"
	"\x00\x43\x1F"
	"\x00\x63\x65"
	"\x00\x83\x43"
	"\x00\xE3\x02"
	"\x00\xC0\x34"

	"\x00\xA0\x44"
	"\x01\xB0\x32"
	"\x00\xB0\x12"

	"\x00\x00\x00" "\x00"
	,
	MusicType::DefinitelyNo
);

// Missing/incomplete loop-to-start marker
ISINSTANCE_TEST(c04,
	"\x01\x00"
	"\x00\x20\xAE"
	"\x00\x40\x7F"
	"\x00\x60\xED"
	"\x00\x80\xCB"
	"\x00\xE0\x06"
	"\x00\x23\xA7"
	"\x00\x43\x1F"
	"\x00\x63\x65"
	"\x00\x83\x43"
	"\x00\xE3\x02"
	"\x00\xC0\x34"

	"\x00\xA0\x44"
	"\x01\xB0\x32"
	"\x00\xB0\x12"

	"\x00\x00\x00"
	,
	MusicType::DefinitelyNo
);
