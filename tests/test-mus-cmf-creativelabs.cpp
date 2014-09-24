/**
 * @file   test-mus-cmf-creativelabs.cpp
 * @brief  Test code for Creative Labs' CMF files.
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

class test_cmf_creativelabs: public test_music
{
	public:
		test_cmf_creativelabs()
		{
			this->type = "cmf-creativelabs";
			this->numInstruments = 6;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			ADD_MUSIC_TEST(&test_cmf_creativelabs::test_op_swap);
			ADD_MUSIC_TEST(&test_cmf_creativelabs::test_ignore_default_tremolo);
			ADD_MUSIC_TEST(&test_cmf_creativelabs::test_inst_duped);

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"CTMM\x01\x01"
				"\x28\x00"
				"\x88\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"

				"\x05" "\xc0\x00"     // leading delay
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x67\x01" // rhythm on
				// Rhythm hi-hat
				"\x00" "\xcf\x01"
				"\x00" "\x9f\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm cymbal
				"\x00" "\xce\x02"
				"\x00" "\x9e\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm tom
				"\x00" "\xcd\x03"
				"\x00" "\x9d\x45\x7f"
				"\x10" "\x45\x00"
				// Disable tremolo depth
				"\x00" "\xb0\x63\x01"
				// Rhythm snare
				"\x00" "\xcc\x04"
				"\x00" "\x9c\x45\x7f"
				"\x10" "\x45\x00"
				// Disable vibrato depth too
				"\x00" "\xb0\x63\x00"
				// Rhythm bass drum
				"\x00" "\xcb\x05"
				"\x00" "\x9b\x45\x7f"
				"\x10" "\x45\x00"

				"\x20" "\xff\x2f\x00" // trailing delay
			));

			// c02: Wrong version
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"CTMF\x01\x02"
				"\x28\x00"
				"\x88\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"

				"\x05" "\xc0\x00"     // leading delay
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x67\x01" // rhythm on
				// Rhythm hi-hat
				"\x00" "\xcf\x01"
				"\x00" "\x9f\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm cymbal
				"\x00" "\xce\x02"
				"\x00" "\x9e\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm tom
				"\x00" "\xcd\x03"
				"\x00" "\x9d\x45\x7f"
				"\x10" "\x45\x00"
				// Disable tremolo depth
				"\x00" "\xb0\x63\x01"
				// Rhythm snare
				"\x00" "\xcc\x04"
				"\x00" "\x9c\x45\x7f"
				"\x10" "\x45\x00"
				// Disable vibrato depth too
				"\x00" "\xb0\x63\x00"
				// Rhythm bass drum
				"\x00" "\xcb\x05"
				"\x00" "\x9b\x45\x7f"
				"\x10" "\x45\x00"

				"\x20" "\xff\x2f\x00" // trailing delay
			));

			// c03: Old version (valid)
			this->isInstance(MusicType::DefinitelyYes, STRING_WITH_NULLS(
				"CTMF\x00\x01"
				"\x28\x00"
				"\x88\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06"

				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"

				"\x05" "\xc0\x00"     // leading delay
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x67\x01" // rhythm on
				// Rhythm hi-hat
				"\x00" "\xcf\x01"
				"\x00" "\x9f\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm cymbal
				"\x00" "\xce\x02"
				"\x00" "\x9e\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm tom
				"\x00" "\xcd\x03"
				"\x00" "\x9d\x45\x7f"
				"\x10" "\x45\x00"
				// Disable tremolo depth
				"\x00" "\xb0\x63\x01"
				// Rhythm snare
				"\x00" "\xcc\x04"
				"\x00" "\x9c\x45\x7f"
				"\x10" "\x45\x00"
				// Disable vibrato depth too
				"\x00" "\xb0\x63\x00"
				// Rhythm bass drum
				"\x00" "\xcb\x05"
				"\x00" "\x9b\x45\x7f"
				"\x10" "\x45\x00"

				"\x20" "\xff\x2f\x00" // trailing delay
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x88\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"

				"\x05" "\xc0\x00"     // leading delay
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x67\x01" // rhythm on
				// Rhythm hi-hat
				"\x00" "\xcf\x01"
				"\x00" "\x9f\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm cymbal
				"\x00" "\xce\x02"
				"\x00" "\x9e\x45\x7f"
				"\x10" "\x45\x00"
				// Rhythm tom
				"\x00" "\xcd\x03"
				"\x00" "\x9d\x45\x7f"
				"\x10" "\x45\x00"
				// Disable tremolo depth
				"\x00" "\xb0\x63\x01"
				// Rhythm snare
				"\x00" "\xcc\x04"
				"\x00" "\x9c\x45\x7f"
				"\x10" "\x45\x00"
				// Disable vibrato depth too
				"\x00" "\xb0\x63\x00"
				// Rhythm bass drum
				"\x00" "\xcb\x05"
				"\x00" "\x9b\x45\x7f"
				"\x10" "\x45\x00"

				"\x20" "\xff\x2f\x00" // trailing delay
			);
		}

		/// Make sure the operators are swapped for rhythm instruments
		void test_op_swap()
		{
			MusicPtr music(this->pType->read(this->base, this->suppData));
			OPLPatchPtr a = boost::dynamic_pointer_cast<OPLPatch>(
				music->patches->at(1)
			);
			OPLPatchPtr b = boost::dynamic_pointer_cast<OPLPatch>(
				music->patches->at(2)
			);
			BOOST_REQUIRE(a);
			BOOST_REQUIRE(b);
			BOOST_CHECK_EQUAL(a->m.attackRate, 0x0e);
			BOOST_CHECK_EQUAL(a->c.attackRate, 0x06);
			BOOST_CHECK_EQUAL(b->m.attackRate, 0x06);
			BOOST_CHECK_EQUAL(b->c.attackRate, 0x0e);
		}

		/// Make sure setting the tremolo/vibrato to the same values is ignored
		void test_ignore_default_tremolo()
		{
			this->base.reset(new stream::string());
			this->base << STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\x00" "\xb0\x63\x03" // set to default value, should be ignored
				"\x00" "\xc0\x00"
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x63\x01" // set to new value
				"\x00" "\xc0\x00"     // set same instrument, should be ignored
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x63\x01" // set to same value, should be ignored
				"\x00" "\x90\x45\x7f" // should be converted into running status
				"\x10" "\x45\x00"
				"\x00" "\xff\x2f\x00"
			);

			// Read the above file
			MusicPtr music(this->pType->read(this->base, this->suppData));
			// Write it out again
			this->base.reset(new stream::string());
			this->pType->write(this->base, this->suppData, music, this->writeFlags);

			// Make sure it matches what we read
			std::string target = STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				//"\x00" "\xb0\x63\x03" // set to default value, should be ignored
				"\x00" "\xc0\x00"
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x63\x01" // set to new value
				//"\x00" "\xc0\x00"     // set same instrument, should be ignored
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				//"\x00" "\xb0\x63\x01" // set to same value, should be ignored
				"\x00" "\x45\x7f" // converted into running status
				"\x10" "\x45\x00"
				"\x00" "\xff\x2f\x00"
			);
			BOOST_REQUIRE(this->is_content_equal(target));
		}

		/// Make sure setting the tremolo/vibrato to the same values is ignored
		void test_inst_duped()
		{
			this->base.reset(new stream::string());
			this->base << STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x00\x00"
				"\xae\xa7\x7f\x1f\xed\x65\xcb\x43\x06\x02\x04" "\x00\x00\x00\x00\x00"
				"\x00" "\xb0\x63\x03" // set to default value, should be ignored
				"\x00" "\xc0\x00"
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xb0\x63\x01" // set to new value
				"\x00" "\xcf\x00"     // set same instrument, should be duplicated
				"\x00" "\x9f\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xce\x00"     // set same instrument, should be duplicated
				"\x00" "\x9e\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xff\x2f\x00"
			);

			// Read the above file
			MusicPtr music(this->pType->read(this->base, this->suppData));

			BOOST_REQUIRE_EQUAL(music->patches->size(), 3);
			OPLPatchPtr a = boost::dynamic_pointer_cast<OPLPatch>(
				music->patches->at(0)
			);
			OPLPatchPtr b = boost::dynamic_pointer_cast<OPLPatch>(
				music->patches->at(1)
			);
			OPLPatchPtr c = boost::dynamic_pointer_cast<OPLPatch>(
				music->patches->at(2)
			);
			BOOST_REQUIRE(a);
			BOOST_REQUIRE(b);
			BOOST_REQUIRE(c);
			BOOST_CHECK_EQUAL(a->rhythm, 0);
			BOOST_CHECK_EQUAL(b->rhythm, 1);
			BOOST_CHECK_EQUAL(c->rhythm, 2);
			BOOST_CHECK_EQUAL(a->m.attackRate, 0x0e);
			BOOST_CHECK_EQUAL(a->c.attackRate, 0x06);
			BOOST_CHECK_EQUAL(b->m.attackRate, 0x0e);
			BOOST_CHECK_EQUAL(b->c.attackRate, 0x06);
			BOOST_CHECK_EQUAL(c->m.attackRate, 0x06);
			BOOST_CHECK_EQUAL(c->c.attackRate, 0x0e);
		}
};

IMPLEMENT_TESTS(cmf_creativelabs);
