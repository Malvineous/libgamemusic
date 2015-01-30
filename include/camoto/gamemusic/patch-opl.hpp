/**
 * @file  camoto/gamemusic/patch-opl.hpp
 * @brief Derived Patch for OPL patches.
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

#ifndef _CAMOTO_GAMEMUSIC_PATCH_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_PATCH_OPL_HPP_

#include <iomanip>
#include <stdint.h>
#include <camoto/gamemusic/patch.hpp>
#include <camoto/stream.hpp>

namespace camoto {
namespace gamemusic {

/// Descendent of Patch for storing OPL instrument settings.
struct OPLOperator {

	/// Default constructor sets everything to zero/defaults.
	OPLOperator();

	/// Is tremolo enabled?
	bool enableTremolo;

	/// Vibrato enabled.
	bool enableVibrato;

	/// Sustain enabled.
	bool enableSustain;

	/// KSR enabled.
	bool enableKSR;

	/// The frequency multiplication factor.  0-15 inclusive.
	uint8_t freqMult;

	/// The key scale level.  0-3 inclusive.
	uint8_t scaleLevel;

	/// The output level.  0-63 inclusive.
	uint8_t outputLevel;

	/// The attack rate.  0-15 inclusive.
	uint8_t attackRate;

	/// The decay rate.  0-15 inclusive.
	uint8_t decayRate;

	/// The sustain rate.  0-15 inclusive.
	uint8_t sustainRate;

	/// The release rate.  0-15 inclusive.
	uint8_t releaseRate;

	/// The waveform select.  0-7 inclusive.
	uint8_t waveSelect;

};

/// Descendent of Patch for storing OPL instrument settings.
struct DLL_EXPORT OPLPatch: public Patch {

	/// Default constructor sets everything to zero/defaults.
	OPLPatch();

	/// Modulator settings (operator 0).
	OPLOperator m;

	/// Carrier settings (operator 1).
	OPLOperator c;

	/// The feedback modulation factor for the channel.  0-7 inclusive.
	uint8_t feedback;

	/// The synth type connection.
	bool connection;

	/// Rhythm mode instrument.
	/**
	 * Rhythm mode instruments that only use a single operator, always load the
	 * settings from the respective operator fields.  So a modulator-only
	 * instrument always gets its settings from the m member, and a carrier-only
	 * instrument always gets its settings from the c member.
	 *
	 * Note that some file formats load carrier-only instruments from the
	 * modulator fields so these will need to be reversed upon loading.
	 * oplNormalisePerc() can help with this.
	 *
	 * -1 == unknown/multiple
	 * 0 == normal instrument (c and m valid)
	 * 1 == hihat (m only)
	 * 2 == top cymbal (c only)
	 * 3 == tom tom (m only)
	 * 4 == snare drum (c only)
	 * 5 == bass drum (c and m)
	 */
	enum Rhythm {
		Unknown   = -1,
		Melodic   =  0,
		HiHat     =  1,
		TopCymbal =  2,
		TomTom    =  3,
		SnareDrum =  4,
		BassDrum  =  5,
	};
	Rhythm rhythm;
};

/// True if OPLPatch::rhythm parameter is a carrier-only percussion instrument
inline bool oplCarOnly(OPLPatch::Rhythm c) {
	return (c == OPLPatch::TopCymbal) || (c == OPLPatch::SnareDrum);
}

/// True if OPLPatch::rhythm parameter is a modulator-only percussion instrument
inline bool oplModOnly(OPLPatch::Rhythm c) {
	return (c == OPLPatch::HiHat) || (c == OPLPatch::TomTom);
}

inline bool operator== (const OPLOperator& a, const OPLOperator& b)
{
	return
		(a.enableTremolo == b.enableTremolo)
		&& (a.enableVibrato == b.enableVibrato)
		&& (a.enableSustain == b.enableSustain)
		&& (a.enableKSR == b.enableKSR)
		&& (a.freqMult == b.freqMult)
		&& (a.scaleLevel == b.scaleLevel)
		//&& (a.outputLevel == b.outputLevel)  // changes for volume/velocity
		&& (a.attackRate == b.attackRate)
		&& (a.decayRate == b.decayRate)
		&& (a.sustainRate == b.sustainRate)
		&& (a.releaseRate == b.releaseRate)
		&& (a.waveSelect == b.waveSelect)
	;
}

inline bool operator== (const OPLPatch& a, const OPLPatch& b)
{
	// We want to be able to compare patches that target different rhythm
	// instruments to see if the actual patch parameters are equal.
//	if (a.rhythm != b.rhythm) return false;

	if ((
		(!oplModOnly(a.rhythm))
		|| (!oplModOnly(b.rhythm))
	) && (
		!(a.c == b.c)
	)) return false;  // carrier used and didn't match

	if ((
		(!oplCarOnly(a.rhythm))
		|| (!oplCarOnly(b.rhythm))
	) && (
		!(a.m == b.m)
	)) return false;  // modulator used and didn't match

	if ((
		(a.rhythm == -1) ||
		(a.rhythm == 0) ||
		(a.rhythm == 5)
	) && (
		!(a.m.outputLevel == b.m.outputLevel)
	)) return false;  // mod output level different on a 2-op patch

	return
		(a.feedback == b.feedback)
		&& (a.connection == b.connection)
	;
}

/// Shared pointer to a Patch.
typedef boost::shared_ptr<OPLPatch> OPLPatchPtr;

/// Convert the OPLPatch::rhythm value into text for error messages.
const char DLL_EXPORT *rhythmToText(OPLPatch::Rhythm rhythm);

} // namespace gamemusic
} // namespace camoto

/// ostream handler for printing patches as ASCII
DLL_EXPORT std::ostream& operator << (std::ostream& s, const camoto::gamemusic::OPLPatch& p);

/// ostream handler for printing patches as ASCII
DLL_EXPORT std::ostream& operator << (std::ostream& s, const camoto::gamemusic::OPLPatchPtr p);

#endif // _CAMOTO_GAMEMUSIC_PATCH_OPL_HPP_
