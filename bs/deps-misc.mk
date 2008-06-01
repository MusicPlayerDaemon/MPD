ifeq ($(HAVE_ID3TAG),1)
  mpd_LDFLAGS += -lid3tag -lz
endif

ifeq ($(HAVE_ICONV),1)
  ifeq ($(NEED_LIBICONV),1)
    mpd_LDFLAGS += -liconv
  endif
endif

ifeq ($(HAVE_IPV6),1)
endif

ifeq ($(HAVE_LANGINFO_CODESET),1)
endif

mpd_LDFLAGS += -lpthread
