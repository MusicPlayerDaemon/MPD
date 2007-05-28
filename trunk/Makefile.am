AUTOMAKE_OPTIONS = foreign 1.6

SUBDIRS = src doc
docdir = $(prefix)/share/doc/$(PACKAGE)
doc_DATA = README UPGRADING
EXTRA_DIST = COPYING $(doc_DATA)

sparse-check:
	$(MAKE) -C src $@
