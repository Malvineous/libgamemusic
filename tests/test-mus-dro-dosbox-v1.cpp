/**
 * @file   test-mus-dro-dosbox-v1.cpp
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

class test_dro_dosbox_v1: public test_music
{
	public:
		test_dro_dosbox_v1()
		{
			this->type = "dro-dosbox-v1";
			this->numInstruments = 6;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			ADD_MUSIC_TEST(&test_dro_dosbox_v1::test_delay_combining);
			ADD_MUSIC_TEST(&test_dro_dosbox_v1::test_inst_read);
			ADD_MUSIC_TEST(&test_dro_dosbox_v1::test_perc_dupe);

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"DBRAWOPP" "\x00\x00\x01\x00"
			));

			// c02: Wrong version
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"DBRAWOPL" "\x01\x00\x00\x00"
			));

			// c03: Too short
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"DB"
			));

			// c04: Short but valid file
			this->isInstance(MusicType::DefinitelyYes, STRING_WITH_NULLS(
				"DBRAWOPL" "\x00\x00\x01\x00"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"DBRAWOPL" "\x00\x00\x01\x00"
				"\x80\x00\x00\x00" "\x9a\x00\x00\x00" "\x00\x00\x00\x00"
				"\x00\x07" // initial delay
				"\x20\xae\x40\x7f\x60\xed\x80\xcb\xe0\x06"
				"\x23\xa7\x43\x1f\x63\x65\x83\x43\xe3\x02\xc0\x04"
				"\xa0\x44"
				"\xb0\x32" "\x00\x0f"
				"\xb0\x12" "\x00\x03"
				// Rhythm hi-hat
				"\x31\xae"
				"\x51\x7f"
				"\x71\xdd"
				"\x91\xcb"
				"\xf1\x06"
				"\xa7\x44"
				"\xb7\x12"
				"\xbd\x21" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm top-cymbal
				"\x35\xae"
				"\x55\x7f"
				"\x75\xcd"
				"\x95\xcb"
				"\xf5\x06"
				"\xa8\x44"
				"\xb8\x12"
				"\xbd\x22" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm tom-tom
				"\x32\xae"
				"\x52\x7f"
				"\x72\xbd"
				"\x92\xcb"
				"\xf2\x06"
				"\xa8\x45"
				"\xb8\x13"
				"\xbd\x24" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm snare
				"\x34\xae"
				"\x54\x7f"
				"\x74\xad"
				"\x94\xcb"
				"\xf4\x06"
				"\xa7\x45"
				"\xb7\x13"
				"\xbd\x28" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm bass-drum
				"\x30\xae"
				"\x50\x7f"
				"\x70\x9d"
				"\x90\xcb"
				"\xf0\x06"
				"\x33\xae"
				"\x53\x7f"
				"\x73\x8d"
				"\x93\xcb"
				"\xf3\x06"
				"\xa6\x44"
				"\xb6\x12"
				"\xbd\x30" "\x00\x0f"
				"\xbd\x20" "\x00\x03" // trailing delay
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
			CHECK_OPL_PATCH(0, feedback, 0x2);
			CHECK_OPL_PATCH(0, m.attackRate, 0xE);
			CHECK_OPL_PATCH(0, c.attackRate, 0x6);
			CHECK_OPL_PATCH(0, rhythm, 0);
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
				"DBRAWOPL" "\x00\x00\x01\x00"
				"\x37\x03\x01\x00" "\x2f\x00\x00\x00" "\x00\x00\x00\x00"
				"\x20\xae\x40\x7f\x60\xed\x80\xcb\xe0\x06"
				"\x23\xa7\x43\x1f\x63\x65\x83\x43\xe3\x02\xc0\x04"
				"\xa0\x44\xb0\x32"
				"\x00\x0f" "\x00\x0f"
				"\xb0\x12"
				"\x00\x0f" "\x01\x0f\x02"
				"\xb0\x32"
				"\x01\xf0\x80\x01\x05\x80"
				"\xb0\x12"
			);

			// Read the above file
			MusicPtr music(this->pType->read(this->base, this->suppData));
			// Write it out again
			this->base.reset(new stream::string());
			this->pType->write(this->base, this->suppData, music, this->writeFlags);

			// Make sure it matches what we read
			std::string target = STRING_WITH_NULLS(
				"DBRAWOPL" "\x00\x00\x01\x00"
				"\x37\x03\x01\x00" "\x2a\x00\x00\x00" "\x00\x00\x00\x00"
				"\x20\xae\x40\x7f\x60\xed\x80\xcb\xe0\x06"
				"\x23\xa7\x43\x1f\x63\x65\x83\x43\xe3\x02\xc0\x04"
				"\xa0\x44\xb0\x32"
				"\x00\x1f"
				"\xb0\x12"
				"\x01\x1f\x02"
				"\xb0\x32"
				"\x01\xff\xff\x00\xf6"
				"\xb0\x12"
			);
			BOOST_REQUIRE(this->is_content_equal(target));
		}

		/// Make sure the percussion patches are duplicated if they refer to
		/// different rhythm instruments.
		void test_perc_dupe()
		{
			this->base.reset(new stream::string());
			this->base << STRING_WITH_NULLS(
				"DBRAWOPL" "\x00\x00\x01\x00"
				"\x80\x00\x00\x00" "\x9a\x00\x00\x00" "\x00\x00\x00\x00"
				"\x00\x07" // initial delay
				"\x20\x11\x40\x11\x60\x11\x80\x11\xe0\x11"
				"\x23\x11\x43\x11\x63\x11\x83\x11\xe3\x11\xc0\x11"
				"\xa0\x44"
				"\xb0\x32" "\x00\x0f"
				"\xb0\x12" "\x00\x03"
				// Rhythm hi-hat
				"\x31\x11"
				"\x51\x11"
				"\x71\x11"
				"\x91\x11"
				"\xf1\x11"
				"\xa7\x11"
				"\xb7\x11"
				"\xbd\x21" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm top-cymbal
				"\x35\x11"
				"\x55\x11"
				"\x75\x11"
				"\x95\x11"
				"\xf5\x11"
				"\xa8\x11"
				"\xb8\x11"
				"\xbd\x22" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm tom-tom
				"\x32\x11"
				"\x52\x11"
				"\x72\x11"
				"\x92\x11"
				"\xf2\x11"
				"\xa8\x11"
				"\xb8\x11"
				"\xbd\x24" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm snare
				"\x34\x11"
				"\x54\x11"
				"\x74\x11"
				"\x94\x11"
				"\xf4\x11"
				"\xa7\x11"
				"\xb7\x11"
				"\xbd\x28" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				// Rhythm bass-drum
				"\x30\x11"
				"\x50\x11"
				"\x70\x11"
				"\x90\x11"
				"\xf0\x11"
				"\x33\x11"
				"\x53\x11"
				"\x73\x11"
				"\x93\x11"
				"\xf3\x11"
				"\xa6\x11"
				"\xb6\x11"
				"\xbd\x30" "\x00\x0f"
				"\xbd\x20" "\x00\x03" // trailing delay
			);
			MusicPtr music(this->pType->read(this->base, this->suppData));
			CHECK_OPL_PATCH(0, m.attackRate, 0x1);
			CHECK_OPL_PATCH(0, c.attackRate, 0x1);
			CHECK_OPL_PATCH(0, rhythm, OPLPatch::Melodic);
			// Rhythm hi-hat
			CHECK_OPL_PATCH(1, m.attackRate, 0x1);
			CHECK_OPL_PATCH(1, rhythm, OPLPatch::HiHat);
			// Rhythm top-cymbal
			CHECK_OPL_PATCH(2, c.attackRate, 0x1);
			CHECK_OPL_PATCH(2, rhythm, OPLPatch::TopCymbal);
			// Rhythm tom-tom
			CHECK_OPL_PATCH(3, m.attackRate, 0x1);
			CHECK_OPL_PATCH(3, rhythm, OPLPatch::TomTom);
			// Rhythm snare
			CHECK_OPL_PATCH(4, c.attackRate, 0x1);
			CHECK_OPL_PATCH(4, rhythm, OPLPatch::SnareDrum);
			// Rhythm bass-drum
			CHECK_OPL_PATCH(5, m.attackRate, 0x1);
			CHECK_OPL_PATCH(5, c.attackRate, 0x1);
			CHECK_OPL_PATCH(5, rhythm, OPLPatch::BassDrum);

			// Do exactly the same again but load all the instruments before playing
			// any notes.
			this->base.reset(new stream::string());
			this->base << STRING_WITH_NULLS(
				"DBRAWOPL" "\x00\x00\x01\x00"
				"\x80\x00\x00\x00" "\x9a\x00\x00\x00" "\x00\x00\x00\x00"
				"\x00\x07" // initial delay
				"\x20\x11\x40\x11\x60\x11\x80\x11\xe0\x11"
				"\x23\x11\x43\x11\x63\x11\x83\x11\xe3\x11\xc0\x11"
				// Rhythm hi-hat
				"\x31\x11"
				"\x51\x11"
				"\x71\x11"
				"\x91\x11"
				"\xf1\x11"
				"\xa7\x11"
				"\xb7\x11"
				// Rhythm top-cymbal
				"\x35\x11"
				"\x55\x11"
				"\x75\x11"
				"\x95\x11"
				"\xf5\x11"
				"\xa8\x11"
				"\xb8\x11"
				// Rhythm tom-tom
				"\x32\x11"
				"\x52\x11"
				"\x72\x11"
				"\x92\x11"
				"\xf2\x11"
				"\xa8\x11"
				"\xb8\x11"
				// Rhythm snare
				"\x34\x11"
				"\x54\x11"
				"\x74\x11"
				"\x94\x11"
				"\xf4\x11"
				"\xa7\x11"
				"\xb7\x11"
				// Rhythm bass-drum
				"\x30\x11"
				"\x50\x11"
				"\x70\x11"
				"\x90\x11"
				"\xf0\x11"
				"\x33\x11"
				"\x53\x11"
				"\x73\x11"
				"\x93\x11"
				"\xf3\x11"
				"\xa6\x11"
				"\xb6\x11"
				"\xa0\x44"
				"\xb0\x32" "\x00\x0f"
				"\xb0\x12" "\x00\x03"
				"\xbd\x21" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				"\xbd\x22" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				"\xbd\x24" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				"\xbd\x28" "\x00\x0f"
				"\xbd\x20" "\x00\x03"
				"\xbd\x30" "\x00\x0f"
				"\xbd\x20" "\x00\x03" // trailing delay
			);
			music = this->pType->read(this->base, this->suppData);
			CHECK_OPL_PATCH(0, m.attackRate, 0x1);
			CHECK_OPL_PATCH(0, c.attackRate, 0x1);
			CHECK_OPL_PATCH(0, rhythm, 0);
			// Rhythm hi-hat
			CHECK_OPL_PATCH(1, m.attackRate, 0x1);
			CHECK_OPL_PATCH(1, rhythm, 1);
			// Rhythm top-cymbal
			CHECK_OPL_PATCH(2, c.attackRate, 0x1);
			CHECK_OPL_PATCH(2, rhythm, 2);
			// Rhythm tom-tom
			CHECK_OPL_PATCH(3, m.attackRate, 0x1);
			CHECK_OPL_PATCH(3, rhythm, 3);
			// Rhythm snare
			CHECK_OPL_PATCH(4, c.attackRate, 0x1);
			CHECK_OPL_PATCH(4, rhythm, 4);
			// Rhythm bass-drum
			CHECK_OPL_PATCH(5, m.attackRate, 0x1);
			CHECK_OPL_PATCH(5, c.attackRate, 0x1);
			CHECK_OPL_PATCH(5, rhythm, 5);
		}
};

IMPLEMENT_TESTS(dro_dosbox_v1);
