/**
 * @file   musictype.hpp
 * @brief  MusicType class, used to identify and open an instance of a
 *         particular music format.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_
#define _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_

#include <vector>
#include <map>

#include <camoto/stream.hpp>
#include <stdint.h>
#include <camoto/suppitem.hpp>
#include <camoto/gamemusic/music.hpp>

/// Main namespace
namespace camoto {
/// Namespace for this library
namespace gamemusic {

/// Interface to a particular music format.
class MusicType {

	public:

		/// Confidence level when guessing a file format.
		enum Certainty {
			DefinitelyNo,  ///< Definitely not in this format
			Unsure,        ///< The checks were inconclusive, it could go either way
			PossiblyYes,   ///< Everything checked out OK, but there's no signature
			DefinitelyYes, ///< This format has a signature and it matched
		};

		/// Get a short code to identify this file format, e.g. "imf-idsoftware"
		/**
		 * This can be useful for command-line arguments.
		 *
		 * @return The music short name/ID.
		 */
		virtual std::string getCode() const
			throw () = 0;

		/// Get the music format name, e.g. "id Software Music Format"
		/**
		 * @return The music name.
		 */
		virtual std::string getFriendlyName() const
			throw () = 0;

		/// Get a list of the known file extensions for this format.
		/**
		 * @return A vector of file extensions, e.g. "imf", "wlf"
		 */
		virtual std::vector<std::string> getFileExtensions() const
			throw () = 0;

		/// Check a stream to see if it's in this music format.
		/**
		 * @param  psMusic A C++ iostream of the file to test.
		 * @return A single confidence value from \ref MusicType::Certainty.
		 */
		virtual Certainty isInstance(stream::input_sptr psMusic) const
			throw (stream::error) = 0;

		/// Create a blank song in this format.
		/**
		 * This function writes out the necessary signatures and headers to create
		 * a valid blank music in this format.
		 *
		 * @param  psMusic A blank stream to store the new music in.
		 * @param  suppData Any supplemental data required by this format (see
		 *         getRequiredSupps()).
		 * @return A pointer to an instance of the MusicWriter class, just as if a
		 *         valid empty file had been opened by open().
		 */
		virtual MusicWriterPtr create(stream::output_sptr output, SuppData& suppData) const
			throw (stream::error) = 0;

		/// Open a music file.
		/**
		 * @pre    Recommended that isInstance() has returned > EC_DEFINITELY_NO.
		 * @param  psMusic The music file.
		 * @param  suppData Any supplemental data required by this format (see
		 *         getRequiredSupps()).
		 * @return A pointer to an instance of the MusicReader class.  Will throw an
		 *         exception if the data is invalid (i.e. if isInstance() returned
		 *         EC_DEFINITELY_NO) however it will try its best to read the data
		 *         anyway, to make it possible to "force" a file to be opened by a
		 *         particular format handler.
		 */
		virtual MusicReaderPtr open(stream::input_sptr input, SuppData& suppData) const
			throw (stream::error) = 0;

		/// Get a list of any required supplemental files.
		/**
		 * For some music formats, data is stored externally to the music file
		 * itself (for example the filenames may be stored in a different file than
		 * the actual file data.)  This function obtains a list of these
		 * supplementary files, so the caller can open them and pass them along
		 * to the music manipulation classes.
		 *
		 * @param  filenameMusic The filename of the music (no path.)  This is
		 *         for supplemental files which share the same base name as the
		 *         music, but a different filename extension.
		 * @return A (possibly empty) map associating required supplemental file
		 *         types with their filenames.  For each returned value the file
		 *         should be opened and placed in a SuppItem instance.  The
		 *         SuppItem is then added to an \ref MP_SUPPDATA map where it can
		 *         be passed to newMusic() or open().  Note that the filenames
		 *         returned can have relative paths, and may even have an absolute
		 *         path, if one was passed in with filenameMusic.
		 */
		virtual SuppFilenames getRequiredSupps(const std::string& filenameMusic) const
			throw () = 0;

};

/// Shared pointer to an MusicType.
typedef boost::shared_ptr<MusicType> MusicTypePtr;

/// Vector of MusicType shared pointers.
typedef std::vector<MusicTypePtr> VC_MUSICTYPE;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_
