ifeq ($(HAVE_AUDIOFILE),1)
  mpd_LDFLAGS += -laudiofile -lm
endif

ifeq ($(HAVE_FAAD),1)
  # XXX needs work
  mpd_CFLAGS += $(FAAD_CFLAGS)
  mpd_LDFLAGS += -lfaad -lmp4ff
endif

ifeq ($(HAVE_FLAC),1)
  mpd_LDFLAGS += -lFLAC -lm
endif

ifeq ($(HAVE_HELIXMP3),1)
  mpd_CFLAGS += $(HELIXMP3_CFLAGS)
  mpd_LDFLAGS += $(HELIXMP3_LDFLAGS) -lmp3codecfixpt
endif

ifeq ($(HAVE_MIKMOD),1)
  mpd_LDFLAGS += -lmikmod -lm
endif

ifeq ($(HAVE_MAD),1)
  mpd_LDFLAGS += -lmad -lm
endif

ifeq ($(HAVE_MPCDEC),1)
  mpd_LDFLAGS += -lmpcdec
endif

ifeq ($(HAVE_OGGFLAC),1)
  mpd_LDFLAGS += $(OGGFLAC_LDFLAGS) -lOggFLAC -logg -lFLAC -lm
endif

ifeq ($(HAVE_OGGVORBIS),1)
  ifeq ($(HAVE_TREMOR),1)
    mpd_LDFLAGS += $(TREMOR_LDFLAGS) -lvorbisidec
  else
    mpd_LDFLAGS += $(OGGVORBIS_LDFLAGS) -lvorbis -logg -lvorbisfile
  endif
endif



