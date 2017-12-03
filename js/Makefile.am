if USING_EMSCRIPTEN

bin_PROGRAMS = libgamemusic-player-asm.js

libgamemusic_player_asm_js_SOURCES = main.cpp

WARNINGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS  = -I $(top_srcdir)/include
AM_CPPFLAGS += $(libgamecommon_CPPFLAGS)
AM_CPPFLAGS += $(WARNINGS)

AM_CXXFLAGS  = $(DEBUG_CXXFLAGS)
AM_CXXFLAGS += $(libgamecommon_CFLAGS)
AM_CXXFLAGS += --bind

AM_LDFLAGS  = $(top_builddir)/src/libgamemusic.la
AM_LDFLAGS += $(libgamecommon_LIBS)
AM_LDFLAGS += -s 'DISABLE_EXCEPTION_CATCHING=0'

endif
