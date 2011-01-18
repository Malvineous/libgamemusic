/**
 * @file   music.hpp
 * @brief  Declaration of top-level Music class, for accessing files
 *         storing music data.
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

#ifndef _CAMOTO_GAMEARCHIVE_MUSIC_HPP_
#define _CAMOTO_GAMEARCHIVE_MUSIC_HPP_

#include <boost/shared_ptr.hpp>
#include <exception>
#include <sstream>
#include <vector>

#include <camoto/types.hpp>
#include <camoto/gamemusic/patchbank.hpp>
#include <camoto/gamemusic/events.hpp>

namespace camoto {
namespace gamemusic {

/// Metadata item types.
enum E_METADATA {
	/// %Music description
	EM_DESCRIPTION,
};

/// Vector of metadata item types.
typedef std::vector<E_METADATA> VC_METADATA_ITEMS;

/// Primary interface to a music file.
/**
 * This class represents a music file.  Its functions are used to read "events"
 * from the underlying file, or to write events out to a blank file.
 *
 * @note Multithreading: Only call one function in this class at a time.  Many
 *       of the functions seek around the underlying stream and thus will break
 *       if two or more functions are executing at the same time.
 */
class MusicReader {

	public:

		MusicReader()
			throw ();

		virtual ~MusicReader()
			throw ();

		/// Get a PatchBank holding all instruments in the song.
		/**
		 * The instrument numbers in each NoteOn event are indices into the
		 * patch bank.
		 *
		 * @preconditions readNextEvent() has not yet been called.  If it has
		 *   been called, subsequent calls to readNextEvent() may return bad data.
		 *   This is because this function may seek in the underlying file, and
		 *   if events have already been read the "next" event may be the next one
		 *   or it may return to the first event depending on how this function
		 *   has been implemented.  So to be safe, only call this *before* the
		 *   first readNextEvent().
		 *
		 * @return The instrument list.
		 */
		virtual PatchBankPtr getPatchBank()
			throw () = 0;

		/// Get the next event from the file.
		/**
		 * Returns the next event in the file (starting with the first event the
		 * first time the function is called.)
		 *
		 * @preconditions You must call getPatchBank() first to load the
		 *   instruments, otherwise the note-on events do not have any list to
		 *   look up when matching patches.
		 *
		 * @return EventPtr containing the next event, or an empty pointer if
		 *         there are no more events to be read.
		 */
		virtual EventPtr readNextEvent()
			throw (std::ios::failure) = 0;

		/// Get the next event from the file and pass it to a HandleEvent instance.
		/**
		 * Retrieves the next event in the file (starting with the first event the
		 * first time the function is called) and passes it to one of the strongly
		 * typed functions in the supplied HandleEvent class instance.
		 *
		 * @param  strFilename Name of the file to search for.
		 * @return EventPtr containing the next event, or an empty pointer if
		 *         there are no more events to be read.
		 */
//		virtual void handleNextEvent(HandleEvent *cb)
//			throw (std::ios::failure) = 0;
// This isn't going to work because events can't be buffered then

		/// Find out which attributes can be set on files in this music.
		/**
		 * If an attribute is not returned by this function, that attribute must
		 * not be supplied to insert().
		 *
		 * Note to music format implementors: There is a default implementation
		 * of this function which returns 0.  Thus this only needs to be overridden
		 * if the music format does actually support any of the attributes.
		 *
		 * @return Zero or more E_ATTRIBUTE values OR'd together.
		 */
//		virtual int getSupportedAttributes() const
//			throw ();

		// The metadata functions all have no-op defaults, they only need to be
		// overridden for music formats that have metadata.

		/// Get a list of supported metadata elements that can be set.
		/**
		 * Some music formats have room for additional data, such as a comment
		 * or copyright notice.  This function is used to obtain a list of the
		 * metadata elements supported by the current format.  Not every format
		 * supports all the metadata types, and any optional elements will be
		 * included in this list (but getMetadata() may return an empty string for
		 * those.)
		 *
		 * Note to music format implementors: There is a default implementation
		 * of this function which returns an empty vector.  Thus this only needs
		 * to be overridden if the music format does actually support metadata.
		 *
		 * @return std::vector of \ref E_METADATA items.
		 */
		virtual VC_METADATA_ITEMS getMetadataList() const
			throw ();

