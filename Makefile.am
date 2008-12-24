ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.9 dist-bzip2

SUBDIRS = src doc
doc_DATA = AUTHORS COPYING NEWS README TODO UPGRADING
EXTRA_DIST = $(doc_DATA) $(wildcard m4/*.m4) autogen.sh scripts .gitignore

sparse-check test:
	$(MAKE) -C src $@

.PHONY: sparse-check test
