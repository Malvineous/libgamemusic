/**
 * @file   test-mus-cdfm-zone66.cpp
 * @brief  Test code for Zone 66 CDFM files.
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
#include "../src/mus-cdfm-zone66-gus.hpp"

class test_cdfm_zone66: public test_music
{
	public:
		test_cdfm_zone66()
		{
			this->type = "cdfm-zone66";
			this->numInstruments = 2;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = 1;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Sample data past EOF
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x55\x00\x00\x00"
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\x0B\x00\x00\x00"
				"\x00\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00"
				"\x09\x63\x49\x12\x34\x06\xC9\x96\x56\x78\x01"
				// Pattern 0
				"\x00\x49\x0F"
				"\x40\x05"
				"\x00\x42\x0F"
				"\x40\x3B"
				"\x60"
				// Pattern 1
				"\x04\x32\x0F"
				"\x40\x05"
				"\x04\x22\x0F"
				"\x40\x3B"
				"\x60"
				// PCM inst 1 sample data
				"\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0"
			));

			// c02: Loop target is past end of song
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x02"
				"\x45\x00\x00\x00"
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\x0B\x00\x00\x00"
				"\x00\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00"
				"\x09\x63\x49\x12\x34\x06\xC9\x96\x56\x78\x01"
				// Pattern 0
				"\x00\x49\x0F"
				"\x40\x05"
				"\x00\x42\x0F"
				"\x40\x3B"
				"\x60"
				// Pattern 1
				"\x04\x32\x0F"
				"\x40\x05"
				"\x04\x22\x0F"
				"\x40\x3B"
				"\x60"
				// PCM inst 1 sample data
				"\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0"
			));

			// c03: Sequence specifies invalid pattern
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x45\x00\x00\x00"
				"\x00\x02"
				"\x00\x00\x00\x00"
				"\x0B\x00\x00\x00"
				"\x00\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00"
				"\x09\x63\x49\x12\x34\x06\xC9\x96\x56\x78\x01"
				// Pattern 0
				"\x00\x49\x0F"
				"\x40\x05"
				"\x00\x42\x0F"
				"\x40\x3B"
				"\x60"
				// Pattern 1
				"\x04\x32\x0F"
				"\x40\x05"
				"\x04\x22\x0F"
				"\x40\x3B"
				"\x60"
				// PCM inst 1 sample data
				"\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0"
			));

			// c04: Pattern data offset is past EOF
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x45\x00\x00\x00"
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\xFF\x00\x00\x00"
				"\x00\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00"
				"\x09\x63\x49\x12\x34\x06\xC9\x96\x56\x78\x01"
				// Pattern 0
				"\x00\x49\x0F"
				"\x40\x05"
				"\x00\x42\x0F"
				"\x40\x3B"
				"\x60"
				// Pattern 1
				"\x04\x32\x0F"
				"\x40\x05"
				"\x04\x22\x0F"
				"\x40\x3B"
				"\x60"
				// PCM inst 1 sample data
				"\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0"
			));

			// c05: Too short: header truncated
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x45\x00\x00"
			));

			// c06: Too short: order list truncated
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x45\x00\x00\x00"
				"\x00"
			));

			// c07: Too short: pattern-offset-list truncated
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x45\x00\x00\x00"
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\x0B\x00\x00"
			));

			// c08: Sample data past EOF, with no PCM instruments
			this->isInstance(MusicType::DefinitelyYes, STRING_WITH_NULLS(
				"\x05\x02\x02\x00\x01\x01"
				"\x55\x00\x00\x00"
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\x0B\x00\x00\x00"
				"\x00\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00"
				"\x09\x63\x49\x12\x34\x06\xC9\x96\x56\x78\x01"
				// Pattern 0
				"\x00\x49\x0F"
				"\x40\x05"
				"\x00\x42\x0F"
				"\x40\x3B"
				"\x60"
				// Pattern 1
				"\x04\x32\x0F"
				"\x40\x05"
				"\x04\x22\x0F"
				"\x40\x3B"
				"\x60"
				// PCM inst 1 sample data
				"\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"\x05\x02\x02\x01\x01\x01"
				"\x45\x00\x00\x00"
				"\x00\x01"
				"\x00\x00\x00\x00"
				"\x0B\x00\x00\x00"
				"\x00\x00\x00\x00" "\x10\x00\x00\x00" "\x00\x00\x00\x00" "\xFF\xFF\xFF\x00"
				"\x09\x63\x49\x12\x34\x06\xC9\x96\x56\x78\x01"
				// Pattern 0
				"\x00\x49\x0F"
				"\x40\x05"
				"\x00\x42\x0F"
				"\x40\x3B"
				"\x60"
				// Pattern 1
				"\x04\x32\x0F"
				"\x40\x05"
				"\x04\x22\x0F"
				"\x40\x3B"
				"\x60"
				// PCM inst 1 sample data
				"\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0\x00\x10\x00\xF0"
			);
		}
};

IMPLEMENT_TESTS(cdfm_zone66);
