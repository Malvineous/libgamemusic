/**
 * @file   dro2txt.cpp
 * @brief  Program to convert DOSBox .dro files into ASCII text files.
 *
 * This program will create text files describing the events in a .dro capture,
 * in a format suitable for comparing with the 'diff' command.  The idea is to
 * capture an original song being played by a game in DOSBox, as well as
 * generating a .dro by converting the same song with libgamemusic.  The two
 * .dro files can then be converted into text files and compared using diff,
 * which will indicate whether the libgamemusic converter is faithfully
 * reproducing the same output as the game.
 *
 * Note that the text is not produced at the register level, because many
 * registers can be written in an arbitrary order while producing the same
 * output.  Thus the text output should only reflect what is heard, meaning two
 * very different .dro files can still compare as identical if they both sound
 * exactly the same, despite being very different at the byte level.
 *
 * Copyright (C) 2010-2012 Adam Nielsen <malvineous@shikadi.net>
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

#include <iostream>
#include <iomanip>
#include <camoto/iostream_helpers.hpp>
#include <camoto/stream_file.hpp>
#include <camoto/gamemusic.hpp>

/// Define this to display frequencies as "block/fnum" instead of milliHertz
//#define DISPLAY_FNUM

using namespace camoto;
namespace gm = camoto::gamemusic;

inline const char *percName(int c)
{
	switch (c) {
		case 0: return "HH";
		case 1: return "CY";
		case 2: return "TT";
		case 3: return "SD";
		case 4: return "BD";
	}
	return "??";
}

void printOp(uint8_t *oplState, int offset)
{
	std::cout << std::hex
		<< 0x20 + offset << '=' << std::setw(2) << (int)oplState[0x20 + offset] << ','
		<< 0x40 + offset << '=' << std::setw(2) << (int)oplState[0x40 + offset] << ','
		<< 0x60 + offset << '=' << std::setw(2) << (int)oplState[0x60 + offset] << ','
		<< 0x80 + offset << '=' << std::setw(2) << (int)oplState[0x80 + offset] << ','
		<< 0xE0 + offset << '=' << std::setw(2) << (int)oplState[0xE0 + offset];
	return;
}

bool globalDiff;  ///< Have we diff'd the chip-wide settings yet?
unsigned long nextDelay = 0;
#define PRINT_DELAY \
	if (nextDelay) { \
		std::cout << "Delay for " << std::dec << nextDelay << "ms\n"; \
		nextDelay = 0; \
	}

#define BIT_TOGGLED(b, v) ((o[b] ^ n[b]) & (v))
#define BIT_STATE(b, v)   (n[b] & (v))
void diffGlobalState(uint8_t *o, uint8_t *n)
{
	if (BIT_TOGGLED(0x01, 0x20)) {
		// WSEnable toggled
		std::cout << "Extended wavesel mode " << (BIT_STATE(0x01, 0x20) ? "enabled" : "disabled") << "\n";
	}
	if (BIT_TOGGLED(0x05, 0x01)) {
		// OPL3 enabled/disabled
		std::cout << "OPL3 mode " << (BIT_STATE(0x05, 0x01) ? "enabled" : "disabled") << "\n";
	}
	if (BIT_TOGGLED(0x08, 0x80)) {
		std::cout << "CSW mode " << (BIT_STATE(0x08, 0x80) ? "enabled" : "disabled") << "\n";
	}
	if (BIT_TOGGLED(0x08, 0x40)) {
		std::cout << "NOTE-SEL mode " << (BIT_STATE(0x08, 0x40) ? "enabled" : "disabled") << "\n";
	}
	if (BIT_TOGGLED(0xBD, 0x80)) {
		std::cout << "Deep tremolo " << (BIT_STATE(0xBD, 0x80) ? "enabled" : "disabled") << "\n";
	}
	if (BIT_TOGGLED(0xBD, 0x40)) {
		std::cout << "Deep vibrato " << (BIT_STATE(0xBD, 0x40) ? "enabled" : "disabled") << "\n";
	}
	if (BIT_TOGGLED(0xBD, 0x20)) {
		std::cout << "Rhythm mode " << (BIT_STATE(0xBD, 0x20) ? "enabled" : "disabled") << "\n";
	}
	return;
}

bool isOpChanged(uint8_t *o, uint8_t *n, int d)
{
	return
		(o[0x20 | d] ^ n[0x20 | d]) |
		(o[0x40 | d] ^ n[0x40 | d]) |
		(o[0x60 | d] ^ n[0x60 | d]) |
		(o[0x80 | d] ^ n[0x80 | d]) |
		(o[0xE0 | d] ^ n[0xE0 | d])
	;
}

void diffChannelState(uint8_t *o, uint8_t *n, int c)
{
	bool diff = false;
	int dm = OPLOFFSET_MOD(c), dc = OPLOFFSET_CAR(c);
	if (isOpChanged(o, n, dm) || isOpChanged(o, n, dc) ||
		(o[0xA0 | c] ^ n[0xA0 | c]) || (o[0xB0 | c] ^ n[0xB0 | c]) ||
		(o[0xC0 | c] ^ n[0xC0 | c])
	) {
		int fnum = n[0xA0 | c] | ((n[0xB0 | c] & 0x03) << 8);
		int block = (n[0xB0 | c] >> 2) & 7;
		int milliHertz = gm::fnumToMilliHertz(fnum, block, 49716);
		std::cout << "Channel " << std::dec << c+1 << " on @ " <<
#ifdef DISPLAY_FNUM
			block << '/' << std::hex << std::setw(3) << fnum <<
#else
			std::setw(7) << std::setfill(' ') << std::dec <<
			milliHertz << " mHz" << std::setfill('0') <<
#endif
			" with: ";
		printOp(n, dm);
		std::cout << ' ';
		printOp(n, dc);
		std::cout << ' ' << 0xC0 + c << '=' << std::setw(2) <<
			(int)n[0xC0 | c] << "\n";
	}
	return;
}

void diffPercState(uint8_t *o, uint8_t *n, int p)
{
	assert(p < 5);
	int c;
	bool bm, bc;
	switch (p) {
		case 0: c = 7; bm = true; bc = false; break; // HH
		case 1: c = 8; bm = false; bc = true; break; // CY
		case 2: c = 8; bm = true; bc = false; break; // TT
		case 3: c = 7; bm = false; bc = true; break; // SD
		case 4: c = 6; bm = true; bc = true;  break; // BD
		default: return;
	}
	bm = !bm; bc = !bc;
	if (
		(bm && isOpChanged(o, n, OPLOFFSET_MOD(c))) ||
		(bc && isOpChanged(o, n, OPLOFFSET_CAR(c))) ||
		(o[0xA0 | c] ^ n[0xA0 | c]) || (o[0xB0 | c] ^ n[0xB0 | c]) ||
		(o[0xC0 | c] ^ n[0xC0 | c])
	) {
		int fnum = n[0xA0 | c] | ((n[0xB0 | c] & 0x03) << 8);
		int block = (n[0xB0 | c] >> 2) & 7;
		int milliHertz = gm::fnumToMilliHertz(fnum, block, 49716);
		std::cout << "Perc " << percName(p) << "   on @ " <<
#ifdef DISPLAY_FNUM
			block << '/' << std::hex << std::setw(3) << fnum <<
#else
			std::setw(7) << std::setfill(' ') << std::dec <<
			milliHertz << " mHz" << std::setfill('0') <<
#endif
			" with: ";
		if (bm) {
			printOp(n, OPLOFFSET_MOD(c));
			std::cout << ' ';
		} else std::cout << std::setfill(' ') << std::setw(30) << ' ' << std::setfill('0');
		if (bc) {
			printOp(n, OPLOFFSET_CAR(c));
			std::cout << ' ';
		} else std::cout << std::setfill(' ') << std::setw(30) << ' ' << std::setfill('0');
		std::cout << 0xC0 + c << '=' << std::setw(2) <<
			(int)n[0xC0 | c] << "\n";
	}
	return;
}

int main(void)
{
	stream::input_sptr cin = stream::open_stdin();
	uint8_t cmdShortDelay, cmdLongDelay, lenCodemap;
	try {
		char sig[8];
		cin->read(sig, 8);
		if (strncmp(sig, "DBRAWOPL", 8) != 0) {
			std::cerr << "ERROR: Input file is not in DOSBox .dro format." << std::endl;
			return 1;
		}
		uint16_t verMajor, verMinor;
		cin >> u16le(verMajor) >> u16le(verMinor);
		if ((verMajor != 2) || (verMinor != 0)) {
			std::cerr << "ERROR: Only DOSBox .dro version 2.0 files are supported." << std::endl;
			return 1;
		}
		cin->seekg(11, stream::cur);
		cin
			>> u8(cmdShortDelay)
				>> u8(cmdLongDelay)
				>> u8(lenCodemap)
			;
	} catch (const stream::incomplete_read&) {
		std::cerr << "ERROR: Input file is not in DOSBox .dro format (short read)."
			<< std::endl;
		return 1;
	}
	uint8_t nextOplState[2][256];
	memset(nextOplState, 0, sizeof(nextOplState));

	uint8_t oplState[2][256];
	memset(oplState, 0, sizeof(oplState));

	uint8_t codeMap[256];
	memset(codeMap, 0, sizeof(codeMap));

	std::cout << std::setfill('0');

	bool first = true;

	try {
		cin->read((char *)codeMap, lenCodemap);
		for (;;) {
		uint8_t code;
		cin >> u8(code);
		if (code == cmdShortDelay) {
			uint8_t delay;
			cin >> u8(delay);
			nextDelay += delay + 1;
		} else if (code == cmdLongDelay) {
			uint16_t delay;
			cin >> u16le(delay);
			nextDelay += delay + 1;
		} else {
			int chip = code >> 7; // high bit
			uint8_t reg = codeMap[code & 0x7F];
			uint8_t val;
			cin >> u8(val);

			// Cache this value
			nextOplState[chip][reg] = val;
		}

		// Only print changes if they will be audible
		if (nextDelay > 5) {

			for (int chip = 0; chip < 2; chip++) {
				// If a note is playing, sync the cache/next state with the current one
				bool notePlaying = (nextOplState[chip][0xBD] & 0x20) && (nextOplState[chip][0xBD] & 0x1F);
				if (!notePlaying) { // no rhythm instrument playing, try the rest
					for (int c = 0; c < 9; c++) {
						if (nextOplState[chip][0xB0 | c] & OPLBIT_KEYON) {
							notePlaying = true;
							break;
						}
					}
				}

				if (notePlaying) {
					// There's a note playing, so check for any chip-wide changes
					diffGlobalState(oplState[chip], nextOplState[chip]);
				}

				// Now run through all the normal channels and see if any notes have
				// been toggled.
				for (int c = 0; c < 9; c++) {
					if (
						(oplState[chip][0xB0 | c] & OPLBIT_KEYON) &&
						(!(nextOplState[chip][0xB0 | c] & OPLBIT_KEYON))
					) {
						// keyon bit switched off
//						PRINT_DELAY;
						std::cout << "Channel " << std::dec << c+1 << " off\n";
					} else if (nextOplState[chip][0xB0 | c] & OPLBIT_KEYON) {
						// This channel is playing
						diffChannelState(oplState[chip], nextOplState[chip], c);
					}
				}

				// Same again but for perc.  Strictly this should be only if rhythm mode
				// is enabled, but we check anyway just in case.
				for (int p = 0; p < 5; p++) {
					int bit = 1 << p;
					if (
						(oplState[chip][0xBD] & bit) &&
						(!(nextOplState[chip][0xBD] & bit))
					) {
						// keyon bit switched off
//						PRINT_DELAY;
						std::cout << "Perc " << percName(p) << " off\n";
					} else if (nextOplState[chip][0xBD] & bit) {
						// This channel is playing
						diffPercState(oplState[chip], nextOplState[chip], p);
					}
				}

				// Now all the differences have been shown, so sync the two register maps
				memcpy(oplState, nextOplState, sizeof(oplState));
			}
			PRINT_DELAY;
		}
	}
	} catch (const stream::incomplete_read&) {
		// complete
	}
	return 0;
}
