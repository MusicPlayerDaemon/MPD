ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.6

SUBDIRS = src doc
doc_DATA = README UPGRADING
EXTRA_DIST = COPYING $(doc_DATA) m4 autogen.sh bs build.mk scripts .gitignore

sparse-check test:
	$(MAKE) -C src $@

.PHONY: sparse-check test
