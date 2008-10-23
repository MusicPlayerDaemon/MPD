bin_PROGRAMS = mpd
SUBDIRS = $(MP4FF_SUBDIR)

mpd_audioOutputs = \
	audioOutputs/audioOutput_shout.c \
	audioOutputs/audioOutput_shout_ogg.c \
	audioOutputs/audioOutput_shout_mp3.c \
	audioOutputs/audioOutput_null.c \
	audioOutputs/audioOutput_fifo.c \
	audioOutputs/audioOutput_alsa.c \
	audioOutputs/audioOutput_ao.c \
	audioOutputs/audioOutput_oss.c \
	audioOutputs/audioOutput_osx.c \
	audioOutputs/audioOutput_pulse.c \
	audioOutputs/audioOutput_mvp.c \
	audioOutputs/audioOutput_jack.c

mpd_headers = \
	notify.h \
	ack.h \
	audio.h \
	audio_format.h \
	audioOutput.h \
	output_internal.h \
	output_api.h \
	output_list.h \
	output_thread.h \
	output_control.h \
	audioOutputs/audioOutput_shout.h \
	buffer2array.h \
	command.h \
	idle.h \
	condition.h \
	conf.h \
	crossfade.h \
	dbUtils.h \
	decoder_thread.h \
	decoder_control.h \
	decoder_api.h \
	decoder_internal.h \
	directory.h \
	directory_save.h \
	directory_print.h \
	database.h \
	update.h \
	dirvec.h \
	gcc.h \
	decoder_list.h \
	inputPlugins/_flac_common.h \
	inputPlugins/_ogg_common.h \
	inputStream.h \
	inputStream_file.h \
	inputStream_http.h \
	inputStream_http_auth.h \
	client.h \
	list.h \
	dlist.h \
	listen.h \
	log.h \
	ls.h \
	main_notify.h \
	normalize.h \
	compress.h \
	os_compat.h \
	outputBuffer.h \
	path.h \
	mapper.h \
	pcm_utils.h \
	pcm_resample.h \
	pcm_dither.h \
	permission.h \
	player_thread.h \
	player_control.h \
	playerData.h \
	playlist.h \
	playlist_save.h \
	replayGain.h \
	ringbuf.h \
	signal_check.h \
	sig_handlers.h \
	sllist.h \
	song.h \
	song_print.h \
	song_save.h \
	songvec.h \
	state_file.h \
	stats.h \
	tag.h \
	tag_internal.h \
	tag_pool.h \
	tag_id3.h \
	tag_print.h \
	tag_save.h \
	utf8.h \
	strset.h \
	utils.h \
	volume.h \
	ioops.h \
	zeroconf.h \
	locate.h \
	stored_playlist.h \
	timer.h


mpd_SOURCES = \
	$(mpd_headers) \
	$(mpd_audioOutputs) \
	notify.c \
	audio.c \
	audioOutput.c \
	output_api.c \
	output_list.c \
	output_thread.c \
	output_control.c \
	output_init.c \
	buffer2array.c \
	command.c \
	idle.c \
	condition.c \
	conf.c \
	crossfade.c \
	dbUtils.c \
	decoder_thread.c \
	decoder_control.c \
	decoder_api.c \
	directory.c \
	directory_save.c \
	directory_print.c \
	database.c \
	dirvec.c \
	update.c \
	decoder_list.c \
	inputStream.c \
	inputStream_file.c \
	inputStream_http.c \
	client.c \
	ioops.c \
	list.c \
	listen.c \
	log.c \
	ls.c \
	main.c \
	main_notify.c \
	normalize.c \
	compress.c \
	outputBuffer.c \
	path.c \
	mapper.c \
	pcm_utils.c \
	pcm_resample.c \
	pcm_dither.c \
	permission.c \
	player_thread.c \
	player_control.c \
	playerData.c \
	playlist.c \
	playlist_save.c \
	replayGain.c \
	ringbuf.c \
	sig_handlers.c \
	signal_check.c \
	sllist.c \
	song.c \
	song_print.c \
	song_save.c \
	songvec.c \
	state_file.c \
	stats.c \
	tag.c \
	tag_pool.c \
	tag_print.c \
	tag_save.c \
	strset.c \
	utils.c \
	volume.c \
	utf8.c \
	locate.c \
	stored_playlist.c \
	timer.c

