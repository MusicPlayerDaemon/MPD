bin_PROGRAMS = mpd

mpd_CFLAGS = $(MPD_CFLAGS)
mpd_CPPFLAGS = \
	$(SQLITE_CFLAGS) \
	$(CURL_CFLAGS) \
	$(MMS_CFLAGS) \
	$(AO_CFLAGS) $(ALSA_CFLAGS) \
	$(SHOUT_CFLAGS) \
	$(OGGVORBIS_CFLAGS) $(VORBISENC_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(AUDIOFILE_CFLAGS) $(LIBMIKMOD_CFLAGS) \
	$(MODPLUG_CFLAGS) \
	$(SIDPLAY_CFLAGS) \
	$(FLUIDSYNTH_CFLAGS) \
	$(WILDMIDI_CFLAGS) \
	$(ID3TAG_CFLAGS) \
	$(MAD_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(GLIB_CFLAGS)
mpd_LDADD = $(MPD_LIBS) \
	$(SQLITE_LIBS) \
	$(CURL_LIBS) \
	$(MMS_LIBS) \
	$(AO_LIBS) $(ALSA_LIBS) \
	$(SHOUT_LIBS) \
	$(OGGVORBIS_LIBS) $(VORBISENC_LIBS) $(FLAC_LIBS) \
	$(AUDIOFILE_LIBS) $(LIBMIKMOD_LIBS) \
	$(MODPLUG_LIBS) \
	$(SIDPLAY_LIBS) \
	$(FLUIDSYNTH_LIBS) \
	$(WILDMIDI_LIBS) \
	$(ID3TAG_LIBS) \
	$(MAD_LIBS) \
	$(MP4FF_LIBS) \
	$(FFMPEG_LIBS) \
	$(GLIB_LIBS)

mpd_headers = \
	notify.h \
	ack.h \
	audio.h \
	audio_format.h \
	audio_parser.h \
	audioOutput.h \
	output_internal.h \
	output_api.h \
	output_plugin.h \
	output_list.h \
	output_all.h \
	output_thread.h \
	output_control.h \
	output_state.h \
	output_print.h \
	output_command.h \
	output/shout_plugin.h \
	buffer2array.h \
	command.h \
	idle.h \
	cmdline.h \
	conf.h \
	crossfade.h \
	dbUtils.h \
	decoder_thread.h \
	decoder_control.h \
	decoder_plugin.h \
	decoder_command.h \
	decoder_buffer.h \
	decoder_api.h \
	decoder_plugin.h \
	decoder_internal.h \
	directory.h \
	directory_save.h \
	directory_print.h \
	database.h \
	encoder_plugin.h \
	encoder_list.h \
	encoder_api.h \
	update.h \
	dirvec.h \
	gcc.h \
	decoder_list.h \
	decoder/_flac_common.h \
	decoder/_ogg_common.h \
	input_stream.h \
	input_file.h \
	input_curl.h \
	input_mms.h \
	icy_metadata.h \
	client.h \
	listen.h \
	log.h \
	ls.h \
	main.h \
	mixer_api.h \
	event_pipe.h \
	daemon.h \
	normalize.h \
	compress.h \
	pipe.h \
	path.h \
	mapper.h \
	pcm_buffer.h \
	pcm_utils.h \
	pcm_convert.h \
	pcm_volume.h \
	pcm_mix.h \
	pcm_channels.h \
	pcm_format.h \
	pcm_resample.h \
	pcm_dither.h \
	pcm_prng.h \
	permission.h \
	player_thread.h \
	player_control.h \
	playlist.h \
	playlist_internal.h \
	playlist_print.h \
	playlist_save.h \
	playlist_state.h \
	queue.h \
	queue_print.h \
	queue_save.h \
	replay_gain.h \
	sig_handlers.h \
	song.h \
	song_print.h \
	song_save.h \
	song_sticker.h \
	songvec.h \
	state_file.h \
	stats.h \
	sticker.h \
	tag.h \
	tag_internal.h \
	tag_pool.h \
	tag_id3.h \
	tag_print.h \
	tag_save.h \
	strset.h \
	utils.h \
	volume.h \
	zeroconf.h zeroconf-internal.h \
	locate.h \
	stored_playlist.h \
	timer.h \
	archive_api.h \
	archive_internal.h \
	archive_list.h \
	input_archive.h


mpd_SOURCES = \
	$(mpd_headers) \
	notify.c \
	audio.c \
	audio_parser.c \
	audioOutput.c \
	output_api.c \
	output_list.c \
	output_all.c \
	output_thread.c \
	output_control.c \
	output_state.c \
	output_print.c \
	output_command.c \
	output_init.c \
	output/null_plugin.c \
	buffer2array.c \
	command.c \
	idle.c \
	cmdline.c \
	conf.c \
	crossfade.c \
	dbUtils.c \
	decoder_thread.c \
	decoder_control.c \
	decoder_buffer.c \
	decoder_api.c \
	directory.c \
	directory_save.c \
	directory_print.c \
	database.c \
	dirvec.c \
	update.c \
	decoder_list.c \
	input_stream.c \
	input_file.c \
	client.c \
	listen.c \
	log.c \
	ls.c \
	main.c \
	event_pipe.c \
	daemon.c \
	mixer_api.c \
	normalize.c \
	compress.c \
	pipe.c \
	path.c \
	mapper.c \
	pcm_convert.c \
	pcm_volume.c \
	pcm_mix.c \
	pcm_channels.c \
	pcm_format.c \
	pcm_resample.c \
	pcm_dither.c \
	permission.c \
	player_thread.c \
	player_control.c \
	playlist.c \
	playlist_global.c \
	playlist_control.c \
	playlist_edit.c \
	playlist_print.c \
	playlist_save.c \
	playlist_state.c \
	queue.c \
	queue_print.c \
	queue_save.c \
	replay_gain.c \
	sig_handlers.c \
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
	locate.c \
	stored_playlist.c \
	timer.c

if ENABLE_SQLITE
mpd_SOURCES += sticker.c song_sticker.c
endif

if HAVE_LIBSAMPLERATE
mpd_SOURCES += pcm_resample_libsamplerate.c
else
mpd_SOURCES += pcm_resample_fallback.c
endif

if HAVE_ID3TAG
mpd_SOURCES += tag_id3.c
endif

# archive plugins

if HAVE_BZ2
mpd_SOURCES += archive/bz2_plugin.c
endif

if HAVE_ZIP
mpd_SOURCES += archive/zip_plugin.c
endif

if HAVE_ISO
mpd_SOURCES += archive/iso_plugin.c
endif

if ENABLE_ARCHIVE
mpd_SOURCES += \
	archive_api.c \
	archive_list.c \
	input_archive.c
endif


# decoder plugins

if HAVE_MAD
mpd_SOURCES += decoder/mad_plugin.c
endif

if HAVE_MPCDEC
mpd_SOURCES += decoder/mpcdec_plugin.c
endif

if HAVE_WAVPACK
mpd_SOURCES += decoder/wavpack_plugin.c
endif

if HAVE_FAAD
mpd_SOURCES += decoder/faad_plugin.c
endif

if HAVE_MP4
mpd_SOURCES += decoder/mp4ff_plugin.c
endif

if HAVE_OGG_COMMON
mpd_SOURCES += decoder/_ogg_common.c
endif

if HAVE_FLAC_COMMON
mpd_SOURCES += decoder/_flac_common.c
endif

if HAVE_OGGVORBIS
mpd_SOURCES += decoder/vorbis_plugin.c
endif

if HAVE_FLAC
mpd_SOURCES += decoder/flac_plugin.c
endif

if HAVE_OGGFLAC
mpd_SOURCES += decoder/oggflac_plugin.c
endif

if HAVE_AUDIOFILE
mpd_SOURCES += decoder/audiofile_plugin.c
endif

if HAVE_MIKMOD
mpd_SOURCES += decoder/mikmod_plugin.c
endif

if HAVE_MODPLUG
mpd_SOURCES += decoder/modplug_plugin.c
endif

if ENABLE_SIDPLAY
mpd_SOURCES += decoder/sidplay_plugin.cxx
endif

if ENABLE_FLUIDSYNTH
mpd_SOURCES += decoder/fluidsynth_plugin.c
endif

if ENABLE_WILDMIDI
mpd_SOURCES += decoder/wildmidi_plugin.c
endif

if HAVE_FFMPEG
mpd_SOURCES += decoder/ffmpeg_plugin.c
endif

# encoder plugins

if ENABLE_ENCODER
mpd_SOURCES += encoder_list.c

if ENABLE_VORBIS_ENCODER
mpd_SOURCES += encoder/vorbis_encoder.c
endif

if ENABLE_LAME_ENCODER
mpd_SOURCES += encoder/lame_encoder.c
endif
endif


if HAVE_ZEROCONF
mpd_SOURCES += zeroconf.c

if HAVE_AVAHI
mpd_SOURCES += zeroconf-avahi.c
endif

if HAVE_BONJOUR
mpd_SOURCES += zeroconf-bonjour.c
endif
endif


if HAVE_CURL
mpd_SOURCES += input_curl.c icy_metadata.c
endif

if ENABLE_MMS
mpd_SOURCES += input_mms.c
endif


if HAVE_ALSA
mpd_SOURCES += output/alsa_plugin.c
mpd_SOURCES += mixer/alsa_mixer.c
endif

if HAVE_AO
mpd_SOURCES += output/ao_plugin.c
endif

if HAVE_FIFO
mpd_SOURCES += output/fifo_plugin.c
endif

if HAVE_JACK
mpd_SOURCES += output/jack_plugin.c
endif

if HAVE_MVP
mpd_SOURCES += output/mvp_plugin.c
endif

if HAVE_OSS
mpd_SOURCES += output/oss_plugin.c
mpd_SOURCES += mixer/oss_mixer.c
endif

if HAVE_OSX
mpd_SOURCES += output/osx_plugin.c
endif

if HAVE_PULSE
mpd_SOURCES += output/pulse_plugin.c
endif

if HAVE_SHOUT
mpd_SOURCES += output/shout_plugin.c
endif

if HAVE_SHOUT_MP3
mpd_SOURCES += output/shout_mp3.c
endif

if HAVE_SHOUT_OGG
mpd_SOURCES += output/shout_ogg.c
endif

# sparse is a semantic parser
# URL: git://www.kernel.org/pub/scm/devel/sparse/sparse.git
SPARSE = sparse
SPARSE_FLAGS =
SPARSE_CPPFLAGS = $(DEFAULT_INCLUDES) \
	-I$(shell $(CC) -print-file-name=include) \
	-I$(shell $(CC) -print-file-name=include-fixed)
sparse-check:
	for i in $(mpd_SOURCES); \
	do \
		$(SPARSE) -I. $(mpd_CFLAGS) $(mpd_CPPFLAGS) $(SPARSE_FLAGS) $(SPARSE_CPPFLAGS) $(srcdir)/$$i || exit; \
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
