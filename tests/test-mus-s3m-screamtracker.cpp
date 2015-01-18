/**
 * @file   test-mus-s3m-screamtracker.cpp
 * @brief  Test code for ScreamTracker 3 .s3m files.
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

class test_s3m_screamtracker: public test_music
{
	public:
		test_s3m_screamtracker()
		{
			this->type = "s3m-screamtracker";
			this->outputWidth = 0x10;
			this->numInstruments = 4;
			this->indexInstrumentOPL = 1;
			this->indexInstrumentMIDI = -1;
			this->indexInstrumentPCM = 0;
			this->hasMetadata[Metadata::Title] = true;
		}

		void addTests()
		{
			this->test_music::addTests();

			// c00: Normal
			this->isInstance(MusicType::DefinitelyYes, this->standard());

			// c01: Invalid signature bytes
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"Test title\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x1B\x11\x00\x00"
				"\x05\x00" "\x04\x00" "\x02\x00"
				"\x00\x00" "\x00\xCA" "\x02\x00"
				"SCRM"
				"\x40\x06\x7D\x30\x10\x00" "\0\0\0\0\0\0\0\0" "\x00\x00"
				"\x00\x08\x01\x09\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\x01\x01\x00\x01\xFF"
				"\x08\x00"
				"\x0E\x00"
				"\x13\x00"
				"\x18\x00"
				"\x1D\x00"
				"\x22\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
				/* Instrument 0 */
				"\x01" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x0D\x00"
				"\x10\x00\x00\x00"
				"\x02\x00\x00\x00"
				"\x08\x00\x00\x00"
				"\x2F\x00\x00\x01" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example PCM instrument\0\0\0\0\0\0" "SCRS"
				"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"
				"\0\0\0\0\0\0" // padding
				/* Instrument 1 */
				"\x02" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example OPL instrument\0\0\0\0\0\0" "SCRI"
				/* Instrument 2 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Instrument 3 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Pattern 0 */
				"\x4A\x00"
				"\x20\x35\x01"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x40\x30"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x20\xFE\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0" // padding
				/* Pattern 1 */
				"\x42\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
			));

			// c02: Invalid signature tag
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"Test title\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x1A\x10\x00\x00"
				"\x05\x00" "\x04\x00" "\x02\x00"
				"\x00\x00" "\x00\xCA" "\x02\x00"
				"SCRW"
				"\x40\x06\x7D\x30\x10\x00" "\0\0\0\0\0\0\0\0" "\x00\x00"
				"\x00\x08\x01\x09\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\x01\x01\x00\x01\xFF"
				"\x08\x00"
				"\x0E\x00"
				"\x13\x00"
				"\x18\x00"
				"\x1D\x00"
				"\x22\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
				/* Instrument 0 */
				"\x01" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x0D\x00"
				"\x10\x00\x00\x00"
				"\x02\x00\x00\x00"
				"\x08\x00\x00\x00"
				"\x2F\x00\x00\x01" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example PCM instrument\0\0\0\0\0\0" "SCRS"
				"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"
				"\0\0\0\0\0\0" // padding
				/* Instrument 1 */
				"\x02" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example OPL instrument\0\0\0\0\0\0" "SCRI"
				/* Instrument 2 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Instrument 3 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Pattern 0 */
				"\x4A\x00"
				"\x20\x35\x01"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x40\x30"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x20\xFE\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0" // padding
				/* Pattern 1 */
				"\x42\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
			));

			// c03: Too short
			this->isInstance(MusicType::DefinitelyNo, STRING_WITH_NULLS(
				"Test title\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x1A"
			));
		}

		virtual std::string standard()
		{
			return STRING_WITH_NULLS(
				"Test title\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x1A\x10\x00\x00"
				"\x05\x00" "\x04\x00" "\x02\x00"
				"\x00\x00" "\x00\xCA" "\x02\x00"
				"SCRM"
				"\x40\x06\x7D\x30\x10\x00" "\0\0\0\0\0\0\0\0" "\x00\x00"
				"\x00\x08\x01\x09\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\x01\x01\x00\x01\xFF"
				"\x08\x00"
				"\x0E\x00"
				"\x13\x00"
				"\x18\x00"
				"\x1D\x00"
				"\x22\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
				/* Instrument 0 */
				"\x01" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x0D\x00"
				"\x10\x00\x00\x00"
				"\x02\x00\x00\x00"
				"\x08\x00\x00\x00"
				"\x2F\x00\x00\x01" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example PCM instrument\0\0\0\0\0\0" "SCRS"
				"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"
				"\0\0\0\0\0\0" // padding
				/* Instrument 1 */
				"\x02" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example OPL instrument\0\0\0\0\0\0" "SCRI"
				/* Instrument 2 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Instrument 3 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Pattern 0 */
				"\x4A\x00"
				"\x20\x35\x01"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x40\x30"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x20\xFE\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0" // padding
				/* Pattern 1 */
				"\x42\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
			);
		}

		virtual std::string metadata_title_replaced()
		{
			return STRING_WITH_NULLS(
				"Replaced\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x1A\x10\x00\x00"
				"\x05\x00" "\x04\x00" "\x02\x00"
				"\x00\x00" "\x00\xCA" "\x02\x00"
				"SCRM"
				"\x40\x06\x7D\x30\x10\x00" "\0\0\0\0\0\0\0\0" "\x00\x00"
				"\x00\x08\x01\x09\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
				"\x01\x01\x00\x01\xFF"
				"\x08\x00"
				"\x0E\x00"
				"\x13\x00"
				"\x18\x00"
				"\x1D\x00"
				"\x22\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
				/* Instrument 0 */
				"\x01" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x0D\x00"
				"\x10\x00\x00\x00"
				"\x02\x00\x00\x00"
				"\x08\x00\x00\x00"
				"\x2F\x00\x00\x01" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example PCM instrument\0\0\0\0\0\0" "SCRS"
				"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"
				"\0\0\0\0\0\0" // padding
				/* Instrument 1 */
				"\x02" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example OPL instrument\0\0\0\0\0\0" "SCRI"
				/* Instrument 2 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Instrument 3 */
				"\x00" "\0\0\0\0\0\0\0\0\0\0\0\0"
				"\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x00\x00\x00\x00"
				"\x3F\x00\x00\x00" "\xAB\x20\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"Example empty instrument\0\0\0\0" "SCRS"
				/* Pattern 0 */
				"\x4A\x00"
				"\x20\x35\x01"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x40\x30"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x20\xFE\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0" // padding
				/* Pattern 1 */
				"\x42\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00"
				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // padding
			);
		}
};

IMPLEMENT_TESTS(s3m_screamtracker);
