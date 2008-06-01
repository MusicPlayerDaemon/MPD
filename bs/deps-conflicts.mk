ifeq ($(HAVE_TREMOR),1)
  ifeq ($(HAVE_SHOUT),1)
    $(error Tremor and Shout are incompatible, enable one or the other)
  endif
  ifeq ($(HAVE_OGGFLAC),1)
    $(error Tremor and OggFLAC are incompatible, enable one or the other)
  endif
endif

ifeq ($(HAVE_HELIXMP3),1)
  ifeq ($(HAVE_MAD),1)
    $(error Helix MP3 and MAD are incompatible, enable one or the other)
  endif
endif
