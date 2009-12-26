ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.9 dist-bzip2

AM_CPPFLAGS = -I$(srcdir)/src $(GLIB_CFLAGS)

bin_PROGRAMS = src/mpd

src_mpd_CFLAGS = $(AM_CFLAGS) $(MPD_CFLAGS)
src_mpd_CPPFLAGS = $(AM_CPPFLAGS) \
	$(SQLITE_CFLAGS) \
	$(ARCHIVE_CFLAGS) \
	$(INPUT_CFLAGS) \
	$(TAG_CFLAGS) \
	$(DECODER_CFLAGS) \
	$(ENCODER_CFLAGS) \
	$(FILTER_CFLAGS) \
	$(OUTPUT_CFLAGS)
src_mpd_LDADD = $(MPD_LIBS) \
	$(SQLITE_LIBS) \
	$(ARCHIVE_LIBS) \
	$(INPUT_LIBS) \
	$(TAG_LIBS) \
	$(DECODER_LIBS) \
	$(ENCODER_LIBS) \
	$(OUTPUT_LIBS) \
	$(FILTER_LIBS) \
	$(GLIB_LIBS)

mpd_headers = \
	src/notify.h \
	src/ack.h \
	src/audio.h \
	src/audio_format.h \
	src/audio_parser.h \
	src/output_internal.h \
	src/output_api.h \
	src/output_plugin.h \
	src/output_list.h \
	src/output_all.h \
	src/output_thread.h \
	src/output_control.h \
	src/output_state.h \
	src/output_print.h \
	src/output_command.h \
	src/buffer2array.h \
	src/command.h \
	src/idle.h \
	src/cmdline.h \
	src/conf.h \
	src/crossfade.h \
	src/dbUtils.h \
	src/decoder_thread.h \
	src/decoder_control.h \
	src/decoder_plugin.h \
	src/decoder_command.h \
	src/decoder_buffer.h \
	src/decoder_api.h \
	src/decoder_plugin.h \
	src/decoder_internal.h \
	src/directory.h \
	src/directory_save.h \
	src/directory_print.h \
	src/database.h \
	src/encoder_plugin.h \
	src/encoder_list.h \
	src/encoder_api.h \
	src/fifo_buffer.h \
	src/update.h \
	src/dirvec.h \
	src/gcc.h \
	src/decoder_list.h \
	src/decoder/_flac_common.h \
	src/decoder/_ogg_common.h \
	src/input_plugin.h \
	src/input_stream.h \
	src/input/file_input_plugin.h \
	src/input/curl_input_plugin.h \
	src/input/lastfm_input_plugin.h \
	src/input/mms_input_plugin.h \
	src/icy_server.h \
	src/icy_metadata.h \
	src/client.h \
	src/listen.h \
	src/log.h \
	src/ls.h \
	src/main.h \
	src/mixer_all.h \
	src/mixer_api.h \
	src/mixer_control.h \
	src/mixer_list.h \
	src/event_pipe.h \
	src/mixer_plugin.h \
	src/daemon.h \
	src/normalize.h \
	src/compress.h \
	src/buffer.h \
	src/pipe.h \
	src/chunk.h \
	src/path.h \
	src/mapper.h \
	src/output/httpd_client.h \
	src/output/httpd_internal.h \
	src/page.h \
	src/pcm_buffer.h \
	src/pcm_utils.h \
	src/pcm_convert.h \
	src/pcm_volume.h \
	src/pcm_mix.h \
	src/pcm_channels.h \
	src/pcm_format.h \
	src/pcm_resample.h \
	src/pcm_resample_internal.h \
	src/pcm_dither.h \
	src/pcm_prng.h \
	src/permission.h \
	src/player_thread.h \
	src/player_control.h \
	src/playlist.h \
	src/playlist_internal.h \
	src/playlist_print.h \
	src/playlist_save.h \
	src/playlist_state.h \
	src/poison.h \
	src/riff.h \
	src/aiff.h \
	src/queue.h \
	src/queue_print.h \
	src/queue_save.h \
	src/replay_gain.h \
	src/sig_handlers.h \
	src/song.h \
	src/song_print.h \
	src/song_save.h \
	src/song_sticker.h \
	src/songvec.h \
	src/socket_util.h \
	src/state_file.h \
	src/stats.h \
	src/sticker.h \
	src/sticker_print.h \
	src/tag.h \
	src/tag_internal.h \
	src/tag_pool.h \
	src/tag_ape.h \
	src/tag_id3.h \
	src/tag_print.h \
	src/tag_save.h \
	src/strset.h \
	src/uri.h \
	src/utils.h \
	src/volume.h \
	src/zeroconf.h src/zeroconf-internal.h \
	src/locate.h \
	src/stored_playlist.h \
	src/timer.h \
	src/archive_api.h \
	src/archive_internal.h \
	src/archive_list.h \
	src/input/archive_input_plugin.h \
	src/cue/cue_tag.h

