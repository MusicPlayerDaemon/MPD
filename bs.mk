#!/usr/bin/make -f
# default target
all:

export CPP CC CPPFLAGS CFLAGS LD LDFLAGS UNAME_S UNAME_M HOST TARGET O

# output directory can be set with O:=dir
ifeq ($(O),)
	O := .
endif

UNAME_S := $(shell uname -s 2>/dev/null || echo unknown)
UNAME_M := $(shell uname -m 2>/dev/null || echo unknown)
HOST := $(UNAME_S)-$(UNAME_M)
TARGET := $(HOST)

include src/Makefile.am

CFLAGS += -I $(O)/src
mpd_SRC := $(addprefix src/,$(filter %.c,$(mpd_SOURCES)))
mpd_HDR := $(addprefix src/,$(filter %.h,$(mpd_SOURCES)))
mpd_OBJ := $(subst .c,.o,$(addprefix $(O)/,$(mpd_SRC)))
mpd_DEP := $(subst .o,.d,$(mpd_OBJ))
DIRS := $(O)/src/inputPlugins $(O)/src/audioOutputs
HDR_DEP_HACK := $(addprefix $(0), $(mpd_HDR))
export HDR_DEP_HACK

dbg:
	@echo mpd_OBJ $(mpd_OBJ)
	@echo mpd_SRC $(mpd_SRC)
	@echo mpd_DEP $(mpd_DEP)

dep: $(mpd_DEP)
	@echo $(mpd_DEP)

$(O)/deftypes: bs/deftypes.c
	$(CC) $(CFLAGS) -o $@+ $<
	if test "$(HOST)" != "$(TARGET)"; then \
		cp bs/deftypes-cross.sh $@+ && chmod +x $@+; fi
	mv $@+ $@

$(O)/config.h: $(O)/deftypes $(O)/config.mk
	@-test -f $@ && mv $@ $@~
	./bs/mkconfig_header.sh > $@+ && $(O)/deftypes >> $@+ && mv $@+ $@

config: $(O)/config.h
$(O)/config.mk:
	@mkdir -p $(DIRS) && >> $@

-include $(O)/config.mk

$(O)/src/%.d: src/%.c $(O)/config.h
	./bs/mkdep.sh $< > $@+ && mv $@+ $@

include $(mpd_OBJ:.o=.d)

$(O)/src/%.o: $(O)/src/%.d $(O)/config.h $(O)/config.mk
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@+ $< && mv $@+ $@


