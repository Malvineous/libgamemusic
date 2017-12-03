Camoto: Classic-game Modding Tools
==================================
Copyright 2010-2017 Adam Nielsen <<malvineous@shikadi.net>>  
http://www.shikadi.net/camoto/  
[![Build Status](https://travis-ci.org/Malvineous/libgamemusic.svg?branch=master)](https://travis-ci.org/Malvineous/libgamemusic)

Camoto is a collection of utilities for editing (modding) "classic" PC
games - those running under MS-DOS from the 1980s and 1990s.

This is **libgamemusic**, one component of the Camoto suite.  libgamemusic is a
library that can read and write many different music file formats, with a focus
on formats used by DOS games.

The library exposes each file format as a long list of "events", such as
note-on, tempo change, pitch bend, and so on.  This event list can then be
manipulated if required, and then written out to a music file in the same or
another format.

An instrument list is also provided, which allows manipulation of each
instrument used in the song.  Current instrument types are General MIDI (patch
numbers), OPL register values (used by Adlib songs) and sampled instruments
(as used by mod/tracker formats.)

A [JavaScript player](http://camoto.shikadi.net/libgamemusic/) is also
available that will play any supported file in a browser in real time on the
client side.

File formats from the following games are supported:

  * Bio Menace (.imf)
  * Blake Stone (.wlf)
  * Commander Keen (.imf)
  * Cosmo's Cosmic Adventures (.imf)
  * Dark Ages (.mid, with OPL patches in sysex events) [read only]
  * Doofus (.bsa)
  * Doom (.mus)
  * Drum Blaster (early .cmf)
  * Duke Nukem II (.imf)
  * God of Thunder (song*, *song)
  * Heretic (.mus)
  * Jill of the Jungle (.cmf)
  * Kiloblaster (.cmf)
  * Magnetic (.bsa)
  * Major Stryker (.wlf)
  * Math Rescue (.cmf)
  * Monster Bash (.imf)
  * Operation Body Count (.wlf)
  * Raptor (.mus, alt tempo)
  * Scud Atak (.cmf)
  * Solar Winds (.cmf)
  * Traffic Department 2192 (.cmf)
  * Trugg (.dsm)
  * Vinyl Goddess From Mars (.mus/.tim) [read only]
  * Wacky Wheels (.klm) [partial]
  * Wolfenstein 3-D (.wlf)
  * Word Rescue (.cmf)
  * Xargon (.cmf)
  * Zone 66 CDFM (.670)

These auxiliary file formats have also been implemented:

  * DOSBox raw OPL (.dro, v1 and v2)
  * Instrument Bank (.ibk)
  * RDOS raw OPL (.raw)
  * ScreamTracker 3 (.s3m)
  * Type-0/single-track MIDI (.mid)
  * AdLib Instrument (.ins)

Many more formats are planned.

The library is compiled and installed in the usual way:

    ./autogen.sh          # Only if compiling from git
    ./configure && make
    make check            # Optional, compile and run tests
    sudo make install
    sudo ldconfig

You will need the following prerequisites already installed:

  * [libgamecommon](https://github.com/Malvineous/libgamecommon) >= 2.0
  * Boost >= 1.59 (Boost >= 1.46 will work if not using `make check`)
  * PortAudio (optional, required only for playback with `gamemus --play`)
  * xmlto (optional for tarball releases, required for git version and if
    manpages are to be changed)

This distribution includes an example program `gamemus` which serves as both
a command-line interface to the library as well as an example of how to use
the library.  This program is installed as part of the `make install` process.
See [`man gamemus`](http://www.shikadi.net/camoto/manpage/gamemus) for full details.

To build the JavaScript player, which allows supported files to be played in a
web browser, build with Emscripten instead:

    ./autogen.sh
    emconfigure ./configure
    emmake make

The output files are placed in the `js/` subdirectory.  Note that due to the
way autotools works, the library files are placed in `js/.libs/`.

See http://camoto.shikadi.net/libgamemusic/ for working examples and for how to
use the web player without needing to compile anything yourself.

All supported file formats are fully documented on the
[ModdingWiki](http://www.shikadi.net/moddingwiki/Category:Music_formats).

This library is released under the GPLv3 license.
