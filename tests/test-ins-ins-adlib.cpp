/**
 * @file   test-ins-ins-adlib.cpp
 * @brief  Test code for Ad Lib INS instrument files.
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

class test_ins_adlib: public test_music
{
	public:
		test_ins_adlib()
		{
			this->type = "ins-adlib";
			this->numInstruments = 1;
			this->indexInstrumentOPL = 0;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = -1;
			this->hasMetadata[camoto::Metadata::Title] = true;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::PossiblyYes, this->standard());

			// c01: Unknown length
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x00\x00"
				// Operator 0
				"\x03\x00"
				"\x0F\x00"
				"\x07\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x01\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x3F\x00"
				"\x01\x00"
				"\x01\x00"
				"\x01\x00"
				//"\x01\x00"
			));

			// c02: Out of range value
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"\x00\x00"
				// Operator 0
				"\x03\x00"
				"\x0F\x00"
				"\x07\x00"
				"\x0F\x00"
				"\x0F\x01"
				"\x01\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x3F\x00"
				"\x01\x00"
				"\x01\x00"
				"\x01\x00"
				"\x01\x00"
				// Operator 1
				"\x02\x00"
				"\x0E\x00"
				"\x07\x00" // always overwritten with op0 value
				"\x0E\x00"
				"\x0E\x00"
				"\x00\x00"
				"\x0E\x00"
				"\x0E\x00"
				"\x3E\x00"
				"\x00\x00"
				"\x00\x00"
				"\x00\x00"
				"\x01\x00" // always overwritten with op0 value
				"Test title\0\0\0\0\0\0\0\0\0\0"
				// Wave select 0 and 1
				"\x07\x00"
				"\x06\x00"
				// Unknown padding
				"\x01\x00"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"\x00\x00"
				// Operator 0
				"\x03\x00"
				"\x0F\x00"
				"\x07\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x01\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x3F\x00"
				"\x01\x00"
				"\x01\x00"
				"\x01\x00"
				"\x00\x00"
				// Operator 1
				"\x02\x00"
				"\x0E\x00"
				"\x07\x00" // always overwritten with op0 value
				"\x0E\x00"
				"\x0E\x00"
				"\x00\x00"
				"\x0E\x00"
				"\x0E\x00"
				"\x3E\x00"
				"\x00\x00"
				"\x00\x00"
				"\x00\x00"
				"\x00\x00" // always overwritten with op0 value
				"Test title\0\0\0\0\0\0\0\0\0\0"
				// Wave select 0 and 1
				"\x07\x00"
				"\x06\x00"
				// Unknown padding
				"\x01\x00"
			);
		}

		virtual std::string metadata_title_replaced()
		{
			return STRING_WITH_NULLS(
				"\x00\x00"
				// Operator 0
				"\x03\x00"
				"\x0F\x00"
				"\x07\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x01\x00"
				"\x0F\x00"
				"\x0F\x00"
				"\x3F\x00"
				"\x01\x00"
				"\x01\x00"
				"\x01\x00"
				"\x00\x00"
				// Operator 1
				"\x02\x00"
				"\x0E\x00"
				"\x07\x00" // always overwritten with op0 value
				"\x0E\x00"
				"\x0E\x00"
				"\x00\x00"
				"\x0E\x00"
				"\x0E\x00"
				"\x3E\x00"
				"\x00\x00"
				"\x00\x00"
				"\x00\x00"
				"\x00\x00" // always overwritten with op0 value
				"Replaced\0\0\0\0\0\0\0\0\0\0\0\0"
				// Wave select 0 and 1
				"\x07\x00"
				"\x06\x00"
				// Unknown padding
				"\x01\x00"
			);
		}
};

IMPLEMENT_TESTS(ins_adlib);
