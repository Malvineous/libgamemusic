/**
 * @file  util-opl.cpp
 * @brief Utility functions related to OPL chips.
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

#include <iostream>
#include <cassert>
#include <camoto/util.hpp> // make_unique
#include <camoto/gamemusic/util-opl.hpp>
#include <camoto/gamemusic/patch-opl.hpp>

#define log2(x) (log(x) / 0.30102999566398119521373889472449)

namespace camoto {
namespace gamemusic {

int fnumToMilliHertz(unsigned int fnum, unsigned int block,
	unsigned int conversionFactor)
{
	assert(block < 8);
	assert(fnum < 1024);

	// Original formula
	//return 1000 * conversionFactor * (double)fnum * pow(2, (double)((signed)block - 20));

	// More efficient version
	return (1000ull * conversionFactor * fnum) >> (20 - block);
}

void milliHertzToFnum(unsigned int milliHertz,
	unsigned int *fnum, unsigned int *block, unsigned int conversionFactor)
{
	// Special case to avoid divide by zero
	if (milliHertz == 0) {
		*block = 0; // actually any block will work
		*fnum = 0;
		return;
	}

	// Special case for frequencies too high to produce
	if (milliHertz > 6208431) {
		*block = 7;
		*fnum = 1023;
		return;
	}

	/// This formula will provide a pretty good estimate as to the best block to
	/// use for a given frequency.  It tries to use the lowest possible block
	/// number that is capable of representing the given frequency.  This is
	/// because as the block number increases, the precision decreases (i.e. there
	/// are larger steps between adjacent note frequencies.)  The 6M constant is
	/// the largest frequency (in milliHertz) that can be represented by the
	/// block/fnum system.
	//int invertedBlock = log2(6208431 / milliHertz);

	// Very low frequencies will produce very high inverted block numbers, but
	// as they can all be covered by inverted block 7 (block 0) we can just clip
	// the value.
	//if (invertedBlock > 7) invertedBlock = 7;
	//*block = 7 - invertedBlock;

	// This is a bit more efficient and doesn't need log2() from math.h
	if (milliHertz > 3104215) *block = 7;
	else if (milliHertz > 1552107) *block = 6;
	else if (milliHertz > 776053) *block = 5;
	else if (milliHertz > 388026) *block = 4;
	else if (milliHertz > 194013) *block = 3;
	else if (milliHertz > 97006) *block = 2;
	else if (milliHertz > 48503) *block = 1;
	else *block = 0;

	// Original formula
	//*fnum = milliHertz * pow(2, 20 - *block) / 1000 / conversionFactor + 0.5;

	// Slightly more efficient version
	*fnum = ((unsigned long long)milliHertz << (20 - *block)) / (conversionFactor * 1000.0) + 0.5;

	if ((*block == 7) && (*fnum > 1023)) {
		std::cerr << "Warning: Frequency out of range, clipped to max" << std::endl;
		*fnum = 1023;
	}

	// Make sure the values come out within range
	//assert(*block >= 0);
	assert(*block <= 7);
	//assert(*fnum >= 0);
	//assert(*fnum < 1024);
	if (*fnum > 1024) *fnum = 1024; // TEMP
	return;
}


struct Purpose {
	OPLPatch::Rhythm rhythm;
	int map[6];
};

void mapInstrument(std::vector<Purpose>& instPurpose, OPLPatch::Rhythm rhythm,
	unsigned int *inst, std::shared_ptr<PatchBank>& patches)
{
	auto& p = instPurpose[*inst];
	unsigned int rhythm_int = (unsigned int)rhythm;

	if (p.map[rhythm_int] >= 0) {
		// This instrument has been set already, use that
		*inst = p.map[rhythm_int];
	} else {
		// This instrument hasn't been set
		if ((int)p.rhythm < 0) {
			// This instrument hasn't been used yet, so update the original def
			p.map[rhythm_int] = *inst;
			p.rhythm = rhythm;
			auto oplPatch = dynamic_cast<OPLPatch*>(patches->at(*inst).get());
			oplPatch->rhythm = rhythm;
		} else {
			// This instrument has already been used for another purpose, so make a
			// copy of it for this purpose.
			auto oplPatch = dynamic_cast<OPLPatch*>(patches->at(*inst).get());
			auto copy = std::make_shared<OPLPatch>(*oplPatch);
			copy->rhythm = rhythm;
			// Update the original to map here, but only in this rhythm mode
			*inst = patches->size();
			p.map[rhythm_int] = *inst;
			patches->push_back(copy);

			Purpose p2;
			// We could assign these to point to itself as we have effectively mapped
			// this new instrument to itself, however the song should never refer to
			// these instrument numbers, so it would be a waste of effort to assign
			// them here.  If the song did ever do this for some reason, the new
			// instrument would just appear as unassigned and it would be pointed at
			// itself on that first use anyway.
			for (unsigned int i = 0; i < 6; i++) p2.map[i] = -1;
			// But we do have to set the use so the duplicated instrument will have
			// its operators swapped if the format requires that.
			p2.rhythm = rhythm;
			instPurpose.push_back(p2);
		}
	}
	return;
}

void oplDenormalisePerc(Music& music, OPLNormaliseType method)
{
	std::vector<Purpose> instPurpose;
	for (unsigned int i = 0; i < music.patches->size(); i++) {
		Purpose p;
		p.rhythm = OPLPatch::Rhythm::Unknown;
		for (unsigned int n = 0; n < 6; n++) p.map[n] = -1;
		instPurpose.push_back(p);
	}
	// For each pattern
	unsigned int patternIndex = 0;
	for (auto& pattern : music.patterns) {

		// For each track
		unsigned int trackIndex = 0;
		for (auto& pt : pattern) {
			auto& ti = music.trackInfo[trackIndex];

			// For each event in the track
			for (auto& te : pt) {
				NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
				if (!ev) continue;

				// If we are here, this is a note-on event
				if (ev->instrument > music.patches->size()) continue;

				//Purpose& p = instPurpose[ev->instrument];
				OPLPatch::Rhythm rhythm = OPLPatch::Rhythm::Unknown;
				if (ti.channelType == TrackInfo::ChannelType::OPL) {
					rhythm = OPLPatch::Rhythm::Melodic;
				} else if (ti.channelType == TrackInfo::ChannelType::OPLPerc) {
					rhythm = (OPLPatch::Rhythm)(ti.channelIndex + 1);
				}
				if (rhythm >= OPLPatch::Rhythm::Melodic) {
					mapInstrument(instPurpose, rhythm, &ev->instrument, music.patches);
				}
			}
			trackIndex++;
		}
		patternIndex++;
	}

	// Swap the operators for those instruments used on one channel-type only
	for (auto& i : *music.patches) {
		auto oplPatch = dynamic_cast<OPLPatch*>(i.get());
		if (!oplPatch) continue;
		switch (method) {
			case OPLNormaliseType::CarFromMod:
				if (oplCarOnly(oplPatch->rhythm)) {
					// This instrument is only used on a carrier-only channel, and the
					// format says the carrier should be loaded from the modulator
					// settings.
					std::swap(oplPatch->c, oplPatch->m);
				}
				break;
			case OPLNormaliseType::ModFromCar:
				if (oplModOnly(oplPatch->rhythm)) {
					// This instrument is only used on a modulator-only channel, and the
					// format says the modulator should be loaded from the carrier
					// settings.
					std::swap(oplPatch->m, oplPatch->c);
				}
				break;
			case OPLNormaliseType::MatchingOps:
				break;
		}
	}
	return;
}

std::unique_ptr<PatchBank> oplNormalisePerc(const Music& music,
	OPLNormaliseType method)
{
	auto newPatchBank = std::make_unique<PatchBank>();

	// Swap the operators if needed
	for (auto& i : *music.patches) {
		auto oplPatchOrig = dynamic_cast<OPLPatch*>(i.get());
		if (!oplPatchOrig) {
			newPatchBank->push_back(i);
			continue;
		}
		auto oplPatch = std::make_shared<OPLPatch>(*oplPatchOrig);
		newPatchBank->push_back(oplPatch);
		switch (method) {
			case OPLNormaliseType::CarFromMod:
				if (oplCarOnly(oplPatch->rhythm)) {
					// This instrument is only used on a carrier-only channel, and the
					// format says the carrier should be loaded from the modulator
					// settings.
					std::swap(oplPatch->c, oplPatch->m);
				}
				break;
			case OPLNormaliseType::ModFromCar:
				if (oplModOnly(oplPatch->rhythm)) {
					// This instrument is only used on a modulator-only channel, and the
					// format says the modulator should be loaded from the carrier
					// settings.
					std::swap(oplPatch->m, oplPatch->c);
				}
				break;
			case OPLNormaliseType::MatchingOps:
				break;
		}
	}

	// As nice as it would be to be able to normalise all the instruments and
	// remove duplicates, this would really complicate the whole process as we'd
	// have to duplicate the Music instance and every event to avoid modifying
	// the original.  Plus that sort of cleanup is kind of up to the end-user
	// anyway.

/*
	// Identify any duplicate instruments
	std::vector<int> map;
	unsigned int ni = 0;
	for (PatchBank::iterator
		i = music->patches->begin(); i != music->patches->end(); /=* i++, *=/ ni++
	) {
		OPLPatchPtr oplPatchI = boost::dynamic_pointer_cast<OPLPatch>(*i);
		if (!oplPatchI) continue;

		bool found = false;
		unsigned int nj = 0;
		for (PatchBank::iterator
			j = music->patches->begin(); j != i; j++, nj++
		) {
			OPLPatchPtr oplPatchJ = boost::dynamic_pointer_cast<OPLPatch>(*j);
			if (!oplPatchJ) continue;
			if (*oplPatchI == *oplPatchJ) {
				// Use instrument J instead of I here as they are the same
				found = true;
				map.push_back(nj);
				// Remove this patch.
				// WARNING: This could remove a line in a song message.  It should be
				// replaced with an empty instrument if the title is different (and
				// non-empty)
				i = music->patches->erase(i);
				ni--;
				break;
			}
		}
		if (found) continue;

		// This instrument hasn't been used yet
		oplPatchI->rhythm = -1;
		map.push_back(ni);
		i++;
	}

	// Update the instrument numbers in the note-on events to compensate for any
	// removed instruments.

	// For each pattern
	unsigned int patternIndex = 0;
	for (std::vector<PatternPtr>::iterator
		pp = music->patterns.begin(); pp != music->patterns.end(); pp++, patternIndex++
	) {
		PatternPtr& pattern = *pp;

		// For each track
		unsigned int trackIndex = 0;
		for (Pattern::iterator
			pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++
		) {
			// For each event in the track
			for (Track::iterator
				tev = (*pt)->begin(); tev != (*pt)->end(); tev++
			) {
				TrackEvent& te = *tev;
				NoteOnEvent *ev = dynamic_cast<NoteOnEvent *>(te.event.get());
				if (!ev) continue;

				// If we are here, this is a note-on event
				if (ev->instrument > music->patches->size()) continue;

				assert(map[ev->instrument] >= 0);
				ev->instrument = map[ev->instrument];
			}
		}
	}
*/
	return std::move(newPatchBank);
}

} // namespace gamemusic
} // namespace camoto
