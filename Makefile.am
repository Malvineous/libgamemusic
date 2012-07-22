# "src" must go first so the library is available when the examples compile
SUBDIRS = src doc examples include utils tests

EXTRA_DIST = @PACKAGE@.pc.in README

DISTCLEANFILES = @PACKAGE@.pc

# Location of boost.m4
ACLOCAL_AMFLAGS = -I m4

srcdir = @srcdir@
VPATH = @srcdir@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = @PACKAGE@.pc
