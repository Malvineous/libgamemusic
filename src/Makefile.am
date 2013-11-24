lib_LTLIBRARIES = libgamemusic.la

libgamemusic_la_SOURCES = main.cpp
libgamemusic_la_SOURCES += decode-midi.cpp
libgamemusic_la_SOURCES += decode-opl.cpp
libgamemusic_la_SOURCES += encode-midi.cpp
libgamemusic_la_SOURCES += encode-opl.cpp
libgamemusic_la_SOURCES += eventconverter-midi.cpp
libgamemusic_la_SOURCES += eventconverter-opl.cpp
libgamemusic_la_SOURCES += events.cpp
libgamemusic_la_SOURCES += exceptions.cpp
libgamemusic_la_SOURCES += metadata-malv.cpp
libgamemusic_la_SOURCES += mus-cmf-creativelabs.cpp
libgamemusic_la_SOURCES += mus-cdfm-zone66.cpp
libgamemusic_la_SOURCES += mus-dro-dosbox-v1.cpp
libgamemusic_la_SOURCES += mus-dro-dosbox-v2.cpp
libgamemusic_la_SOURCES += mus-got.cpp
libgamemusic_la_SOURCES += mus-ibk-instrumentbank.cpp
libgamemusic_la_SOURCES += mus-imf-idsoftware.cpp
libgamemusic_la_SOURCES += mus-klm-wacky.cpp
libgamemusic_la_SOURCES += mus-mid-type0.cpp
libgamemusic_la_SOURCES += mus-raw-rdos.cpp
libgamemusic_la_SOURCES += opl-util.cpp
libgamemusic_la_SOURCES += patch.cpp
libgamemusic_la_SOURCES += patch-midi.cpp
libgamemusic_la_SOURCES += patch-opl.cpp
libgamemusic_la_SOURCES += patch-pcm.cpp
libgamemusic_la_SOURCES += patchbank.cpp

EXTRA_libgamemusic_la_SOURCES = decode-midi.hpp
EXTRA_libgamemusic_la_SOURCES += decode-opl.hpp
EXTRA_libgamemusic_la_SOURCES += encode-midi.hpp
EXTRA_libgamemusic_la_SOURCES += encode-opl.hpp
EXTRA_libgamemusic_la_SOURCES += metadata-malv.hpp
EXTRA_libgamemusic_la_SOURCES += mus-cmf-creativelabs.hpp
EXTRA_libgamemusic_la_SOURCES += mus-cdfm-zone66.hpp
EXTRA_libgamemusic_la_SOURCES += mus-dro-dosbox-v1.hpp
EXTRA_libgamemusic_la_SOURCES += mus-dro-dosbox-v2.hpp
EXTRA_libgamemusic_la_SOURCES += mus-got.hpp
EXTRA_libgamemusic_la_SOURCES += mus-ibk-instrumentbank.hpp
EXTRA_libgamemusic_la_SOURCES += mus-imf-idsoftware.hpp
EXTRA_libgamemusic_la_SOURCES += mus-klm-wacky.hpp
EXTRA_libgamemusic_la_SOURCES += mus-mid-type0.hpp
EXTRA_libgamemusic_la_SOURCES += mus-raw-rdos.hpp

WARNINGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS  = $(BOOST_CPPFLAGS)
AM_CPPFLAGS += -I $(top_srcdir)/include
AM_CPPFLAGS += $(libgamecommon_CPPFLAGS)
AM_CPPFLAGS += $(WARNINGS)

AM_CXXFLAGS  = $(DEBUG_CXXFLAGS)
AM_CXXFLAGS += $(libgamecommon_CFLAGS)

libgamemusic_la_LDFLAGS  = $(AM_LDFLAGS)
libgamemusic_la_LDFLAGS += -version-info 1:0:0

libgamemusic_la_LIBADD  = $(BOOST_SYSTEM_LIBS)
libgamemusic_la_LIBADD += $(BOOST_FILESYSTEM_LIBS)
libgamemusic_la_LIBADD += $(libgamecommon_LIBS)
