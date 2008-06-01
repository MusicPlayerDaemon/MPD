ifeq ($(HAVE_ALSA),1)
  mpd_CFLAGS += $(ALSA_CFLAGS) -DHAVE_LIBASOUND=1
  mpd_LDFLAGS += $(ALSA_LDFLAGS) -lasound
endif

ifeq ($(HAVE_AO),1)
  # mpd_CFLAGS += -pthread
  mpd_LDFLAGS += -lao -lpthread
endif

ifeq ($(HAVE_FIFO),1)
endif

ifeq ($(HAVE_JACK),1)
  mpd_LDFLAGS += -ljack
endif

ifeq ($(HAVE_MVP),1)
endif

ifeq ($(HAVE_OSX),1)
  mpd_LDFLAGS += -framework AudioUnit -framework CoreServices
endif
ifeq ($(HAVE_OSS),1)
  # check some BSDs (-lossaudio?)
endif
ifeq ($(HAVE_PULSE),1)
  mpd_LDFLAGS += -lpulse -lpulse-simple
endif
ifeq ($(HAVE_SHOUT),1)
  mpd_CFLAGS += -DHAVE_SHOUT_SHOUT_H=1
  # XXX -ltheora, -lspeex
  mpd_LDFLAGS += -lshout -logg -lvorbis -lvorbisenc -ltheora -lspeex -lpthread
endif
ifeq ($(HAVE_SUN),1)
  mpd_CFLAGS += $(SUN_CFLAGS)
  mpd_LDFLAGS += $(SUN_LDFLAGS)
endif
