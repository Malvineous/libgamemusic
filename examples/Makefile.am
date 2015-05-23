bin_PROGRAMS = gamemus
noinst_PROGRAMS = hello

gamemus_SOURCES = gamemus.cpp
hello_SOURCES = hello.cpp

WARNINGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS  = -I $(top_srcdir)/include
AM_CPPFLAGS += $(BOOST_CPPFLAGS)
AM_CPPFLAGS += $(libgamecommon_CPPFLAGS)
AM_CPPFLAGS += $(WARNINGS)

AM_CXXFLAGS  = $(DEBUG_CXXFLAGS)
AM_CXXFLAGS += $(portaudio_CFLAGS)
AM_CXXFLAGS += $(libgamecommon_CFLAGS)

AM_LDFLAGS  = $(top_builddir)/src/libgamemusic.la
AM_LDFLAGS += $(BOOST_SYSTEM_LIB)
AM_LDFLAGS += $(BOOST_PROGRAM_OPTIONS_LIB)
AM_LDFLAGS += $(portaudio_LIBS)
AM_LDFLAGS += $(libgamecommon_LIBS)
