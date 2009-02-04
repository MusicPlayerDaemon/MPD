DOCBOOK_FILES = protocol.xml
DOCBOOK_HTML = $(patsubst %.xml,%/index.html,$(DOCBOOK_FILES))

man_MANS = mpd.1 mpd.conf.5
doc_DATA = mpdconf.example
EXTRA_DIST = $(man_MANS) $(DOCBOOK_FILES) mpdconf.example

if ENABLE_DOCUMENTATION
protocoldir = $(docdir)/protocol
protocol_DATA = $(wildcard protocol/*.html)

$(DOCBOOK_HTML): %/index.html: %.xml
	$(XMLTO) -o protocol --stringparam chunker.output.encoding=utf-8 html $<

all-local: $(DOCBOOK_HTML)

clean-local:
	rm -rf $(patsubst %.xml,%,$(DOCBOOK_FILES))
endif
