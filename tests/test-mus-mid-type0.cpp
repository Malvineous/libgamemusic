/**
 * @file   test-mus-mid-type0.cpp
 * @brief  Test code for type-0 MIDI files.
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

#define INITIAL_TEMPO  ((double)MIDI_DEF_uS_PER_QUARTER_NOTE / MIDI_DEF_TICKS_PER_QUARTER_NOTE)
#define INSTRUMENT_TYPE  1 // MIDI

#define testdata_noteonoff \
	"MThd\x00\x00\x00\x06" \
	"\x00\x00" \
	"\x00\x01" \
	"\x00\xc0" \
	"MTrk\x00\x00\x00\x15" \
	"\x00\xff\x51\x03\x07\xa1\x20" \
	"\x00\xc0\x00" \
	"\x00\x90\x45\x7f" \
	"\x10\x45\x00" \
	"\x00\xff\x2f\x00"

#define MUSIC_CLASS fmt_mus_mid_type0
#define MUSIC_TYPE  "mid-type0"
#include "test-musictype-read.hpp"
#include "test-musictype-write.hpp"

// Test some invalid formats to make sure they're not identified as valid
// files.  Note that they can still be opened though (by 'force'), this
// only checks whether they look like valid files or not.

// The "c00" test has already been performed in test-musicreader.hpp to ensure
// the initial state is correctly identified as a valid file.

// Wrong signature
ISINSTANCE_TEST(c01,
	"MThf\x00\x00\x00\x06" \
	"\x00\x00" \
	"\x00\x01" \
	"\x00\xc0" \
	"MTrk\x00\x00\x00\x15" \
	"\x00\xff\x51\x03\x07\xa1\x20" \
	"\x00\xc0\x00" \
	"\x00\x90\x45\x7f" \
	"\x10\x45\x00" \
	"\x00\xff\x2f\x00"
	,
	MusicType::DefinitelyNo
);

// Wrong type
ISINSTANCE_TEST(c02,
	"MThd\x00\x00\x00\x06" \
	"\x00\x02" \
	"\x00\x01" \
	"\x00\xc0" \
	"MTrk\x00\x00\x00\x15" \
	"\x00\xff\x51\x03\x07\xa1\x20" \
	"\x00\xc0\x00" \
	"\x00\x90\x45\x7f" \
	"\x10\x45\x00" \
	"\x00\xff\x2f\x00"
	,
	MusicType::DefinitelyNo
);
