/**
 * @file   synth-opl.hpp
 * @brief  Interface to an OPL/FM/Adlib synthesizer.
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

#ifndef _CAMOTO_GAMEMUSIC_SYNTH_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_SYNTH_OPL_HPP_

#include <stdint.h>

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

namespace camoto {
namespace gamemusic {

/// Interface to an OPL/FM/Adlib synthesizer.
class DLL_EXPORT SynthOPL
{
	public:
		SynthOPL(unsigned long sampleRate);
		~SynthOPL();

		void reset();
		void write(unsigned int chip, unsigned int reg, unsigned int val);

		/// Synthesize and mix audio into the given buffer.
		void mix(int16_t *output, unsigned long len);

	protected:
		unsigned long outputSampleRate; ///< in Hertz, e.g. 44100

		// OPL stuff is stored in here so we don't have to #include it
		void *internal;
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_SYNTH_OPL_HPP_