src_mpd_SOURCES = \
	$(mpd_headers) \
	$(ARCHIVE_SRC) \
	$(INPUT_SRC) \
	$(TAG_SRC) \
	$(DECODER_SRC) \
	$(ENCODER_SRC) \
	$(OUTPUT_API_SRC) $(OUTPUT_SRC) \
	$(MIXER_API_SRC) $(MIXER_SRC) \
	src/notify.c \
	src/audio.c \
	src/audio_parser.c \
	src/buffer2array.c \
	src/command.c \
	src/idle.c \
	src/cmdline.c \
	src/conf.c \
	src/crossfade.c \
	src/dbUtils.c \
	src/decoder_thread.c \
	src/decoder_control.c \
	src/decoder_api.c \
	src/decoder_internal.c \
	src/directory.c \
	src/directory_save.c \
	src/directory_print.c \
	src/database.c \
	src/dirvec.c \
	src/fifo_buffer.c \
	src/update.c \
	src/client.c \
	src/listen.c \
	src/log.c \
	src/ls.c \
	src/main.c \
	src/event_pipe.c \
	src/daemon.c \
	src/normalize.c \
	src/compress.c \
	src/buffer.c \
	src/pipe.c \
	src/chunk.c \
	src/path.c \
	src/mapper.c \
	src/page.c \
	src/pcm_convert.c \
	src/pcm_volume.c \
	src/pcm_mix.c \
	src/pcm_channels.c \
	src/pcm_format.c \
	src/pcm_resample.c \
	src/pcm_resample_fallback.c \
	src/pcm_dither.c \
	src/permission.c \
	src/player_thread.c \
	src/player_control.c \
	src/playlist.c \
	src/playlist_global.c \
	src/playlist_control.c \
	src/playlist_edit.c \
	src/playlist_print.c \
	src/playlist_save.c \
	src/playlist_state.c \
	src/queue.c \
	src/queue_print.c \
	src/queue_save.c \
	src/replay_gain.c \
	src/sig_handlers.c \
	src/song.c \
	src/song_print.c \
	src/song_save.c \
	src/songvec.c \
	src/socket_util.c \
	src/state_file.c \
	src/stats.c \
	src/tag.c \
	src/tag_pool.c \
	src/tag_print.c \
	src/tag_save.c \
	src/strset.c \
	src/uri.c \
	src/utils.c \
	src/volume.c \
	src/locate.c \
	src/stored_playlist.c \
	src/timer.c

if ENABLE_SQLITE
src_mpd_SOURCES += \
	src/sticker.c \
	src/sticker_print.c \
	src/song_sticker.c
endif

FILTER_CFLAGS = \
	$(SAMPLERATE_CFLAGS)
FILTER_LIBS = \
	$(SAMPLERATE_LIBS)

if HAVE_LIBSAMPLERATE
src_mpd_SOURCES += src/pcm_resample_libsamplerate.c
endif

# archive plugins

ARCHIVE_CFLAGS = \
	$(ISO9660_CFLAGS) \
	$(ZZIP_CFLAGS)

