/**
 * @file   test-mus-imf-idsoftware-type1.cpp
 * @brief  Test code for type-1 id Software IMF files.
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

#include "test-mus-imf-idsoftware-common.hpp"

#define testdata_noteonoff \
	"\x3c\x00" \
	imf_commonheader \
	imf_setinstrument \
	imf_noteonoff

#define HAS_OPL_RHYTHM_INSTRUMENTS

#define testdata_rhythm_hihat \
	"\x2c\x00" \
	imf_rhythm_hihat

#define testdata_rhythm_cymbal \
	"\x28\x00" \
	imf_rhythm_cymbal

#define testdata_rhythm_tom \
	"\x2c\x00" \
	imf_rhythm_tom

#define testdata_rhythm_snare \
	"\x28\x00" \
	imf_rhythm_snare

#define testdata_rhythm_bassdrum \
	"\x40\x00" \
	imf_rhythm_bassdrum

#define MUSIC_CLASS fmt_mus_imf_idsoftware_type1
#define MUSIC_TYPE  "imf-idsoftware-type1"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Too short
ISINSTANCE_TEST(c01,
	"\x00"
	,
	MusicType::DefinitelyNo
);

// Invalid register
ISINSTANCE_TEST(c02,
	"\x08\x00"
	"\x00\x00\x00\x00"
	"\xF9\x00\x00\x00"
	,
	MusicType::DefinitelyNo
);

// Delay too large
ISINSTANCE_TEST(c03,
	"\x08\x00"
	"\x00\x00\x00\x00"
	"\xBD\x20\x00\xF0"
	,
	MusicType::DefinitelyNo
);

// Type-1 file with zero length
ISINSTANCE_TEST(c04,
	"\x00\x00"
	"\x00\x00\x00\x00"
	"\x12\x34\x56\x78"
	,
	MusicType::DefinitelyNo
);

// Type-1 file with wrong length
ISINSTANCE_TEST(c05,
	"\x00\x01"
	"\x00\x00\x00\x00"
	"\x12\x34\x56\x78"
	,
	MusicType::DefinitelyNo
);

// Make sure weird short files don't get picked up
ISINSTANCE_TEST(c06,
	"\x01\x00"
	"\x00"
	,
	MusicType::DefinitelyNo
);
