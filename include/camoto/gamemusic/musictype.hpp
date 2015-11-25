/**
 * @file  camoto/gamemusic/musictype.hpp
 * @brief MusicType class, used to identify, read and write an instance of a
 *        particular music format.
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

#ifndef _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_
#define _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_

#include <vector>
#include <map>
#include <stdint.h>
#include <camoto/enum-ops.hpp>
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
		/// Type of object this class creates
		typedef Music obj_t;

		/// Type name as a string
		static constexpr const char* const obj_t_name = "Music"; // defined in main.cpp

		/// Confidence level when guessing a file format.
		enum class Certainty {
			DefinitelyNo,  ///< Definitely not in this format
			Unsure,        ///< The checks were inconclusive, it could go either way
			PossiblyYes,   ///< Everything checked out OK, but there's no signature
			DefinitelyYes, ///< This format has a signature and it matched
		};

		/// Output control flags.
		enum class WriteFlags {
			Default          = 0x00,  ///< No special treatment
			IntegerNotesOnly = 0x01,  ///< Disable pitchbends
		};

		/// Available capability flags, returned by caps().
		/**
		 * These are intended as guidelines to be used to warn users about loss of
		 * fidelity when converting between formats.  Conversion may proceed anyway,
		 * however some content will be dropped where possible, and exceptions will
		 * be thrown where this is not possible.
		 */
		enum class Caps {
			/// @todo How to handle songs with shared instrument banks like MIDI or Ken's Labyrinth?
			InstOPL       = 0x0001, ///< Can use OPL instruments
			InstOPLRhythm = 0x0002, ///< Can use OPL rhythm-mode percussive instruments (if unset, format does not support OPL rhythm mode)
			InstMIDI      = 0x0004, ///< Can use MIDI instruments
			InstPCM       = 0x0008, ///< Can use sampled (PCM) instruments

			/// Bitmask to check whether instruments are present.
			/**
			 * For file formats with external instrument banks (e.g. Ken's Labyrinth)
			 * this bitmask will return zero, because none of the Inst* flags will be
			 * set.)  In this case, since the song will just use indices into the
			 * instrument bank, the bank's caps() should be checked to see what sort
			 * of instruments can be stored, if needed.
			 */
			HasInstruments_Bitmask = 0x000F,

			HasEvents     = 0x0020, ///< Set if song, unset if instrument bank

			/// Keeps patterns separate.
			/**
			 * If unset, all patterns are merged into one (possibly duplicated based
			 * on order numbers) and a single pattern is passed to the write handler.
			 * On load, a single pattern is expected and it may be analysed and split
			 * automatically or with manual assistance into multiple patterns.
			 */
			HasPatterns   = 0x0080,

			HasLoopDest   = 0x0100, ///< Can the loop destination be set?

			HardwareOPL_Bitmask   = 0x0200, ///< Bitmask to check for OPL2 or OPL3
			HardwareOPL2  = 0x0200, ///< OPL2 only - max 9 channels or 6 + 5 perc
			HardwareOPL3  = 0x0600, ///< OPL2 or OPL3 - max 18 channels or 15 + 5 perc
		};

		/// Get a short code to identify this file format, e.g. "imf-idsoftware"
		/**
		 * This can be useful for command-line arguments.
		 *
		 * @return The music short name/ID.
		 */
		virtual std::string code() const = 0;

		/// Get the music format name, e.g. "id Software Music Format"
		/**
		 * @return The music name.
		 */
		virtual std::string friendlyName() const = 0;

		/// Get a list of the known file extensions for this format.
		/**
		 * @return A vector of file extensions, e.g. "imf", "wlf"
		 */
		virtual std::vector<std::string> fileExtensions() const = 0;

		/// Find out what features this file format supports.
		/**
		 * @return Zero or more \ref Caps items OR'd together.
		 */
		virtual Caps caps() const = 0;

		/// Check a stream to see if it's in this music format.
		/**
		 * @param input
		 *   The file to test.
		 *
		 * @return A single confidence value from \ref MusicType::Certainty.
		 */
		virtual Certainty isInstance(stream::input& input) const = 0;

		/// Read a music file in this format.
		/**
		 * @pre Recommended that isInstance() has returned > DefinitelyNo.
		 *
		 * @param content
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
		virtual std::unique_ptr<Music> read(stream::input& content,
			SuppData& suppData) const = 0;

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
		 *   One or more \ref WriteFlags values affecting the type of data written.
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
		virtual void write(stream::output& output, SuppData& suppData,
			const Music& music, WriteFlags flags) const = 0;

		/// Get a list of any required supplemental files.
		/**
		 * For some music formats, data is stored externally to the music file
		 * itself (for example the filenames may be stored in a different file than
		 * the actual file data.)  This function obtains a list of these
		 * supplementary files, so the caller can open them and pass them along
		 * to the music manipulation classes.
		 *
		 * @param content
		 *   Optional stream containing an existing file (or a NULL pointer if a
		 *   new file is to be created.)  This is used for file formats which store
		 *   filenames internally, allowing those filenames to be read out and
		 *   returned.  For newly created files (when this parameter is NULL),
		 *   default filenames are synthesised based on filename.
		 *
		 * @param filename
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
		virtual SuppFilenames getRequiredSupps(stream::input& content,
			const std::string& filename) const = 0;

		/// Discover valid metadata supported by this file format.
		/**
		 *  @see camoto::Metadata::supportedAttributes()
		 */
		virtual std::vector<Attribute> supportedAttributes() const = 0;

};

IMPLEMENT_ENUM_OPERATORS(MusicType::Caps);
IMPLEMENT_ENUM_OPERATORS(MusicType::WriteFlags);

/// ostream handler for printing Certainty types in test-case errors
CAMOTO_GAMEMUSIC_API std::ostream& operator << (std::ostream& s,
	const MusicType::Certainty& r);

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_MUSICTYPE_HPP_
