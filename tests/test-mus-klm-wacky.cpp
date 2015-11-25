/**
 * @file   test-mus-klm-wacky.cpp
 * @brief  Test code for Wacky Wheels KLM files.
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

class test_klm_wacky: public test_music
{
	public:
		test_klm_wacky()
		{
			this->type = "klm-wacky";
			this->numInstruments = 1;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::Certainty::DefinitelyYes, this->standard());

			// c01: Instrument block length is wrong multiple
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x0F\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
				"\x30\x00"
				"\x10\x44\x32"
				"\xfd\x10"
				"\x00"
				"\xff"
			));

			// c02: Music offset past EOF
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\xF0\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
				"\x30\x00"
				"\x10\x44\x32"
				"\xfd\x10"
				"\x00"
				"\xff"
			));

			// c03: Invalid 0xF0 event type
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
				"\x30\x00"
				"\x10\x44\x32"
				"\xFA\x0f"
				"\x00"
				"\xff"
			));

			// c04: Invalid normal event type
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
				"\x30\x00"
				"\x10\x44\x32"
				"\x55\x0f"
				"\x00"
				"\xff"
			));

			// c05: Truncated event
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x01\x0A"
				"\x30\x00"
				"\x10\x44"
			));

			// c06: Bad instrument in reg 0xE0
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x80\x01\x0A"
				"\x30\x00"
				"\x10\x44\x32"
				"\xfd\x10"
				"\x00"
				"\xff"
			));

			// c07: Bad instrument in 0xE3
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				"\x13\x00\x71\xF0\xAE\x8A\xE1\xE2\x00\x81\x0A"
				"\x30\x00"
				"\x10\x44\x32"
				"\xfd\x10"
				"\x00"
				"\xff"
			));

			// c08: All valid instrument bits enabled
			this->isInstance(MusicType::Certainty::DefinitelyYes, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				"\xff\xff\xff\xff\xff\xff\xff\xff\x07\x07\x3f"
				"\x30\x00"
				"\x10\x44\x32"
				"\xfd\x10"
				"\x00"
				"\xff"
			));

			// c09: Too short
			this->isInstance(MusicType::Certainty::DefinitelyNo, STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x0F"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"\x18\x01" "\x01" "\x10\x00"
				// Instruments
				"\xff\xbe\xff\xee\xff\xee\xff\x0e\x07\x06\x0f"
				// Events
				"\x30\x00"
				"\x10\x44\x32"
				"\xfd\x10"
				"\x00"
				"\xff"
			);
		}

};

IMPLEMENT_TESTS(klm_wacky);
