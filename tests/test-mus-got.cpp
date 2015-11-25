/**
 * @file   test-mus-got.cpp
 * @brief  Test code for God of Thunder files.
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

class test_mus_got: public test_music
{
	public:
		test_mus_got()
		{
			this->type = "got";
			this->numInstruments = 1;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::Certainty::PossiblyYes, this->standard());

			// c01: Too short
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x01"
				"\x00\x00\x00" "\x00"
			));

			// c02: Uneven length
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x01\x00"
				"\x00\x20\xAE"
				"\x00"
				"\x00\x00\x00" "\x00"
			));

			// c03: Bad signature
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x02\x00"
				"\x00\x20\xAE"
				"\x00\x40\x7F"
				"\x00\x60\xED"
				"\x00\x80\xCB"
				"\x00\xE0\x06"
				"\x00\x23\xA7"
				"\x00\x43\x1F"
				"\x00\x63\x65"
				"\x00\x83\x43"
				"\x00\xE3\x02"
				"\x00\xC0\x04"

				"\x00\xA0\x44"
				"\x01\xB0\x32"
				"\x00\xB0\x12"

				"\x00\x00\x00" "\x00"
			));

			// c04: Missing/incomplete loop-to-start marker
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x01\x00"
				"\x00\x20\xAE"
				"\x00\x40\x7F"
				"\x00\x60\xED"
				"\x00\x80\xCB"
				"\x00\xE0\x06"
				"\x00\x23\xA7"
				"\x00\x43\x1F"
				"\x00\x63\x65"
				"\x00\x83\x43"
				"\x00\xE3\x02"
				"\x00\xC0\x04"

				"\x00\xA0\x44"
				"\x01\xB0\x32"
				"\x00\xB0\x12"

				"\x00\x00\x00"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"\x01\x00"
				"\x00\x20\xFF"
				"\x00\x40\xFF"
				"\x00\x60\xFF"
				"\x00\x80\xFF"
				"\x00\xE0\x07"
				"\x00\x23\x0E"
				"\x00\x43\xBE"
				"\x00\x63\xEE"
				"\x00\x83\xEE"
				"\x00\xE3\x06"
				"\x00\xC0\x0F"

				"\x00\xA0\x44"
				"\x01\xB0\x32"
				"\x00\xB0\x12"

				"\x00\x00\x00" "\x00"
			);
		}

};

IMPLEMENT_TESTS(mus_got);