ARCHIVE_LIBS = \
	$(ISO9660_LIBS) \
	$(ZZIP_LIBS)

ARCHIVE_SRC =

if HAVE_BZ2
ARCHIVE_SRC += src/archive/bz2_plugin.c
endif

if HAVE_ZIP
ARCHIVE_SRC += src/archive/zip_plugin.c
endif

if HAVE_ISO
ARCHIVE_SRC += src/archive/iso_plugin.c
endif

if ENABLE_ARCHIVE
ARCHIVE_SRC += \
	src/archive_api.c \
	src/archive_list.c \
	src/input/archive_input_plugin.c
endif


# tag plugins

TAG_CFLAGS = \
	$(ID3TAG_CFLAGS)
TAG_LIBS = \
	$(ID3TAG_LIBS)

TAG_SRC = \
	src/tag_ape.c

if HAVE_ID3TAG
TAG_SRC += src/tag_id3.c \
	src/riff.c src/aiff.c
endif

# decoder plugins

DECODER_CFLAGS = \
	$(VORBIS_CFLAGS) $(TREMOR_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(AUDIOFILE_CFLAGS) \
	$(LIBMIKMOD_CFLAGS) \
	$(MODPLUG_CFLAGS) \
	$(SIDPLAY_CFLAGS) \
	$(FLUIDSYNTH_CFLAGS) \
	$(WILDMIDI_CFLAGS) \
	$(MAD_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(CUE_CFLAGS)

DECODER_LIBS = \
	$(VORBIS_LIBS) $(TREMOR_LIBS) \
	$(FLAC_LIBS) \
	$(AUDIOFILE_LIBS) $(LIBMIKMOD_LIBS) \
	$(MODPLUG_LIBS) \
	$(SIDPLAY_LIBS) \
	$(FLUIDSYNTH_LIBS) \
	$(WILDMIDI_LIBS) \
	$(MAD_LIBS) \
	$(MP4FF_LIBS) \
	$(FFMPEG_LIBS) \
	$(CUE_LIBS)

DECODER_SRC = \
	src/decoder_buffer.c \
	src/decoder_list.c

if HAVE_MAD
DECODER_SRC += src/decoder/mad_plugin.c
endif

if HAVE_MPCDEC
DECODER_SRC += src/decoder/mpcdec_plugin.c
endif

if HAVE_WAVPACK
DECODER_SRC += src/decoder/wavpack_plugin.c
endif

if HAVE_FAAD
DECODER_SRC += src/decoder/faad_plugin.c
endif

if HAVE_MP4
DECODER_SRC += src/decoder/mp4ff_plugin.c
endif

if HAVE_OGG_COMMON
DECODER_SRC += src/decoder/_ogg_common.c
endif

if HAVE_FLAC_COMMON
DECODER_SRC += src/decoder/_flac_common.c
endif

if ENABLE_VORBIS_DECODER
DECODER_SRC += src/decoder/vorbis_plugin.c
endif

if HAVE_FLAC
DECODER_SRC += src/decoder/flac_plugin.c
endif

if HAVE_OGGFLAC
DECODER_SRC += src/decoder/oggflac_plugin.c
endif

if HAVE_AUDIOFILE
DECODER_SRC += src/decoder/audiofile_plugin.c
endif

if ENABLE_MIKMOD_DECODER
DECODER_SRC += src/decoder/mikmod_plugin.c
endif

if HAVE_MODPLUG
DECODER_SRC += src/decoder/modplug_plugin.c
endif

if ENABLE_SIDPLAY
DECODER_SRC += src/decoder/sidplay_plugin.cxx
endif

if ENABLE_FLUIDSYNTH
DECODER_SRC += src/decoder/fluidsynth_plugin.c
endif

if ENABLE_WILDMIDI
DECODER_SRC += src/decoder/wildmidi_plugin.c
endif

if HAVE_FFMPEG
DECODER_SRC += src/decoder/ffmpeg_plugin.c
endif

# encoder plugins

ENCODER_CFLAGS = \
	$(LAME_CFLAGS) \
	$(VORBISENC_CFLAGS)

ENCODER_LIBS = \
	$(LAME_LIBS) \
	$(VORBISENC_LIBS)

ENCODER_SRC =

if ENABLE_ENCODER
ENCODER_SRC += src/encoder_list.c

if ENABLE_VORBIS_ENCODER
ENCODER_SRC += src/encoder/vorbis_encoder.c
endif

if ENABLE_LAME_ENCODER
ENCODER_SRC += src/encoder/lame_encoder.c
endif
endif


if HAVE_ZEROCONF
src_mpd_SOURCES += src/zeroconf.c

if HAVE_AVAHI
src_mpd_SOURCES += src/zeroconf-avahi.c
endif

if HAVE_BONJOUR
src_mpd_SOURCES += src/zeroconf-bonjour.c
endif
endif

if HAVE_CUE
DECODER_SRC += src/cue/cue_tag.c
endif

#
# input plugins
#

INPUT_CFLAGS = \
	$(CURL_CFLAGS) \
	$(MMS_CFLAGS)

INPUT_LIBS = \
	$(CURL_LIBS) \
	$(MMS_LIBS)

INPUT_SRC = \
	src/input_stream.c \
	src/input/file_input_plugin.c

if HAVE_CURL
INPUT_SRC += src/input/curl_input_plugin.c src/icy_metadata.c
endif

if ENABLE_LASTFM
INPUT_SRC += src/input/lastfm_input_plugin.c
endif

if ENABLE_MMS
INPUT_SRC += src/input/mms_input_plugin.c
endif


OUTPUT_CFLAGS = \
	$(AO_CFLAGS) \
	$(ALSA_CFLAGS) \
	$(JACK_CFLAGS) \
	$(PULSE_CFLAGS) \
	$(SHOUT_CFLAGS)

OUTPUT_LIBS = \
	$(AO_LIBS) \
	$(ALSA_LIBS) \
	$(JACK_LIBS) \
	$(PULSE_LIBS) \
	$(SHOUT_LIBS)

OUTPUT_API_SRC = \
	src/output_list.c \
	src/output_all.c \
	src/output_thread.c \
	src/output_control.c \
	src/output_state.c \
	src/output_print.c \
	src/output_command.c \
	src/output_init.c

OUTPUT_SRC = \
	src/output/null_plugin.c

MIXER_API_SRC = \
	src/mixer_control.c \
	src/mixer_all.c \
	src/mixer_api.c

MIXER_SRC =

if HAVE_ALSA
OUTPUT_SRC += src/output/alsa_plugin.c
MIXER_SRC += src/mixer/alsa_mixer.c
endif

if HAVE_AO
OUTPUT_SRC += src/output/ao_plugin.c
endif

if HAVE_FIFO
OUTPUT_SRC += src/output/fifo_plugin.c
endif

if ENABLE_PIPE_OUTPUT
OUTPUT_SRC += src/output/pipe_output_plugin.c
endif

if HAVE_JACK
OUTPUT_SRC += src/output/jack_plugin.c
endif

if HAVE_MVP
OUTPUT_SRC += src/output/mvp_plugin.c
endif

if HAVE_OSS
OUTPUT_SRC += src/output/oss_plugin.c
MIXER_SRC += src/mixer/oss_mixer.c
endif

if HAVE_OSX
OUTPUT_SRC += src/output/osx_plugin.c
endif

if HAVE_PULSE
OUTPUT_SRC += src/output/pulse_plugin.c
MIXER_SRC += src/mixer/pulse_mixer.c
endif

if HAVE_SHOUT
OUTPUT_SRC += src/output/shout_plugin.c
endif

if ENABLE_HTTPD_OUTPUT
OUTPUT_SRC += \
	src/icy_server.c \
	src/output/httpd_client.c \
	src/output/httpd_output_plugin.c
endif

if ENABLE_SOLARIS_OUTPUT
OUTPUT_SRC += src/output/solaris_output_plugin.c
endif


#
# Sparse code analysis
#
# sparse is a semantic parser
# URL: git://www.kernel.org/pub/scm/devel/sparse/sparse.git
#

SPARSE = sparse
SPARSE_FLAGS =
SPARSE_CPPFLAGS = $(DEFAULT_INCLUDES) \
	-I$(shell $(CC) -print-file-name=include) \
	-I$(shell $(CC) -print-file-name=include-fixed)
SPARSE_CPPFLAGS += -D__SCHAR_MAX__=127 -D__SHRT_MAX__=32767 \
	-D__INT_MAX__=2147483647 -D__LONG_MAX__=2147483647
sparse-check:
	$(SPARSE) -I. $(src_mpd_CFLAGS) $(src_mpd_CPPFLAGS) $(SPARSE_FLAGS) $(SPARSE_CPPFLAGS) $(filter-out %.cxx,$(src_mpd_SOURCES))

.PHONY: sparse-check


#
# Test programs
#

if ENABLE_TEST

noinst_PROGRAMS = \
	test/read_conf \
	test/run_input \
	test/run_decoder \
	test/read_tags \
	test/run_output \
	test/read_mixer \
	test/software_volume

test_read_conf_CPPFLAGS = $(AM_CPPFLAGS) \
	$(GLIB_CFLAGS)
test_read_conf_LDADD = $(MPD_LIBS) \
	$(GLIB_LIBS)
test_read_conf_SOURCES = test/read_conf.c \
	src/conf.c src/buffer2array.c src/utils.c

test_run_input_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ARCHIVE_CFLAGS) \
	$(INPUT_CFLAGS)
test_run_input_LDADD = $(MPD_LIBS) \
	$(ARCHIVE_LIBS) \
	$(INPUT_LIBS) \
	$(GLIB_LIBS)
test_run_input_SOURCES = test/run_input.c \
	src/conf.c src/buffer2array.c src/utils.c \
	src/tag.c src/tag_pool.c src/tag_save.c \
	$(ARCHIVE_SRC) \
	$(INPUT_SRC)

test_run_decoder_CPPFLAGS = $(AM_CPPFLAGS) \
	$(TAG_CFLAGS) \
	$(ARCHIVE_CFLAGS) \
	$(INPUT_CFLAGS) $(DECODER_CFLAGS)
test_run_decoder_LDADD = $(MPD_LIBS) \
	$(TAG_LIBS) \
	$(ARCHIVE_LIBS) \
	$(INPUT_LIBS) $(DECODER_LIBS) \
	$(GLIB_LIBS)
test_run_decoder_SOURCES = test/run_decoder.c \
	src/conf.c src/buffer2array.c src/utils.c src/log.c \
	src/tag.c src/tag_pool.c \
	src/replay_gain.c \
	src/uri.c \
	$(ARCHIVE_SRC) \
	$(INPUT_SRC) \
	$(TAG_SRC) \
	$(DECODER_SRC)

test_read_tags_CPPFLAGS = $(AM_CPPFLAGS) \
	$(TAG_CFLAGS) \
	$(ARCHIVE_CFLAGS) \
	$(INPUT_CFLAGS) $(DECODER_CFLAGS)
test_read_tags_LDADD = $(MPD_LIBS) \
	$(TAG_LIBS) \
	$(ARCHIVE_LIBS) \
	$(INPUT_LIBS) $(DECODER_LIBS) \
	$(GLIB_LIBS)
test_read_tags_SOURCES = test/read_tags.c \
	src/conf.c src/buffer2array.c src/utils.c src/log.c \
	src/tag.c src/tag_pool.c \
	src/replay_gain.c \
	src/uri.c \
	$(ARCHIVE_SRC) \
	$(INPUT_SRC) \
	$(TAG_SRC) \
	$(DECODER_SRC)

if ENABLE_ENCODER
noinst_PROGRAMS += test/run_encoder
test_run_encoder_SOURCES = test/run_encoder.c \
	src/conf.c src/buffer2array.c \
	src/utils.c \
	src/tag.c src/tag_pool.c \
	src/audio_parser.c \
	$(ENCODER_SRC)
test_run_encoder_LDADD = $(MPD_LIBS) \
	$(ENCODER_LIBS) \
	$(GLIB_LIBS)
endif

test_software_volume_SOURCES = test/software_volume.c \
	src/audio_parser.c \
	src/pcm_volume.c
test_software_volume_LDADD = \
	$(GLIB_LIBS)

test_run_output_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ENCODER_CFLAGS) \
	$(OUTPUT_CFLAGS)
