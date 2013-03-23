/**
 * @file   test-mus-imf-idsoftware-type0.cpp
 * @brief  Test code for type-0 id Software IMF files.
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
	imf_commonheader \
	imf_setinstrument \
	imf_noteonoff

#define HAS_OPL_RHYTHM_INSTRUMENTS

#define testdata_rhythm_hihat \
	imf_rhythm_hihat

#define testdata_rhythm_cymbal \
	imf_rhythm_cymbal

#define testdata_rhythm_tom \
	imf_rhythm_tom

#define testdata_rhythm_snare \
	imf_rhythm_snare

#define testdata_rhythm_bassdrum \
	imf_rhythm_bassdrum

#define MUSIC_CLASS fmt_mus_imf_idsoftware_type0
#define MUSIC_TYPE  "imf-idsoftware-type0"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test the OPL volume functions (this isn't specific to this format, it's just
// a convenient place to put it!)
TEST_BEFORE_AFTER(opl_volume,
	"\x00\x00" "\x00\x00"
	"\x21\xae" "\x00\x00"
	"\x41\x7f" "\x00\x00"
	"\x61\xed" "\x00\x00"
	"\x81\xcb" "\x00\x00"
	"\xe1\x06" "\x00\x00"
	"\x24\xa7" "\x00\x00"
	"\x44\x1f" "\x00\x00"
	"\x64\x65" "\x00\x00"
	"\x84\x43" "\x00\x00"
	"\xe4\x02" "\x00\x00"
	"\xc1\x34" "\x00\x00"

	"\xA1\x44" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x00" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x0f" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x1e" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x01" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"
	,
	"\x00\x00" "\x00\x00"
	"\x21\xae" "\x00\x00"
	"\x41\x7f" "\x00\x00"
	"\x61\xed" "\x00\x00"
	"\x81\xcb" "\x00\x00"
	"\xe1\x06" "\x00\x00"
	"\x24\xa7" "\x00\x00"
	"\x44\x1f" "\x00\x00"
	"\x64\x65" "\x00\x00"
	"\x84\x43" "\x00\x00"
	"\xe4\x02" "\x00\x00"
	"\xc1\x34" "\x00\x00"

	"\xA1\x44" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x00" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x0f" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x1e" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x10\x00"

	"\x44\x01" "\x00\x00"
	"\xB1\x32" "\x10\x00"
	"\xB1\x12" "\x00\x00" // note trailing delay removed here
);

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
	"\x00\x00\x00\x00"
	"\xF9\x00\x00\x00"
	,
	MusicType::DefinitelyNo
);

// Delay too large
ISINSTANCE_TEST(c03,
	"\x00\x00\x00\x00"
	"\xBD\x20\x00\xF0"
	,
	MusicType::DefinitelyNo
);

// Type-0 file with nonzero length
ISINSTANCE_TEST(c04,
	"\x04\x00\x00\x00"
	"\x12\x34\x56\x78"
	,
	MusicType::DefinitelyNo
);
