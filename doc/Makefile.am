DOCBOOK_FILES = protocol.xml
DOCBOOK_HTML = $(patsubst %.xml,%.html,$(DOCBOOK_FILES))

man_MANS = mpd.1 mpd.conf.5
doc_DATA = mpdconf.example
EXTRA_DIST = $(man_MANS) $(doc_DATA)

MOSTLYCLEANFILES = $(DOCBOOK_HTML)

if HAVE_XMLTO
doc_DATA += $(DOCBOOK_HTML)

$(DOCBOOK_HTML): %.html: %.xml
	$(XMLTO) html-nochunks $<
endif