test_run_output_LDADD = $(MPD_LIBS) \
	$(ENCODER_LIBS) \
	$(OUTPUT_LIBS) \
	$(GLIB_LIBS)
test_run_output_SOURCES = test/run_output.c \
	src/conf.c src/buffer2array.c src/utils.c src/log.c \
	src/audio_parser.c \
	src/timer.c \
	src/tag.c src/tag_pool.c \
	src/fifo_buffer.c \
	src/page.c \
	src/socket_util.c \
	src/output_init.c src/output_list.c \
	$(ENCODER_SRC) \
	src/mixer_api.c \
	src/mixer_control.c \
	$(MIXER_SRC) \
	$(OUTPUT_SRC)

test_read_mixer_CPPFLAGS = $(AM_CPPFLAGS) \
	$(OUTPUT_CFLAGS)
test_read_mixer_LDADD = $(MPD_LIBS) \
	$(OUTPUT_LIBS) \
	$(GLIB_LIBS)
test_read_mixer_SOURCES = test/read_mixer.c \
	src/conf.c src/buffer2array.c src/utils.c src/log.c \
	src/mixer_control.c src/mixer_api.c \
	$(MIXER_SRC)

endif


#
# Documentation
#

man_MANS = doc/mpd.1 doc/mpd.conf.5
doc_DATA = AUTHORS COPYING NEWS README UPGRADING doc/mpdconf.example

