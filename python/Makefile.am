INCLUDES = $(PYTHON_INCLUDES)

pyexec_LTLIBRARIES = _gamemusic.la

_gamemusic_la_SOURCES = gamemusic_wrap.cxx
_gamemusic_la_LDFLAGS = -avoid-version -module
_gamemusic_la_LIBADD = $(BOOST_SYSTEM_LIBS) $(libgamecommon_LIBS) $(top_builddir)/src/libgamemusic.la
_gamemusic_la_CPPFLAGS = $(BOOST_CPPFLAGS) -I $(top_srcdir)/include $(libgamecommon_CFLAGS)

# Include the SWIG input file in the distribution
EXTRA_gamemusic_la_DIST = gamemusic.i

# This is one of SWIG's output files, but we include it in the dist so that
# end users don't need SWIG installed.  It's a pyexec file because it must be
# installed alongside the .so library in the pyexec dir to work.
dist_pyexec_DATA = gamemusic.py

# Delete the SWIG output files on a 'make maintainer-clean' so SWIG can
# regenerate them.
MAINTAINERCLEANFILES = gamemusic.py gamemusic_wrap.cxx

# In the event that the SWIG output files are missing or the SWIG input file
# has changed, use SWIG to rebuild them.  We can't include gamemusic.py as
# a target here or SWIG will be run twice.
%.py %_wrap.cxx: %.i
	if test -z "$(SWIG)" ; then \
		echo "SWIG is required to perform this action.  See http://www.swig.org"; \
		exit 1; \
	else \
		$(SWIG) -c++ -python -classic -Wextra -I$(top_srcdir)/include $(libgamecommon_CFLAGS) $<; \
	fi
