/**
 * @file   test-mus-mus-dmx.cpp
 * @brief  Test code for DMX MUS files.
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

class test_mus_dmx: public test_music
{
	public:
		test_mus_dmx()
		{
			this->type = "mus-dmx";
			this->numInstruments = 1;
			this->indexInstrumentOPL = -1;
			this->indexInstrumentMIDI = 0;
			this->indexInstrumentPCM = -1;
			this->skipInstDetect.push_back("mus-dmx-raptor");
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Too short
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"MUS\x1A"
				"\x11\x00"
			));

			// c02: Invalid signature bytes
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"BUS\x1A"
				"\x11\x00" "\x12\x00" "\x01\x00" "\x00\x00" "\x01\x00" "\x00\x00"
				"\x02\x00"
				"\x40\x00\x02"
				"\x90\xB0\x7E"
				"\x81\x01"
				"\x90\x32"
				"\x7F"
				"\x00\x30"
				"\x80\x32"
				"\x10"
				"\x60"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"MUS\x1A"
				"\x14\x00" "\x12\x00" "\x01\x00" "\x00\x00" "\x01\x00" "\x00\x00"
				"\x02\x00"
				"\x40\x00\x02"
				"\x90\xB0\x7F"
				"\x81\x01"
				"\xA0\x90"
				"\x20"
				"\x90\x32"
				"\x7F"
				"\x00\x30"
				"\x80\x32"
				"\x10"
				"\x60"
			);
		}
};

IMPLEMENT_TESTS(mus_dmx);