DOCBOOK_FILES = doc/protocol.xml doc/user.xml doc/developer.xml doc/sticker.xml

if ENABLE_DOCUMENTATION
protocoldir = $(docdir)/protocol
protocol_DATA = $(wildcard doc/protocol/*.html)

userdir = $(docdir)/user
user_DATA = $(wildcard doc/user/*.html)

developerdir = $(docdir)/developer
developer_DATA = $(wildcard doc/developer/*.html)

if HAVE_XMLTO

DOCBOOK_HTML = $(patsubst %.xml,%/index.html,$(DOCBOOK_FILES))

$(DOCBOOK_HTML): %/index.html: %.xml
	$(XMLTO) -o $(@D) --stringparam chunker.output.encoding=utf-8 html $<

else

DOCBOOK_HTML =

endif

doc/api/html/index.html: doc/doxygen.conf
	@mkdir -p $(@D)
	$(DOXYGEN) $<

all-local: $(DOCBOOK_HTML) doc/api/html/index.html

clean-local:
	rm -rf $(patsubst %.xml,%,$(DOCBOOK_FILES))
	rm -rf doc/api

install-data-local: doc/api/html/index.html
	$(mkinstalldirs) $(DESTDIR)$(docdir)/api/html
	$(INSTALL_DATA) -c -m 644 doc/api/html/*.html doc/api/html/*.css \
		doc/api/html/*.png doc/api/html/*.gif \
		$(DESTDIR)$(docdir)/api/html

upload: $(DOCBOOK_HTML) doc/api/html/index.html
	rsync -vpruz --delete doc/ cirrus@www.musicpd.org:/var/www/musicpd.org/www/doc/ \
		--chmod=Dug+rwx,Do+rx,Fug+rw,Fo+r \
		--include=protocol --include=protocol/** \
		--include=user --include=user/** \
		--include=developer --include=developer/** \
		--include=api --include=api/** \
		--exclude=*

endif


#
# Distribution
#

EXTRA_DIST = $(doc_DATA) autogen.sh \
	$(wildcard scripts/*.sh) \
	$(man_MANS) $(DOCBOOK_FILES) doc/mpdconf.example doc/doxygen.conf
