ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.9 dist-bzip2

SUBDIRS = src doc
docs = README UPGRADING
EXTRA_DIST = COPYING $(docs) m4 autogen.sh scripts .gitignore

sparse-check test:
	$(MAKE) -C src $@

.PHONY: sparse-check test
