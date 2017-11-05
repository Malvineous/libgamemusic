/**
 * @file   test-mus-mid-type0.cpp
 * @brief  Test code for type-0 MIDI files.
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

class test_mid_type0: public test_music
{
	public:
		test_mid_type0()
		{
			this->type = "mid-type0";
			this->numInstruments = 1;
			this->indexInstrumentOPL = -1;
			this->indexInstrumentMIDI = 0;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			ADD_MUSIC_TEST(&test_mid_type0::test_empty_read);

			// c00: Normal
			this->isInstance(MusicType::Certainty::DefinitelyYes, this->standard());

			// c01: Wrong signature
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"MThf\x00\x00\x00\x06"
				"\x00\x00"
				"\x00\x01"
				"\x00\xc0"
				"MTrk\x00\x00\x00\x15"
				"\x00\xff\x51\x03\x07\xa1\x20"
				"\x00\xc0\x00"
				"\x00\x90\x45\x7f"
				"\x10\x45\x00"
				"\x00\xff\x2f\x00"
			));

			// c02: Wrong type
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"MThd\x00\x00\x00\x06"
				"\x00\x02"
				"\x00\x01"
				"\x00\xc0"
				"MTrk\x00\x00\x00\x15"
				"\x00\xff\x51\x03\x07\xa1\x20"
				"\x00\xc0\x00"
				"\x00\x90\x45\x7f"
				"\x10\x45\x00"
				"\x00\xff\x2f\x00"
			));

			// c03: File too short (header)
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"MThd\x00\x00\x00\x06"
				"\x00"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"MThd\x00\x00\x00\x06"
				"\x00\x00"
				"\x00\x01"
				"\x00\xc0"
				"MTrk\x00\x00\x00\x15"
				"\x00\xff\x51\x03\x07\xa1\x20"
				"\x00\xc0\x00"
				"\x00\x90\x45\x7f"
				"\x10\x45\x00"
				"\x00\xff\x2f\x00"
			);
		}

		/// A song with some events but no notes and zero duration
		std::string empty()
		{
			return STRING_WITH_NULLS(
				"MThd\x00\x00\x00\x06"
				"\x00\x00"
				"\x00\x01"
				"\x00\xc0"
				"MTrk\x00\x00\x00\x0b"
				"\x00\xff\x51\x03\x07\xa1\x20"
				"\x00\xff\x2f\x00"
			);
		}

		/// Make sure an empty file has no tracks
		void test_empty_read()
		{
			this->base.seekp(0, stream::start);
			this->base.data.clear();

			this->base << this->empty();

			// Read the standard file
			auto music = this->pType->read(this->base, this->suppData);

			BOOST_CHECK_EQUAL(music->trackInfo.size(), 0);
		}
};

IMPLEMENT_TESTS(mid_type0);