		/// Get the value of a metadata element.
		/**
		 * Returns the value of a metadata item reported to exist by
		 * getMetadataList().
		 *
		 * Note to music format implementors: There is a default implementation
		 * of this function which always throws an exception.  Thus this only needs
		 * to be overridden if the music format does actually support metadata.
		 *
		 * @param  item Item to retrieve.  Must have been included in the list
		 *         returned by getMetadataList().
		 * @return A string containing the metadata (may be empty.)
		 */
		virtual std::string getMetadata(E_METADATA item) const
			throw (std::ios::failure);

};


/// Primary interface to write to a music file.
/**
 * This class is used to write events out to a new music file.
 *
 * This implements the EventHandler class, and events are "handled" by writing
 * them out to the underlying file.
 *
 * @note Multithreading: Only call one function in this class at a time.  Many
 *       of the functions seek around the underlying stream and thus will break
 *       if two or more functions are executing at the same time.
 */
class MusicWriter: virtual public EventHandler {

	public:

		MusicWriter()
			throw ();

		virtual ~MusicWriter()
			throw ();

		/// Set the instruments to use in this song.
		/**
		 * This must be called before start().
		 *
		 * @throws EInvalidPatchType if one or more of the patches are of a type
		 *   that is not supported by the music format (e.g. setting an OPL patch
		 *   when writing a MIDI file.)
		 */
		virtual void setPatchBank(const PatchBankPtr& instruments)
			throw (EBadPatchType) = 0;

		/// Find out which attributes can be set on files in this music.
		/**
		 * If an attribute is not returned by this function, that attribute must
		 * not be supplied to insert().
		 *
		 * Note to music format implementors: There is a default implementation
		 * of this function which returns 0.  Thus this only needs to be overridden
		 * if the music format does actually support any of the attributes.
		 *
		 * @return Zero or more E_ATTRIBUTE values OR'd together.
		 */
//		virtual int getSupportedAttributes() const
//			throw ();

		// The metadata functions all have no-op defaults, they only need to be
		// overridden for music formats that have metadata.

		/// Change the value of a metadata element.
		/**
		 * Only elements returned by getMetadataList() can be changed.
		 *
		 * Note to music format implementors: There is a default implementation
		 * of this function which always throws an exception.  Thus this only needs
		 * to be overridden if the music format does actually support metadata.
		 *
		 * @param  item Item to set.  Must have been included in the list returned
		 *         by getMetadataList().
		 * @param  value The value to set.  Passing an empty string will remove
		 *         the metadata element if possible, otherwise it will be set to
		 *         a blank.
		 */
		virtual void setMetadata(E_METADATA item, const std::string& value)
			throw (std::ios::failure);

		/// Start writing to the output stream.
		/**
		 * This function must be called before any events are written.  It
		 * writes the file header if there is one.  It must be called *after* all
		 * the metadata and instruments are set, in case these have to be written
		 * into the file header.
		 */
		virtual void start()
			throw (std::ios::failure) = 0;

		/// Finish writing to the output stream.
		/**
		 * This function writes out any footer or closing data to the stream.
		 * It must be called after the last event has been written.
		 */
		virtual void finish()
			throw (std::ios::failure) = 0;

		/// Write the next event to the file.
		/**
		 * Write out the given Event to the underlying file.
		 *
		 * @param  ev  A pointer to the %Event to write.
		 *
		 * @note   The first time this function is called it will write out the
		 *         file header, instruments, etc.  This means the instruments
		 *         and metadata must be set *before* this function is called.
		 */
/*		virtual void writeNextEvent(EventPtr ev)
			throw (std::ios::failure);

	protected:

		virtual void writeNextEvent(TempoEvent *ev)
			throw (std::ios::failure) = 0;

		virtual void writeNextEvent(NoteOnEvent *ev)
			throw (std::ios::failure) = 0;

		virtual void writeNextEvent(NoteOffEvent *ev)
			throw (std::ios::failure) = 0;
*/
};

/// Shared pointer to a MusicReader
typedef boost::shared_ptr<MusicReader> MusicReaderPtr;

/// Shared pointer to a MusicWriter
typedef boost::shared_ptr<MusicWriter> MusicWriterPtr;

// /// Vector of Music shared pointers.
//typedef std::vector<MusicPtr> VC_MUSIC;

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEARCHIVE_MUSIC_HPP_
