/**
 * @file   test-mus-cdfm-zone66.cpp
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
	"\x05\x02\x02\x01\x01\x01" \
	"\x2A\x00\x00\x00" \
	"\x00\x01" \
	"\x00\x00\x00\x00" \
	"\x09\x00\x00\x00" \
	"\x00\x00\x00\x00" "\x04\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00" \
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A" \
	\
	"\x00\x49\x0F" \
	"\x40\x05" \
	"\x00\x42\x0F" \
	"\x60" \
	\
	"\x04\x32\x0F" \
	"\x40\x05" \
	"\x04\x22\x0F" \
	"\x60" \
	\
	"\x80\x80\x80\x80"

#define MUSIC_CLASS fmt_mus_cdfm_zone66
#define MUSIC_TYPE  "cdfm-zone66"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Bad loop target
ISINSTANCE_TEST(c01,
	"\x05\x02\x02\x01\x01\x03" \
	"\x2A\x00\x00\x00" \
	"\x00\x01" \
	"\x00\x00\x00\x00" \
	"\x09\x00\x00\x00" \
	"\x00\x00\x00\x00" "\x04\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00" \
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A" \
	\
	"\x00\x49\x0F" \
	"\x40\x05" \
	"\x00\x42\x0F" \
	"\x60" \
	\
	"\x04\x32\x0F" \
	"\x40\x05" \
	"\x04\x22\x0F" \
	"\x60" \
	\
	"\x80\x80\x80\x80"
	,
	MusicType::DefinitelyNo
);

// Invalid pattern in sequence
ISINSTANCE_TEST(c02,
	"\x05\x02\x02\x01\x01\x01" \
	"\x2A\x00\x00\x00" \
	"\x03\x01" \
	"\x00\x00\x00\x00" \
	"\x09\x00\x00\x00" \
	"\x00\x00\x00\x00" "\x04\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00" \
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A" \
	\
	"\x00\x49\x0F" \
	"\x40\x05" \
	"\x00\x42\x0F" \
	"\x60" \
	\
	"\x04\x32\x0F" \
	"\x40\x05" \
	"\x04\x22\x0F" \
	"\x60" \
	\
	"\x80\x80\x80\x80"
	,
	MusicType::DefinitelyNo
);

// Pattern offset past EOF
ISINSTANCE_TEST(c03,
	"\x05\x02\x02\x01\x01\x01" \
	"\x2A\x00\x00\x00" \
	"\x00\x01" \
	"\x00\x00\x00\x00" \
	"\xFF\x00\x00\x00" \
	"\x00\x00\x00\x00" "\x04\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00" \
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A" \
	\
	"\x00\x49\x0F" \
	"\x40\x05" \
	"\x00\x42\x0F" \
	"\x60" \
	\
	"\x04\x32\x0F" \
	"\x40\x05" \
	"\x04\x22\x0F" \
	"\x60" \
	\
	"\x80\x80\x80\x80"
	,
	MusicType::DefinitelyNo
);
