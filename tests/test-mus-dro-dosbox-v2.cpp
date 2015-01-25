/**
 * @file   test-mus-dro-dosbox-v2.cpp
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

class test_dro_dosbox_v2: public test_music
{
	public:
		test_dro_dosbox_v2()
		{
			this->type = "dro-dosbox-v2";
			this->numInstruments = 6;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			ADD_MUSIC_TEST(&test_dro_dosbox_v2::test_delay_combining);
			ADD_MUSIC_TEST(&test_dro_dosbox_v2::test_inst_read);

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"DBRAWOPP" "\x02\x00\x00\x00"
			));

			// c02: Wrong version
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"DBRAWOPL" "\x00\x00\x01\x00"
			));

			// c03: Too short
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"DB"
			));

			// c04: Short but valid file
			this->isInstance(MusicType::DefinitelyYes, STRING_WITH_NULLS(
				"DBRAWOPL" "\x02\x00\x00\x00"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"DBRAWOPL" "\x02\x00\x00\x00"
				"\x4d\x00\x00\x00" "\x80\x00\x00\x00"
				"\x00\x00\x00"
				"\xff\xfe\x32"
				"\x20\x40\x60\x80\xe0"
				"\x23\x43\x63\x83\xe3\xc0"
				"\xa0\xb0"
				// Rhythm hi-hat
				"\x31\x51\x71\x91\xf1\xa7\xb7\xbd"
				// Rhythm top-cymbal
				"\x35\x55\x75\x95\xf5\xa8\xb8"
				// Rhythm tom-tom
				"\x32\x52\x72\x92\xf2"
				// Rhythm snare
				"\x34\x54\x74\x94\xf4"
				// Rhythm bass-drum
				"\x30\x50\x70\x90\xf0"
				"\x33\x53\x73\x93\xf3"
				"\xa6\xb6"
				// Note data
				"\xff\x07" // leading delay
				"\x00\xff\x01\xff\x02\xff\x03\xff\x04\x07"
				"\x05\x0e\x06\xbe\x07\xee\x08\xee\x09\x06\x0a\x0f"
				"\x0b\x44"
				"\x0c\x32" "\xff\x0f"
				"\x0c\x12" "\xff\x03"
				// Rhythm hi-hat
				"\x0d\xae"
				"\x0e\x7f"
				"\x0f\xdd"
				"\x10\xcb"
				"\x11\x06"
				"\x12\x44"
				"\x13\x12"
				"\x14\x21" "\xff\x0f"
				"\x14\x20" "\xff\x03"
				// Rhythm top-cymbal
				"\x15\xae"
				"\x16\x7f"
				"\x17\xcd"
				"\x18\xcb"
				"\x19\x06"
				"\x1a\x44"
				"\x1b\x12"
				"\x14\x22" "\xff\x0f"
				"\x14\x20" "\xff\x03"
				// Rhythm tom-tom
				"\x1c\xae"
				"\x1d\x7f"
				"\x1e\xbd"
				"\x1f\xcb"
				"\x20\x06"
				"\x1a\x45"
				"\x1b\x13"
				"\x14\x24" "\xff\x0f"
				"\x14\x20" "\xff\x03"
				// Rhythm snare
				"\x21\xae"
				"\x22\x7f"
				"\x23\xad"
				"\x24\xcb"
				"\x25\x06"
				"\x12\x45"
				"\x13\x13"
				"\x14\x28" "\xff\x0f"
				"\x14\x20" "\xff\x03"
				// Rhythm bass-drum
				"\x26\xae"
				"\x27\x7f"
				"\x28\x9d"
				"\x29\xcb"
				"\x2a\x06"
				"\x2b\xae"
				"\x2c\x7f"
				"\x2d\x8d"
				"\x2e\xcb"
				"\x2f\x06"
				"\x30\x44"
				"\x31\x12"
				"\x14\x30" "\xff\x0f"
				"\x14\x20" "\xff\x03" // trailing delay
			);
		}

#define CHECK_OPL_PATCH(p, field, value) \
	{ \
		OPLPatch *patch = dynamic_cast<OPLPatch *>(music->patches->at(p).get()); \
		assert(patch); \
		BOOST_CHECK_EQUAL(patch->field, value); \
	}

		/// Make sure OPL decoder reads the instruments properly
		void test_inst_read()
		{
			// Read the standard file
			MusicPtr music(this->pType->read(this->base, this->suppData));
			// Melodic instrument is handled in default test
			// Rhythm hi-hat
			CHECK_OPL_PATCH(1, m.scaleLevel, 0x1);
			CHECK_OPL_PATCH(1, m.attackRate, 0xD);
			CHECK_OPL_PATCH(1, rhythm, 1);
			// Rhythm top-cymbal
			CHECK_OPL_PATCH(2, c.scaleLevel, 0x1);
			CHECK_OPL_PATCH(2, c.attackRate, 0xC);
			CHECK_OPL_PATCH(2, rhythm, 2);
			// Rhythm tom-tom
			CHECK_OPL_PATCH(3, m.attackRate, 0xB);
			CHECK_OPL_PATCH(3, rhythm, 3);
			// Rhythm snare
			CHECK_OPL_PATCH(4, c.attackRate, 0xA);
			CHECK_OPL_PATCH(4, rhythm, 4);
			// Rhythm bass-drum
			CHECK_OPL_PATCH(5, m.attackRate, 0x9);
			CHECK_OPL_PATCH(5, c.attackRate, 0x8);
			CHECK_OPL_PATCH(5, rhythm, 5);
		}

		/// Make sure delays are combined correctly
		void test_delay_combining()
		{
			this->base.reset(new stream::string());
			this->base << STRING_WITH_NULLS(
				"DBRAWOPL" "\x02\x00\x00\x00"
				"\x19\x00\x00\x00" "\x42\x06\x01\x00"
				"\x00\x00\x00"
				"\xff\xfe\x0d"
				"\x20\x40\x60\x80\xe0"
				"\x23\x43\x63\x83\xe3\xc0"
				"\xa0\xb0"
				"\x00\xae\x01\x7f\x02\xed\x03\xcb\x04\x06"
				"\x05\xa7\x06\x1f\x07\x65\x08\x43\x09\x02\x0a\x04"
				"\x0b\x44\x0c\x32"
				"\xff\x0f" "\xff\x0f"
				"\x0c\x12"
				"\xff\x0f" "\xfe\x02" "\xff\x0f"
				"\x0c\x32"
				"\xfe\x80" "\xfe\x81" "\xff\x01"
				"\x0c\x12"
			);

			// Read the above file
			MusicPtr music(this->pType->read(this->base, this->suppData));
			// Write it out again
			this->base.reset(new stream::string());
			this->pType->write(this->base, this->suppData, music, this->writeFlags);

			// Make sure it matches what we read
			std::string target = STRING_WITH_NULLS(
				"DBRAWOPL" "\x02\x00\x00\x00"
				"\x16\x00\x00\x00" "\x42\x06\x01\x00"
				"\x00\x00\x00"
				"\xff\xfe\x0d"
				"\x20\x40\x60\x80\xe0"
				"\x23\x43\x63\x83\xe3\xc0"
				"\xa0\xb0"
				"\x00\xae\x01\x7f\x02\xed\x03\xcb\x04\x06"
				"\x05\xa7\x06\x1f\x07\x65\x08\x43\x09\x02\x0a\x04"
				"\x0b\x44\x0c\x32"
				"\xff\x1f"
				"\x0c\x12"
				"\xfe\x02" "\xff\x1f"
				"\x0c\x32"
				"\xfe\xff" "\xfe\x02" "\xff\x01"
				"\x0c\x12"
			);
			BOOST_REQUIRE(this->is_content_equal(target));
		}
};

IMPLEMENT_TESTS(dro_dosbox_v2);
