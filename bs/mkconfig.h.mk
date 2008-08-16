have += TCP
have += UN

have += AUDIOFILE
have += FAAD
have += FAACDECCONFIGURATION_DONTUPSAMPLEIMPLICITSBR
have += FAACDECCONFIGURATION_DOWNMATRIX
have += FAACDECFRAMEINFO_SAMPLERATE
have += MP4AUDIOSPECIFICCONFIG
have += FAAD_BUFLEN_FUNCS
have += FLAC
have += HELIXMP3
have += MIKMOD
have += MAD
have += MPCDEC
have += OGGFLAC
have += OGGVORBIS
have += TREMOR
have += WAVPACK
have += ALSA
have += AO
have += FIFO
have += JACK
have += MVP
have += OSX
have += OSS
have += PULSE
have += SHOUT
have += SUN

have += ID3TAG
have += ICONV
have += IPV6
have += LANGINFO_CODESET

MPD_PATH_MAX ?= 255
req_vars += MPD_PATH_MAX

export

include $(O)/config.mk
$(O)/config.h: $(O)/config_detected.h $(O)/config.mk
	echo '#ifndef CONFIG_H' > $@+
	echo '#define CONFIG_H' >> $@+
	$(SHELL) ./bs/pkginfo-header.sh >> $@+
	cat $(O)/config_detected.h >> $@+
	echo '/* user-enabled features: */' >> $@+
	for d in $(have); do eval "val=`echo '$$'HAVE_$$d` var=HAVE_$$d"; \
		if test -n "$$val"; then echo "#define $$var 1" >> $@+; \
		else echo "/* #undef $$var */" >> $@+; fi ; done
	for d in $(req_vars); do eval "val=`echo '$$'$$d`"; \
		echo "#define $$d $$val" >> $@+; done
	echo '#endif /* CONFIG_H */' >> $@+
	mv $@+ $@
.NOTPARALLEL:
