/**
 * @file   musictype.hpp
 * @brief  MusicType class, used to identify, read and write an instance of a
 *         particular music format.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
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
#include <stdint.h>
#include <camoto/error.hpp>
#include <camoto/stream.hpp>
#include <camoto/suppitem.hpp>
#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/exceptions.hpp>

/// Main namespace
namespace camoto {
/// Namespace for this library
namespace gamemusic {

/// Interface to a particular music format.
class MusicType
{
	public:
		/// Confidence level when guessing a file format.
		enum Certainty {
			DefinitelyNo,  ///< Definitely not in this format
			Unsure,        ///< The checks were inconclusive, it could go either way
			PossiblyYes,   ///< Everything checked out OK, but there's no signature
			DefinitelyYes, ///< This format has a signature and it matched
		};

		/// Output control flags.
		enum Flags {
			Default          = 0x00,  ///< No special treatment
			IntegerNotesOnly = 0x01,  ///< Disable pitchbends
		};

		/// Get a short code to identify this file format, e.g. "imf-idsoftware"
		/**
		 * This can be useful for command-line arguments.
		 *
		 * @return The music short name/ID.
		 */
		virtual std::string getCode() const = 0;

		/// Get the music format name, e.g. "id Software Music Format"
		/**
		 * @return The music name.
		 */
		virtual std::string getFriendlyName() const = 0;

		/// Get a list of the known file extensions for this format.
		/**
		 * @return A vector of file extensions, e.g. "imf", "wlf"
		 */
		virtual std::vector<std::string> getFileExtensions() const = 0;

		/// Check a stream to see if it's in this music format.
		/**
		 * @param input
		 *   The file to test.
		 *
		 * @return A single confidence value from \ref MusicType::Certainty.
		 */
		virtual Certainty isInstance(stream::input_sptr input) const = 0;

		/// Read a music file in this format.
		/**
		 * @pre Recommended that isInstance() has returned > DefinitelyNo.
		 *
		 * @param input
		 *   The music file to read.
		 *
		 * @param suppData
		 *   Any supplemental data required by this format (see getRequiredSupps()).
		 *
		 * @return A pointer to an instance of the Music class.  Will throw an
		 *   exception if the data is invalid (likely if isInstance() returned
		 *   DefinitelyNo) however it will try its best to read the data anyway, to
		 *   make it possible to "force" a file to be opened by a particular format
		 *   handler.
		 *
		 * @throw stream::error
		 *   I/O error reading from input stream (e.g. file truncated)
		 */
		virtual MusicPtr read(stream::input_sptr input, SuppData& suppData)
			const = 0;

		/// Write a song in this file format.
		/**
		 * This function writes out the necessary signatures and headers to create
		 * a valid blank music in this format.
		 *
		 * @param output
		 *   A blank stream to store the new song in.
		 *
		 * @param suppData
		 *   Any supplemental data required by this format (see getRequiredSupps()).
		 *
		 * @param music
		 *   The actual song data to write.
		 *
		 * @param flags
		 *   One or more \ref Flags values affecting the type of data written.
		 *
		 * @throw stream::error
		 *   I/O error writing to output stream (e.g. disk full)
		 *
		 * @throw format_limitation
		 *   This file format cannot store the requested data (e.g. a standard MIDI
		 *   file being asked to store PCM instruments.)  This will be a common
		 *   failure mode, so the error message should be presented to the user as
		 *   it will indicate what they are required to do to remedy the problem.
		 *
		 * @post The stream will be truncated to the correct size.
		 */
		virtual void write(stream::output_sptr output, SuppData& suppData,
			MusicPtr music, unsigned int flags) const = 0;

		/// Get a list of any required supplemental files.
		/**
		 * For some music formats, data is stored externally to the music file
		 * itself (for example the filenames may be stored in a different file than
		 * the actual file data.)  This function obtains a list of these
		 * supplementary files, so the caller can open them and pass them along
		 * to the music manipulation classes.
		 *
		 * @param input
		 *   Optional stream containing an existing file (or a NULL pointer if a
		 *   new file is to be created.)  This is used for file formats which store
		 *   filenames internally, allowing those filenames to be read out and
		 *   returned.  For newly created files (when this parameter is NULL),
		 *   default filenames are synthesised based on filenameMusic.
		 *
		 * @param filenameMusic
		 *   The filename of the music file (no path.)  This is for supplemental
		 *   files which share the same base name as the music, but a different
		 *   filename extension.
		 *
		 * @return A (possibly empty) map associating required supplemental file
		 *   types with their filenames.  For each returned value the file should be
		 *   opened and placed in a SuppItem instance.  The SuppItem is then added
		 *   to an \ref SuppData map where it can be passed to create() or
		 *   open().  Note that the filenames returned can have relative paths.
		 */
		virtual SuppFilenames getRequiredSupps(stream::input_sptr input,
			const std::string& filenameMusic) const = 0;

		/// Discover valid metadata supported by this file format.
		/**
		 *  @see camoto::Metadata::getMetadataList()
		 */
		virtual Metadata::MetadataTypes getMetadataList() const = 0;

};

/// Shared pointer to an MusicType.
typedef boost::shared_ptr<MusicType> MusicTypePtr;

/// Vector of MusicType shared pointers.
typedef std::vector<MusicTypePtr> MusicTypeVector;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_
