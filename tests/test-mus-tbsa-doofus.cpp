/**
 * @file   test-mus-tbsa-doofus.cpp
 * @brief  Test code for DOSBox raw OPL capture files.
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

class test_tbsa_doofus: public test_music
{
	public:
		test_tbsa_doofus()
		{
			this->type = "tbsa-doofus";
			this->numInstruments = 2;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
			this->outputWidth = 8;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"XBSA0.01"
				"\x14\x00" // order lists
				"\x18\x00" // unknown1
				"\x1A\x00" // unknown2
				"\x1C\x00" // unknown3
				"\x1E\x00" // instruments
				"\x3D\x00" // tracks

				// 0x0014: Order lists
				"\x22\x00" // order list 0
				"\xFF\xFF"

				// 0x0018: Unknown1
				"\xFF\xFF"

				// 0x001A: Unknown2
				"\xFF\xFF"

				// 0x001C: Unknown1
				"\xFF\xFF"

				// 0x001E: Instruments
				"\x26\x00" // instrument 0
				"\xFF\xFF"

				// 0x0022: Order List 0
				"\x01" "\x00"
				"\x3B\x00" // track list for first pattern (order 0)

				// 0x0026: Instrument 0
				"\xFF\xFF\x01\x01\x0F\x07\x01\x3D\x03\x01\x01\x07" "\xEE\xEE\x00\x00\x0E\x3C\x02\x06"

				// 0x003B: Tracks in orderlist 0, order 0
				"\x00\x01\xFE" // track 0 + 1

				// 0x003D: Track pointer list
				"\x43\x00" // track 0
				"\x4C\x00" // track 1
				"\xFF\xFF"

				// 0x0043: Track 0
				"\x80" // set inst 0
				"\xFD\x7F" // full volume
				"\x90" // set small increment to 0 (one row)
				"\x30" // note on
				"\xFE"
				"\xBD" // set large increment to 29 (62 rows)
				"\xFE" // cause 62 row jump
				"\xFF"

				// 0x004C: Track 1
				"\x80" // set inst 0
				"\xFD\x7F" // full volume
				"\x91" // set small increment to 1 (two row)
				"\x30" // note on
				"\xFE"
				"\xBB" // set large increment to 27 (60 rows)
				"\xFE" // cause 60 row jump
				"\xFF"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"TBSA0.01"
				"\x14\x00" // order lists
				"\x18\x00" // unknown1
				"\x1A\x00" // unknown2
				"\x1C\x00" // unknown3
				"\x1E\x00" // instruments
				"\x24\x00" // patsegs

				// 0x0014: Order lists
				"\x2E\x00" // order list 0
				"\xFF\xFF"

				// 0x0018: Unknown1
				"\xFF\xFF"

				// 0x001A: Unknown2
				"\xFF\xFF"

				// 0x001C: Unknown1
				"\xFF\xFF"

				// 0x001E: Instruments
				"\x34\x00" // instrument 0
				"\x48\x00" // instrument 1
				"\xFF\xFF"

				// 0x0024: Patseg pointer list
				"\x64\x00" // Offset of patseg #0
				"\x6D\x00" // Offset of patseg #1
				"\x78\x00" // Offset of patseg #2
				"\x81\x00" // Offset of patseg #3
				"\xFF\xFF"

				// 0x002E: Order List 0 (one entry per track)
				"\x02" "\x00"
				"\x5C\x00" // list of patsegs for track 0
				"\x60\x00" // list of patsegs for track 1

				// 0x0034: Instrument 0
				"\xFF\xFF\x01\x01\x0F\x07\x01\x3D\x03\x01\x01\x07" "\xEE\xEE\x00\x00\x0E\x3C\x02\x06"
				"\x55\x55\x00\x00\x05\x05\x00\x3F\x01\x00\x00\x05" "\xAA\xAA\x01\x01\x0A\x00\x00\x03"

				// 0x005C: Patsegs for track 0
				"\x00\x02\x00\xFE" // patseg #0

				// 0x0060: Patsegs for track 1
				"\x01\x03\x01\xFE" // patseg #1

				// 0x0064: Patseg #0
				"\x80" // set inst 0
				"\xFD\x7F" // full volume
				"\xA0" // set small increment to 0 (one row)
				"\x30" // note on
				"\xFE"
				"\xDD" // set large increment to 29 (62 rows)
				"\xFE" // cause 62 row jump
				"\xFF"

				// 0x006D: Patseg #1
				"\x80" // set inst 0
				"\xFD\x7F" // full volume
				"\xA1" // set small increment to 1 (two rows)
				"\x30" // note on
				"\xFE"
				"\x31"
				"\xFE"
				"\xD7" // set large increment to 23 (56 rows)
				"\xFE" // cause 56 row jump
				"\xFF"

				// 0x0078: Patseg #2
				"\x80" // set inst 0
				"\xFD\x7F" // full volume
				"\xA0" // set small increment to 0 (one row)
				"\x32" // note on
				"\xFE"
				"\xDD" // set large increment to 29 (62 rows)
				"\xFE" // cause 62 row jump
				"\xFF"

				// 0x0081: Patseg #3 (long leading delay)
				"\xC0" // set large increment to 0 (33 rows)
				"\xFE" // cause 32 row jump
				"\xBA" // set small increment to 1A (27 rows)
				"\xFE"
				"\x80" // set inst 0
				"\xFD\x7F" // full volume
				"\xA0" // set small increment to 0 (one row)
				"\x33" // note on
				"\xFE"
				"\x34"
				"\xFE"
				"\xFF"
			);
		}
};

IMPLEMENT_TESTS(tbsa_doofus);
