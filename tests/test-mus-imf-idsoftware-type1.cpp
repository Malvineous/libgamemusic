/**
 * @file   test-mus-imf-idsoftware-type1.cpp
 * @brief  Test code for type-1 id Software IMF files.
 *
 * Copyright (C) 2010-2015 Adam Nielsen <malvineous@shikadi.net>
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

class test_imf_idsoftware_type1: public test_music
{
	public:
		test_imf_idsoftware_type1()
		{
			this->type = "imf-idsoftware-type1";
			this->numInstruments = 1;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
			this->skipInstDetect.push_back("wlf-idsoftware-type1");
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Too short
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x00"
			));

			// c02: Invalid register
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x08\x00"
				"\x00\x00\x00\x00"
				"\xF9\x00\x00\x00"
			));

			// c03: Delay too large
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x08\x00"
				"\x00\x00\x00\x00"
				"\xBD\x20\x00\xF0"
			));

			// c04: Type-1 file with wrong length
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\x12\x34\x56\x78"
			));

			// c05: Short but valid file
			this->isInstance(MusicType::DefinitelyYes, STRING_WITH_NULLS(
				"\x04\x00"
				"\x00\x00\x00\x00"
			));

			// c06: Truncated file
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x00\x00\x00\x00"
				"\xBD\x20\x00"
			));

			// c07: Make sure weird short files don't get picked up
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x01\x00"
				"\x00"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"\x40\x00"

				"\x00\x00" "\x00\x00"
				"\x00\x00" "\x20\x00" // leading delay
				// Set instrument
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
				"\xc1\x04" "\x00\x00"
				// Note on/off
				"\xa1\x44" "\x00\x00"
				"\xb1\x32" "\x10\x00"
				"\xb1\x12" "\x30\x00" // trailing delay
			);
		}
};

IMPLEMENT_TESTS(imf_idsoftware_type1);