if HAVE_LIBSAMPLERATE
mpd_SOURCES += pcm_resample_libsamplerate.c
else
mpd_SOURCES += pcm_resample_fallback.c
endif

if HAVE_ID3TAG
mpd_SOURCES += tag_id3.c
endif


# decoder plugins

if HAVE_MAD
mpd_SOURCES += inputPlugins/mp3_plugin.c
endif

if HAVE_MPCDEC
mpd_SOURCES += inputPlugins/mpc_plugin.c
endif

if HAVE_WAVPACK
mpd_SOURCES += inputPlugins/wavpack_plugin.c
endif

if HAVE_FAAD
mpd_SOURCES += inputPlugins/aac_plugin.c inputPlugins/mp4_plugin.c
endif

if HAVE_OGG_COMMON
mpd_SOURCES += inputPlugins/_ogg_common.c
endif

if HAVE_FLAC_COMMON
mpd_SOURCES += inputPlugins/_flac_common.c
endif

if HAVE_OGGVORBIS
mpd_SOURCES += inputPlugins/oggvorbis_plugin.c
endif

if HAVE_FLAC
mpd_SOURCES += inputPlugins/flac_plugin.c
endif

if HAVE_OGGFLAC
mpd_SOURCES += inputPlugins/oggflac_plugin.c
endif

if HAVE_AUDIOFILE
mpd_SOURCES += inputPlugins/audiofile_plugin.c
endif

if HAVE_MIKMOD
mpd_SOURCES += inputPlugins/mod_plugin.c
endif

if HAVE_FFMPEG
mpd_SOURCES += inputPlugins/ffmpeg_plugin.c
endif


if HAVE_ZEROCONF
mpd_SOURCES += zeroconf.c
endif


mpd_CFLAGS = $(MPD_CFLAGS)
mpd_CPPFLAGS = \
	$(AO_CFLAGS) $(ALSA_CFLAGS) \
	$(SHOUT_CFLAGS) \
	$(OGGVORBIS_CFLAGS) $(VORBISENC_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(AUDIOFILE_CFLAGS) $(LIBMIKMOD_CFLAGS) \
	$(ID3TAG_CFLAGS) \
	$(MAD_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(GLIB_CFLAGS)
mpd_LDADD = $(MPD_LIBS) $(MP4FF_LIB) \
	$(AO_LIBS) $(ALSA_LIBS) \
	$(SHOUT_LIBS) \
	$(OGGVORBIS_LIBS) $(VORBISENC_LIBS) $(FLAC_LIBS) \
	$(AUDIOFILE_LIBS) $(LIBMIKMOD_LIBS) \
	$(ID3TAG_LIBS) \
	$(MAD_LIBS) \
	$(FFMPEG_LIBS) \
	$(GLIB_LIBS)

DIST_SUBDIRS = mp4ff

# sparse is a semantic parser
# URL: git://www.kernel.org/pub/scm/devel/sparse/sparse.git
SPARSE = sparse
SPARSE_FLAGS =
sparse-check:
	for i in $(mpd_SOURCES); \
	do \
		$(SPARSE) -I. $(mpd_CFLAGS) $(SPARSE_FLAGS) $(srcdir)/$$i || exit; \
	done

TEST_CFLAGS = -DUNIT_TEST
TEST_FILES := $(shell grep UNIT_TEST \
               $(addprefix $(srcdir)/, $(mpd_SOURCES)) | \
               awk -F: '{print $$1}' | uniq)

test: $(addprefix test-, $(subst .c,,$(TEST_FILES)))

test-%: %.c
	$(CC) $(CFLAGS) $(TEST_CFLAGS) -o $@ $<
	@./$@
	@echo $@: OK

.PHONY: sparse-check test
