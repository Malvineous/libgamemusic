/**
 * @file   test-mus-imf-idsoftware-common.hpp
 * @brief  Test code shared between both types of id Software IMF files.
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

#define imf_commonheader \
	"\x00\x00" "\x00\x00"

// F-num 577 is 440.2Hz in block 4 (w/ conversion constant of 50,000)
// F-num 580 is 439.9Hz in block 4 (w/ conversion constant of 49,716)
#define imf_noteonoff \
	"\xA1\x44" "\x00\x00" \
	"\xB1\x32" "\x10\x00" \
	"\xB1\x12" "\x00\x00"

#define imf_setinstrument \
	"\x21\xae" "\x00\x00" \
	"\x41\x7f" "\x00\x00" \
	"\x61\xed" "\x00\x00" \
	"\x81\xcb" "\x00\x00" \
	"\xe1\x06" "\x00\x00" \
	"\x24\xa7" "\x00\x00" \
	"\x44\x1f" "\x00\x00" \
	"\x64\x65" "\x00\x00" \
	"\x84\x43" "\x00\x00" \
	"\xe4\x02" "\x00\x00" \
	"\xc1\x34" "\x00\x00"

#define imf_rhythm_hihat \
	"\x00\x00\x00\x00" \
	"\x31\xae" "\x00\x00" \
	"\x51\x7f" "\x00\x00" \
	"\x71\xed" "\x00\x00" \
	"\x91\xcb" "\x00\x00" \
	"\xf1\x06" "\x00\x00" \
	\
	"\xc7\x39" "\x00\x00" \
	\
	"\xa7\x44" "\x00\x00" \
	"\xb7\x12" "\x00\x00" \
	\
	"\xbd\x21" "\x10\x00" \
	"\xbd\x20" "\x00\x00"

#define imf_rhythm_cymbal \
	"\x00\x00\x00\x00" \
	"\x35\xae" "\x00\x00" \
	"\x55\x7f" "\x00\x00" \
	"\x75\xed" "\x00\x00" \
	"\x95\xcb" "\x00\x00" \
	"\xf5\x06" "\x00\x00" \
	\
	"\xa8\x44" "\x00\x00" \
	"\xb8\x12" "\x00\x00" \
	\
	"\xbd\x22" "\x10\x00" \
	"\xbd\x20" "\x00\x00"

#define imf_rhythm_tom \
	"\x00\x00\x00\x00" \
	"\x32\xae" "\x00\x00" \
	"\x52\x7f" "\x00\x00" \
	"\x72\xed" "\x00\x00" \
	"\x92\xcb" "\x00\x00" \
	"\xf2\x06" "\x00\x00" \
	\
	"\xc8\x39" "\x00\x00" \
	\
	"\xa8\x44" "\x00\x00" \
	"\xb8\x12" "\x00\x00" \
	\
	"\xbd\x24" "\x10\x00" \
	"\xbd\x20" "\x00\x00"

#define imf_rhythm_snare \
	"\x00\x00\x00\x00" \
	"\x34\xae" "\x00\x00" \
	"\x54\x7f" "\x00\x00" \
	"\x74\xed" "\x00\x00" \
	"\x94\xcb" "\x00\x00" \
	"\xf4\x06" "\x00\x00" \
	\
	"\xa7\x44" "\x00\x00" \
	"\xb7\x12" "\x00\x00" \
	\
	"\xbd\x28" "\x10\x00" \
	"\xbd\x20" "\x00\x00"

#define imf_rhythm_bassdrum \
	"\x00\x00\x00\x00" \
	"\x30\xae" "\x00\x00" \
	"\x50\x7f" "\x00\x00" \
	"\x70\xed" "\x00\x00" \
	"\x90\xcb" "\x00\x00" \
	"\xf0\x06" "\x00\x00" \
	\
	"\x33\xae" "\x00\x00" \
	"\x53\x7f" "\x00\x00" \
	"\x73\xed" "\x00\x00" \
	"\x93\xcb" "\x00\x00" \
	"\xf3\x06" "\x00\x00" \
	\
	"\xc6\x39" "\x00\x00" \
	\
	"\xa6\x44" "\x00\x00" \
	"\xb6\x12" "\x00\x00" \
	\
	"\xbd\x30" "\x10\x00" \
	"\xbd\x20" "\x00\x00"
