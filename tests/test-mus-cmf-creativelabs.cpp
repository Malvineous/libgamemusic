/**
 * @file   test-mus-cmf-creativelabs.cpp
 * @brief  Test code for Creative Labs' CMF files.
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

			{
				this->attributes.emplace_back();
				auto& a = this->attributes.back();
				a.type = Attribute::Type::Text;
				a.name = CAMOTO_ATTRIBUTE_TITLE;
				a.textValue = "Test title";
				a.textMaxLength = 32767;
			}
			{
				this->attributes.emplace_back();
				auto& a = this->attributes.back();
				a.type = Attribute::Type::Text;
				a.name = CAMOTO_ATTRIBUTE_AUTHOR;
				a.textValue = "Test author";
				a.textMaxLength = 32767;
			}
			{
				this->attributes.emplace_back();
				auto& a = this->attributes.back();
				a.type = Attribute::Type::Text;
				a.name = CAMOTO_ATTRIBUTE_COMMENT;
				a.textValue = "Test description";
				a.textMaxLength = 32767;
			}
		}

		void addTests()
		{
			this->test_music::addTests();

			ADD_MUSIC_TEST(&test_cmf_creativelabs::test_op_swap);
			ADD_MUSIC_TEST(&test_cmf_creativelabs::test_inst_duped);
			ADD_MUSIC_TEST(&test_cmf_creativelabs::test_default_instrument);

			// c00: Normal
			this->isInstance(MusicType::Certainty::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"CTMM\x01\x01"
				"\x28\x00"
				"\x88\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"

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
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"CTMF\x01\x02"
				"\x28\x00"
				"\x88\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\x7E\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"

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
			this->isInstance(MusicType::Certainty::DefinitelyYes, STRING_WITH_NULLS(
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

			// c04: Too short
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"CTMF\x00\x01"
			));

			// a01: Change title
			this->changeAttribute(0, "Replaced", STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x4E\x00"
				"\xAE\x00"
				"\x40\x00\xCD\x00"
				"\x28\x00" "\x31\x00" "\x3D\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\xC0\x00"

				"Replaced\0"
				"Test author\0"
				"Test description\0"

				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"

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

			// a02: Change composer
			this->changeAttribute(1, "Replaced", STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x4D\x00"
				"\xAD\x00"
				"\x40\x00\xCD\x00"
				"\x28\x00" "\x33\x00" "\x3C\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\xC0\x00"

				"Test title\0"
				"Replaced\0"
				"Test description\0"

				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"

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

			// a03: Change description
			this->changeAttribute(2, "Replaced", STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x48\x00"
				"\xA8\x00"
				"\x40\x00\xCD\x00"
				"\x28\x00" "\x33\x00" "\x3F\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\xC0\x00"

				"Test title\0"
				"Test author\0"
				"Replaced\0"

				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"

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

			// Make sure setting the tremolo/vibrato to the same values is ignored
			this->rewrite(STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x39\x01"
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
			), STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x39\x01"
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
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x50\x00"
				"\xB0\x00"
				"\x40\x00\xCD\x00"
				"\x28\x00" "\x33\x00" "\x3F\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01"
				"\x06\x00"
				"\xC0\x00"

				"Test title\0"
				"Test author\0"
				"Test description\0"

				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				// Duplicated instruments, until the normalisation problems are solved
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"

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
			auto music = this->pType->read(this->base, this->suppData);

			BOOST_REQUIRE_GE(music->patches->size(), 3);
			auto mel = dynamic_cast<OPLPatch*>(music->patches->at(0).get());
			auto hat = dynamic_cast<OPLPatch*>(music->patches->at(1).get());
			auto cym = dynamic_cast<OPLPatch*>(music->patches->at(2).get());
			BOOST_REQUIRE(mel);
			BOOST_REQUIRE(hat);
			BOOST_REQUIRE(cym);
			BOOST_CHECK_EQUAL(mel->rhythm, OPLPatch::Rhythm::Melodic);
			BOOST_CHECK_EQUAL(hat->rhythm, OPLPatch::Rhythm::HiHat);
			BOOST_CHECK_EQUAL(cym->rhythm, OPLPatch::Rhythm::TopCymbal);
			BOOST_CHECK_EQUAL(mel->m.attackRate, 15);
			BOOST_CHECK_EQUAL(mel->c.attackRate, 14);
			BOOST_CHECK_EQUAL(hat->m.attackRate, 15);
			BOOST_CHECK_EQUAL(hat->c.attackRate, 14);
			BOOST_CHECK_EQUAL(cym->m.attackRate, 14); // cym is swapped
			BOOST_CHECK_EQUAL(cym->c.attackRate, 15);
		}

		/// Make sure setting the tremolo/vibrato to the same values is ignored
		void test_inst_duped()
		{
			this->base.seekp(0, stream::start);
			this->base.data.clear();

			this->base << STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x39\x01"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
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
			auto music = this->pType->read(this->base, this->suppData);

			BOOST_REQUIRE_EQUAL(music->patches->size(), 3);
			auto mel = dynamic_cast<OPLPatch*>(music->patches->at(0).get());

			// You would expect the hihat to be instrument #1 and the cymbal to be #2,
			// but because the denormalisation process (where instruments are
			// duplicated as needed) works channel by channel, the order is changed.
			// So the top cymbal is instrument #1 because channel 14 comes before
			// channel 15.  When we get to channel 15, the hi-hat is assigned
			// instrument #2.
			auto hat = dynamic_cast<OPLPatch*>(music->patches->at(2).get());
			auto cym = dynamic_cast<OPLPatch*>(music->patches->at(1).get());

			BOOST_REQUIRE(mel);
			BOOST_REQUIRE(hat);
			BOOST_REQUIRE(cym);
			BOOST_CHECK_EQUAL(mel->rhythm, OPLPatch::Rhythm::Melodic);
			BOOST_CHECK_EQUAL(hat->rhythm, OPLPatch::Rhythm::HiHat);
			BOOST_CHECK_EQUAL(cym->rhythm, OPLPatch::Rhythm::TopCymbal);
			BOOST_CHECK_EQUAL(mel->m.attackRate, 15);
			BOOST_CHECK_EQUAL(mel->c.attackRate, 14);
			BOOST_CHECK_EQUAL(hat->m.attackRate, 15);
			BOOST_CHECK_EQUAL(hat->c.attackRate, 14);
			BOOST_CHECK_EQUAL(cym->m.attackRate, 14); // cym is swapped
			BOOST_CHECK_EQUAL(cym->c.attackRate, 15);
		}

		/// Make sure the default instruments are used
		void test_default_instrument()
		{
			this->base.seekp(0, stream::start);
			this->base.data.clear();

			this->base << STRING_WITH_NULLS(
				"CTMF\x01\x01"
				"\x28\x00"
				"\x38\x00"
				"\xc0\x00\xe8\x03"
				"\x00\x00\x00\x00\x00\x00"
				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x01\x00"
				"\x39\x01"
				"\xFF\x0E\xFF\xBE\xFF\xEE\xFF\xEE\x07\x06\x0F" "\x00\x00\x00\x00\x00"
				"\x00" "\xc0\x01" // instrument past end of bank
				"\x00" "\x90\x45\x7f"
				"\x10" "\x45\x00"
				"\x00" "\xff\x2f\x00"
			);

			// Read the above file
			auto music = this->pType->read(this->base, this->suppData);

			// Ensure patches + default are loaded
			BOOST_REQUIRE_EQUAL(music->patches->size(), 2);
			auto def = dynamic_cast<OPLPatch*>(music->patches->at(1).get());

			BOOST_REQUIRE(def);
			BOOST_CHECK_EQUAL(def->rhythm, OPLPatch::Rhythm::Melodic);
			BOOST_CHECK_EQUAL(def->feedback, 4);
			BOOST_CHECK_EQUAL(def->connection, false);

			BOOST_CHECK_EQUAL(def->c.enableTremolo, false);
			BOOST_CHECK_EQUAL(def->c.enableVibrato, false);
			BOOST_CHECK_EQUAL(def->c.enableSustain, false);
			BOOST_CHECK_EQUAL(def->c.enableKSR, true);
			BOOST_CHECK_EQUAL(def->c.freqMult, 2);
			BOOST_CHECK_EQUAL(def->c.scaleLevel, 0);
			BOOST_CHECK_EQUAL(def->c.outputLevel, 0);
			BOOST_CHECK_EQUAL(def->c.attackRate, 15);
			BOOST_CHECK_EQUAL(def->c.decayRate, 2);
			BOOST_CHECK_EQUAL(def->c.sustainRate, 7);
			BOOST_CHECK_EQUAL(def->c.releaseRate, 2);
			BOOST_CHECK_EQUAL(def->c.waveSelect, 0);

			BOOST_CHECK_EQUAL(def->m.enableTremolo, false);
			BOOST_CHECK_EQUAL(def->m.enableVibrato, false);
			BOOST_CHECK_EQUAL(def->m.enableSustain, false);
			BOOST_CHECK_EQUAL(def->m.enableKSR, false);
			BOOST_CHECK_EQUAL(def->m.freqMult, 7);
			BOOST_CHECK_EQUAL(def->m.scaleLevel, 1);
			BOOST_CHECK_EQUAL(def->m.outputLevel, 15);
			BOOST_CHECK_EQUAL(def->m.attackRate, 15);
			BOOST_CHECK_EQUAL(def->m.decayRate, 2);
			BOOST_CHECK_EQUAL(def->m.sustainRate, 6);
			BOOST_CHECK_EQUAL(def->m.releaseRate, 0);
			BOOST_CHECK_EQUAL(def->m.waveSelect, 0);
		}
};

IMPLEMENT_TESTS(cmf_creativelabs);
