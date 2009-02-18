ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.9 dist-bzip2

SUBDIRS = src doc
doc_DATA = AUTHORS COPYING NEWS README TODO UPGRADING
EXTRA_DIST = $(doc_DATA) autogen.sh \
	$(wildcard scripts/*.sh) scripts/mpd.spec \
	.gitignore

AM_CFLAGS = -I$(srcdir)/src $(GLIB_CFLAGS)
AM_LDFLAGS = $(GLIB_LIBS)


#
# Test programs
#

if ENABLE_TEST

noinst_PROGRAMS = test/software_volume

test_software_volume_SOURCES = test/software_volume.c \
	src/audio_parser.c \
	src/pcm_volume.c

endif


sparse-check test:
	$(MAKE) -C src $@

.PHONY: sparse-check test
