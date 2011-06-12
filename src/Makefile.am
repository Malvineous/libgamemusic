lib_LTLIBRARIES = libgamemusic.la

libgamemusic_la_SOURCES = main.cpp
libgamemusic_la_SOURCES += events.cpp
libgamemusic_la_SOURCES += music.cpp
libgamemusic_la_SOURCES += patch.cpp
libgamemusic_la_SOURCES += patch-opl.cpp
libgamemusic_la_SOURCES += patchbank.cpp
libgamemusic_la_SOURCES += patchbank-fmt-ibk.cpp
libgamemusic_la_SOURCES += mus-generic-opl.cpp
libgamemusic_la_SOURCES += mus-imf-idsoftware.cpp
libgamemusic_la_SOURCES += mus-dro-dosbox-v1.cpp
libgamemusic_la_SOURCES += mus-dro-dosbox-v2.cpp
libgamemusic_la_SOURCES += mus-raw-rdos.cpp
libgamemusic_la_SOURCES += mus-generic-midi.cpp
libgamemusic_la_SOURCES += mus-rawmidi.cpp
libgamemusic_la_SOURCES += mus-cmf-creativelabs.cpp
libgamemusic_la_SOURCES += mus-klm-wacky.cpp
libgamemusic_la_SOURCES += mus-mid-type0.cpp

EXTRA_libgamemusic_la_SOURCES = patchbank-fmt-ibk.hpp
EXTRA_libgamemusic_la_SOURCES += mus-imf-idsoftware.hpp
EXTRA_libgamemusic_la_SOURCES += mus-dro-dosbox-v1.hpp
EXTRA_libgamemusic_la_SOURCES += mus-dro-dosbox-v2.hpp
EXTRA_libgamemusic_la_SOURCES += mus-raw-rdos.hpp
EXTRA_libgamemusic_la_SOURCES += mus-generic-midi.hpp
EXTRA_libgamemusic_la_SOURCES += mus-rawmidi.hpp
EXTRA_libgamemusic_la_SOURCES += mus-cmf-creativelabs.hpp
EXTRA_libgamemusic_la_SOURCES += mus-klm-wacky.hpp
EXTRA_libgamemusic_la_SOURCES += mus-mid-type0.hpp

AM_CPPFLAGS = $(DEBUG_CPPFLAGS) $(BOOST_CPPFLAGS) -I $(top_srcdir)/include $(libgamecommon_CFLAGS)
AM_CXXFLAGS = $(DEBUG_CXXFLAGS)

libgamemusic_la_LDFLAGS = $(AM_LDFLAGS) -release @VERSION@ -version-info 0
libgamemusic_la_LIBADD = $(BOOST_SYSTEM_LIBS) $(BOOST_FILESYSTEM_LIBS) $(libgamecommon_LIBS)
