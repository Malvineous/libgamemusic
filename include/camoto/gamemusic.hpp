/**
 * @file   gamemusic.hpp
 * @brief  Main header for libgamemusic (includes everything.)
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

#ifndef _CAMOTO_GAMEMUSIC_HPP_
#define _CAMOTO_GAMEMUSIC_HPP_

/// Main namespace
namespace camoto {
/// Namespace for this library
namespace gamemusic {

/**

\mainpage libgamemusic

libgamemusic provides a standard interface to access different music file
formats.

\section structure Structure

The main interface to the library is the getManager() function, which returns
an instance of the Manager class.  The Manager is used to query supported
music formats, and for each supported file format it returns an instance of
the FormatType class.

The FormatType class can be used to examine files and check what file format
they are in, and if they are in the correct format, to open them.  Successfully
opening a music file produces an instance of the Music class.  The MusicType
class can also be used to create new music files from scratch, which will again
return a Music instance.

Reading and writing files through the Music class is done with lists of
"events", such as note-on and note-off.  These events are transparently
converted to and from the underlying file format by the Music class.

\section example Examples

The libgamemusic distribution comes with example code in the form of the
<a href="http://github.com/Malvineous/libgamemusic/blob/master/examples/gamemus.cpp">gamemus
utility</a>, which provides a simple command-line interface to the
full functionality of the library.

For a small "hello world" example, try this:

@include hello.cpp

When run, this program produces output similar to the following:

@verbatim
TODO
@endverbatim

\section info More information

Additional information including a mailing list is available from the Camoto
homepage <http://www.shikadi.net/camoto>.

**/
}
}

// These are all in the camoto::gamemusic namespace
#include <camoto/gamemusic/eventconverter-midi.hpp>
#include <camoto/gamemusic/eventconverter-opl.hpp>
#include <camoto/gamemusic/patch.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/patch-midi.hpp>
#include <camoto/gamemusic/patchbank.hpp>
#include <camoto/gamemusic/opl-util.hpp>
#include <camoto/gamemusic/events.hpp>
#include <camoto/gamemusic/exceptions.hpp>
#include <camoto/gamemusic/music.hpp>
#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/manager.hpp>

#endif // _CAMOTO_GAMEMUSIC_HPP_
