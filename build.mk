#!/usr/bin/make -f
# default target
all:

# output directory can be set with O:=dir
ifeq ($(O),)
  O := .
endif
srcdir := src
include src/Makefile.am
mpd_SRC := $(addprefix src/,$(filter %.c,$(mpd_SOURCES)))
mpd_HDR := $(addprefix src/,$(filter %.h,$(mpd_SOURCES)))
mpd_OBJ := $(subst .c,.o,$(addprefix $(O)/,$(mpd_SRC)))
mpd_DEP := $(subst .o,.d,$(mpd_OBJ))

-include $(mpd_DEP)

uname_s := $(shell uname -s | tr A-Z a-z 2>/dev/null || echo unknown)
uname_m := $(shell uname -m | tr A-Z a-z 2>/dev/null || echo unknown)

# build: the machine type this builds on
# HOST: the machine this runs on
build := $(uname_s)-$(uname_m)
HOST ?= $(build)
-include $(O)/config.mk
-include $(O)/config_detected.mk
include bs/deps-audio.mk
include bs/deps-input.mk
include bs/deps-misc.mk
include bs/deps-conflicts.mk

RANLIB ?= ranlib
ifneq ($(HOST),$(build))
  ifeq ($(origin CC),default)
    CC := $(HOST)-$(CC)
  endif
  ifeq ($(origin RANLIB),default)
    RANLIB := $(HOST)-$(RANLIB)
  endif
  ifeq ($(origin AR),default)
    AR := $(HOST)-$(AR)
  endif
endif

mpd_CFLAGS += -I$(O)/src
mpd_LDFLAGS += -lm
mpd_DIRS := $(O)/src/inputPlugins $(O)/src/audioOutputs

STRIP ?= strip
export O CC CPPFLAGS CFLAGS HOST SHELL

defconfig: $(O)/config.mk

$(O)/config.mk:
	@test -d "$(@D)" || $(SHELL) ./bs/mkdir_p.sh "$(@D)"
	@if ! test -e $@; then \
		sed -e 's/@@HOST@@/$(HOST)/g' \
		  < bs/config.mk.default > $@+ && mv $@+ $@ && \
		  echo Please edit $(O)/config.mk to your liking; fi
	@touch $@

$(O)/src/../config.h: $(O)/config.h
$(O)/config.h: $(O)/config.mk $(O)/config_detected.h
	$(MAKE) -f bs/mkconfig.h.mk $(O)/config.h

$(O)/config_detected.h:
	@test -d "$(@D)" || $(SHELL) ./bs/mkdir_p.sh "$(@D)"
	> $@+ && $(SHELL) ./bs/mkconfig-header.sh >> $@+ && \
		$(SHELL) ./bs/cross-arch.sh >> $@+ && mv $@+ $@

all: $(O)/src/mpd
	@echo OK $<
strip: $(O)/src/mpd
	$(STRIP) $(STRIP_OPTS) $<
$(O)/src/mpd: $(mpd_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(mpd_CFLAGS) $(LDFLAGS) $(mpd_LDFLAGS)
$(O)/src/%.o: src/%.c $(O)/config.h
	@HDR_DEP_HACK="$(O)/config.h $(mpd_HDR)" \
		CFLAGS="$(CFLAGS) $(mpd_CFLAGS)" \
		$(SHELL) ./bs/mkdep.sh $< $(dir $<) $(subst .o,.d,$@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(mpd_CFLAGS) -c -o $@ $<
$(mpd_SRC): $(O)/config.h
clean:
	$(RM) $(O)/src/mpd $(mpd_OBJ)
reallyclean: clean
	$(RM) $(mpd_DEP) $(O)/depmode $(O)/config.h

# in case a .d file had a dep on a deleted file, we have this:
src/%.h src/%.c::
	@true
# we only do static linking in bs (since our main goal is cross-compiling),
# if you want dynamic linking, use autotools
$(O)/%.a::
	$(RM) $@
	$(AR) cru $@ $^
	$(RANLIB) $@
dist:
	$(SHELL) ./bs/mkdist.sh

.PHONY: clean reallyclean defconfig all
.PRECIOUS: $(O)/%.h
.SUFFIXES:
.SUFFIXES: .c .o .h .d
