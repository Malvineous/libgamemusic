/**
 * @file   test-mus-raw-rdos.cpp
 * @brief  Test code for Rdos raw Adlib capture files.
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
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

#include "test-music.hpp"

class test_raw_rdos: public test_music
{
	public:
		test_raw_rdos()
		{
			this->type = "raw-rdos";
			this->numInstruments = 6;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"RAWADATO" "\x50\x08"
			));

			// c02: Too short
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"RAWADATA"
			));

			// c03: Short but valid file
			this->isInstance(MusicType::DefinitelyYes, STRING_WITH_NULLS(
				"RAWADATA" "\x50\x08"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"RAWADATA" "\x50\x08"
				"\x00\x02\x50\x08"
				// Note data
				"\x08\x00" // leading delay
				"\xae\x20\x7f\x40\xed\x60\xcb\x80\x06\xe0"
				"\xa7\x23\x1f\x43\x65\x63\x43\x83\x02\xe3\x04\xc0"
				"\x44\xa0"
				"\x32\xb0"
				"\x10\x00"
				"\x12\xb0"
				// Rhythm hi-hat
				"\xae\x31"
				"\x7f\x51"
				"\xdd\x71"
				"\xcb\x91"
				"\x06\xf1"
				"\x44\xa7"
				"\x12\xb7"
				"\x21\xbd" "\x10\x00"
				"\x20\xbd" "\x04\x00"
				// Rhythm top-cymbal
				"\xae\x35"
				"\x7f\x55"
				"\xcd\x75"
				"\xcb\x95"
				"\x06\xf5"
				"\x44\xa8"
				"\x12\xb8"
				"\x22\xbd" "\x10\x00"
				"\x20\xbd" "\x04\x00"
				// Rhythm tom-tom
				"\xae\x32"
				"\x7f\x52"
				"\xbd\x72"
				"\xcb\x92"
				"\x06\xf2"
				"\x45\xa8"
				"\x13\xb8"
				"\x24\xbd" "\x10\x00"
				"\x20\xbd" "\x04\x00"
				// Rhythm snare
				"\xae\x34"
				"\x7f\x54"
				"\xad\x74"
				"\xcb\x94"
				"\x06\xf4"
				"\x45\xa7"
				"\x13\xb7"
				"\x28\xbd" "\x10\x00"
				"\x20\xbd" "\x04\x00"
				// Rhythm bass-drum
				"\xae\x30"
				"\x7f\x50"
				"\x9d\x70"
				"\xcb\x90"
				"\x06\xf0"
				"\xae\x33"
				"\x7f\x53"
				"\x8d\x73"
				"\xcb\x93"
				"\x06\xf3"
				"\x44\xa6"
				"\x12\xb6"
				"\x30\xbd" "\x10\x00"
				"\x20\xbd" "\x04\x00" // trailing delay
/*
				// Enable OPL3
				"\x00\x02" "\x01\x05"
				"\xe3\x34"
				"\x32\xb0"
				"\x10\x00"
				"\x12\xb0"
				"\x04\x00" // trailing delay
*/
				"\xff\xff"
			);
		}
};

IMPLEMENT_TESTS(raw_rdos);
