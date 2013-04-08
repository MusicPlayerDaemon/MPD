ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.11 dist-bzip2 dist-xz subdir-objects

AM_CPPFLAGS += -I$(srcdir)/src $(GLIB_CFLAGS)

AM_CPPFLAGS += -DSYSTEM_CONFIG_FILE_LOCATION='"$(sysconfdir)/mpd.conf"'

bin_PROGRAMS = src/mpd

noinst_LIBRARIES = \
	libutil.a \
	libpcm.a \
	libtag.a \
	libinput.a \
	libplaylist_plugins.a \
	libdecoder_plugins.a \
	libfilter_plugins.a \
	libmixer_plugins.a \
	liboutput_plugins.a

src_mpd_CPPFLAGS = $(AM_CPPFLAGS) \
	$(AVAHI_CFLAGS) \
	$(LIBWRAP_CFLAGS) \
	$(SQLITE_CFLAGS)
src_mpd_LDADD = \
	$(PLAYLIST_LIBS) \
	$(AVAHI_LIBS) \
	$(LIBWRAP_LDFLAGS) \
	$(SQLITE_LIBS) \
	$(DECODER_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	$(OUTPUT_LIBS) \
	$(FILTER_LIBS) \
	$(ENCODER_LIBS) \
	$(MIXER_LIBS) \
	libutil.a \
	$(SYSTEMD_DAEMON_LIBS) \
	$(GLIB_LIBS)

mpd_headers = \
	src/check.h \
	src/notify.h \
	src/ack.h \
	src/ape.h \
	src/audio_format.h \
	src/audio_check.h \
	src/audio_parser.h \
	src/output_internal.h \
	src/output_api.h \
	src/output_list.h \
	src/output_all.h \
	src/output_thread.h \
	src/output_control.h \
	src/output_state.h \
	src/output_print.h \
	src/output_command.h \
	src/filter_internal.h \
	src/filter_config.h \
	src/filter_plugin.h \
	src/filter_registry.h \
	src/filter/autoconvert_filter_plugin.h \
	src/filter/chain_filter_plugin.h \
	src/filter/convert_filter_plugin.h \
	src/filter/replay_gain_filter_plugin.h \
	src/filter/volume_filter_plugin.h \
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
	src/database.h \
	src/encoder_plugin.h \
	src/encoder_list.h \
	src/encoder_api.h \
	src/exclude.h \
	src/fd_util.h \
	src/glib_compat.h \
	src/update.h \
	src/inotify_source.h \
	src/inotify_queue.h \
	src/inotify_update.h \
	src/gcc.h \
	src/decoder_list.h \
	src/decoder_print.h \
	src/decoder/flac_compat.h \
	src/decoder/flac_metadata.h \
	src/decoder/flac_pcm.h \
	src/decoder/_flac_common.h \
	src/decoder/_ogg_common.h \
	src/decoder/pcm_decoder_plugin.h \
	src/input_init.h \
	src/input_plugin.h \
	src/input_registry.h \
	src/input_stream.h \
	src/input/file_input_plugin.h \
	src/input/ffmpeg_input_plugin.h \
	src/input/curl_input_plugin.h \
	src/input/rewind_input_plugin.h \
	src/input/mms_input_plugin.h \
	src/input/despotify_input_plugin.h \
	src/input/cdio_paranoia_input_plugin.h \
	src/despotify_utils.h \
	src/text_file.h \
	src/text_input_stream.h \
	src/icy_server.h \
	src/icy_metadata.h \
	src/client.h \
	src/client_internal.h \
	src/server_socket.h \
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
	src/mixer_type.h \
	src/mixer/software_mixer_plugin.h \
	src/mixer/pulse_mixer_plugin.h \
	src/daemon.h \
	src/AudioCompress/config.h \
	src/AudioCompress/compress.h \
	src/buffer.h \
	src/pipe.h \
	src/chunk.h \
	src/path.h \
	src/mapper.h \
	src/open.h \
	src/output/httpd_client.h \
	src/output/httpd_internal.h \
	src/page.h \
	src/permission.h \
	src/player_thread.h \
	src/player_control.h \
	src/playlist.h \
	src/playlist_error.h \
	src/playlist_internal.h \
	src/playlist_print.h \
	src/playlist_save.h \
	src/playlist_state.h \
	src/playlist_plugin.h \
	src/playlist_list.h \
	src/playlist_mapper.h \
	src/playlist_any.h \
	src/playlist_song.h \
	src/playlist_queue.h \
	src/playlist_vector.h \
	src/playlist_database.h \
	src/playlist/extm3u_playlist_plugin.h \
	src/playlist/m3u_playlist_plugin.h \
	src/playlist/pls_playlist_plugin.h \
	src/playlist/xspf_playlist_plugin.h \
	src/playlist/asx_playlist_plugin.h \
	src/playlist/rss_playlist_plugin.h \
	src/playlist/lastfm_playlist_plugin.h \
	src/playlist/despotify_playlist_plugin.h \
	src/playlist/cue_playlist_plugin.h \
	src/poison.h \
	src/riff.h \
	src/aiff.h \
	src/queue.h \
	src/queue_print.h \
	src/queue_save.h \
	src/refcount.h \
	src/replay_gain_config.h \
	src/replay_gain_info.h \
	src/replay_gain_ape.h \
	src/sig_handlers.h \
	src/song.h \
	src/song_print.h \
	src/song_save.h \
	src/song_sticker.h \
	src/song_sort.c src/song_sort.h \
	src/socket_util.h \
	src/state_file.h \
	src/stats.h \
	src/sticker.h \
	src/sticker_print.h \
	src/tag.h \
	src/tag_internal.h \
	src/tag_pool.h \
	src/tag_table.h \
	src/tag_ape.h \
	src/tag_id3.h \
	src/tag_rva2.h \
	src/tag_print.h \
	src/tag_save.h \
	src/tokenizer.h \
	src/strset.h \
	src/uri.h \
	src/utils.h \
	src/string_util.h \
	src/volume.h \
	src/zeroconf.h src/zeroconf-internal.h \
	src/locate.h \
	src/stored_playlist.h \
	src/timer.h \
	src/archive_api.h \
	src/archive_internal.h \
	src/archive_list.h \
	src/archive_plugin.h \
	src/archive/bz2_archive_plugin.h \
	src/archive/iso9660_archive_plugin.h \
	src/archive/zzip_archive_plugin.h \
	src/input/archive_input_plugin.h \
	src/mpd_error.h

src_mpd_SOURCES = \
	$(mpd_headers) \
	$(DECODER_SRC) \
	$(OUTPUT_API_SRC) \
	$(MIXER_API_SRC) \
	src/glib_socket.h \
	src/clock.c src/clock.h \
	src/notify.c \
	src/audio_config.c src/audio_config.h \
	src/audio_check.c \
	src/audio_format.c \
	src/audio_parser.c \
	src/protocol/argparser.c src/protocol/argparser.h \
	src/protocol/result.c src/protocol/result.h \
	src/command.c \
	src/idle.c \
	src/cmdline.c \
	src/conf.c \
	src/crossfade.c \
	src/cue/cue_parser.c src/cue/cue_parser.h \
	src/dbUtils.c \
	src/decoder_thread.c \
	src/decoder_control.c \
	src/decoder_api.c \
	src/decoder_internal.c \
	src/decoder_print.c \
	src/directory.c \
	src/directory_save.c \
	src/database.c \
	src/db_internal.h \
	src/db_error.h \
	src/db_lock.c src/db_lock.h \
	src/db_save.c src/db_save.h \
	src/db_print.c src/db_print.h \
	src/db_plugin.h \
	src/db_visitor.h \
	src/db_selection.h \
	src/db/simple_db_plugin.c src/db/simple_db_plugin.h \
	src/exclude.c \
	src/fd_util.c \
	src/fifo_buffer.c src/fifo_buffer.h \
	src/growing_fifo.c src/growing_fifo.h \
	src/filter_config.c \
	src/filter_plugin.c \
	src/filter_registry.c \
	src/update.c \
	src/update_queue.c src/update_queue.h \
	src/update_io.c src/update_io.h \
	src/update_db.c src/update_db.h \
	src/update_walk.c src/update_walk.h \
	src/update_song.c src/update_song.h \
	src/update_container.c src/update_container.h \
	src/update_internal.h \
	src/update_remove.c src/update_remove.h \
	src/client.c \
	src/client_event.c \
	src/client_expire.c \
	src/client_global.c \
	src/client_idle.h \
	src/client_idle.c \
	src/client_list.c \
	src/client_new.c \
	src/client_process.c \
	src/client_read.c \
	src/client_write.c \
	src/client_message.h \
	src/client_message.c \
	src/client_subscribe.h \
	src/client_subscribe.c \
	src/client_file.c src/client_file.h \
	src/server_socket.c \
	src/listen.c \
	src/log.c \
	src/ls.c \
	src/io_thread.c src/io_thread.h \
	src/main.c \
	src/main_win32.c \
	src/event_pipe.c \
	src/daemon.c \
	src/AudioCompress/compress.c \
	src/buffer.c \
	src/pipe.c \
	src/chunk.c \
	src/path.c \
	src/mapper.c \
	src/page.c \
	src/permission.c \
	src/player_thread.c \
	src/player_control.c \
	src/playlist.c \
	src/playlist_global.c \
	src/playlist_control.c \
	src/playlist_edit.c \
	src/playlist_print.c \
	src/playlist_save.c \
	src/playlist_mapper.c \
	src/playlist_any.c \
	src/playlist_song.c \
	src/playlist_state.c \
	src/playlist_queue.c \
	src/playlist_vector.c \
	src/playlist_database.c \
	src/queue.c \
	src/queue_print.c \
	src/queue_save.c \
	src/replay_gain_config.c \
	src/replay_gain_info.c \
	src/sig_handlers.c \
	src/song.c \
	src/song_update.c \
	src/song_print.c \
	src/song_save.c \
	src/resolver.c src/resolver.h \
	src/socket_util.c \
	src/state_file.c \
	src/stats.c \
	src/tag.c \
	src/tag_pool.c \
	src/tag_print.c \
	src/tag_save.c \
	src/tag_handler.c src/tag_handler.h \
	src/tag_file.c src/tag_file.h \
	src/tokenizer.c \
	src/text_file.c \
	src/text_input_stream.c \
	src/strset.c \
	src/uri.c \
	src/utils.c \
	src/string_util.c \
	src/volume.c \
	src/locate.c \
	src/stored_playlist.c \
	src/timer.c

#
# Windows resource file
#

src/win/mpd_win32_rc.$(OBJEXT): src/win/mpd_win32_rc.rc
	$(WINDRES) -i $< -o $@

if HAVE_WINDOWS
noinst_DATA = src/win/mpd_win32_rc.rc

src_mpd_DEPENDENCIES = src/win/mpd_win32_rc.$(OBJEXT)
src_mpd_LDFLAGS = -Wl,src/win/mpd_win32_rc.$(OBJEXT)
endif

if ENABLE_DESPOTIFY
src_mpd_SOURCES += \
	src/despotify_utils.c
endif

if ENABLE_INOTIFY
src_mpd_SOURCES += \
	src/inotify_source.c \
	src/inotify_queue.c \
	src/inotify_update.c
endif

if ENABLE_SQLITE
src_mpd_SOURCES += \
	src/sticker.c \
	src/sticker_print.c \
	src/song_sticker.c
endif

# Generic utility library

libutil_a_SOURCES = \
	src/util/list.h \
	src/util/list_sort.c src/util/list_sort.h \
	src/util/byte_reverse.c src/util/byte_reverse.h \
	src/util/bit_reverse.c src/util/bit_reverse.h

# PCM library

libpcm_a_SOURCES = \
	src/pcm_buffer.c src/pcm_buffer.h \
	src/pcm_export.c src/pcm_export.h \
	src/pcm_convert.c src/pcm_convert.h \
	src/dsd2pcm/dsd2pcm.c src/dsd2pcm/dsd2pcm.h \
	src/pcm_dsd.c src/pcm_dsd.h \
	src/pcm_dsd_usb.c src/pcm_dsd_usb.h \
	src/pcm_volume.c src/pcm_volume.h \
	src/pcm_mix.c src/pcm_mix.h \
	src/pcm_channels.c src/pcm_channels.h \
	src/pcm_pack.c src/pcm_pack.h \
	src/pcm_format.c src/pcm_format.h \
	src/pcm_resample.c src/pcm_resample.h \
	src/pcm_resample_fallback.c \
	src/pcm_resample_internal.h \
	src/pcm_dither.c src/pcm_dither.h \
	src/pcm_prng.h \
	src/pcm_utils.h
libpcm_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(SAMPLERATE_CFLAGS)

PCM_LIBS = \
	libpcm.a \
	$(SAMPLERATE_LIBS)

if HAVE_LIBSAMPLERATE
libpcm_a_SOURCES += src/pcm_resample_libsamplerate.c
endif

# archive plugins

if ENABLE_ARCHIVE

noinst_LIBRARIES += libarchive.a

src_mpd_SOURCES +=  \
	src/update_archive.c src/update_archive.h

libarchive_a_SOURCES = \
	src/archive_api.c \
	src/archive_list.c \
	src/archive_plugin.c \
	src/input/archive_input_plugin.c
libarchive_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(BZ2_CFLAGS) \
	$(ISO9660_CFLAGS) \
	$(ZZIP_CFLAGS)

ARCHIVE_LIBS = \
	libarchive.a \
	$(BZ2_LIBS) \
	$(ISO9660_LIBS) \
	$(ZZIP_LIBS)

if HAVE_BZ2
libarchive_a_SOURCES += src/archive/bz2_archive_plugin.c
endif

if HAVE_ZZIP
libarchive_a_SOURCES += src/archive/zzip_archive_plugin.c
endif

if HAVE_ISO9660
libarchive_a_SOURCES += src/archive/iso9660_archive_plugin.c
endif

else
ARCHIVE_LIBS =
endif


# tag plugins

libtag_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ID3TAG_CFLAGS)
TAG_LIBS = \
	libtag.a \
	$(ID3TAG_LIBS)

libtag_a_SOURCES =\
	src/ape.c \
	src/replay_gain_ape.c \
	src/tag_ape.c

if HAVE_ID3TAG
libtag_a_SOURCES += \
	src/tag_id3.c \
	src/tag_rva2.c \
	src/riff.c src/aiff.c
endif

# decoder plugins

libdecoder_plugins_a_SOURCES = \
	src/decoder/pcm_decoder_plugin.c \
	src/decoder/dsdiff_decoder_plugin.c \
	src/decoder/dsdiff_decoder_plugin.h \
	src/decoder/dsf_decoder_plugin.c \
	src/decoder/dsf_decoder_plugin.h \
	src/decoder/dsdlib.c \
	src/decoder/dsdlib.h \
	src/decoder_buffer.c \
	src/decoder_plugin.c \
	src/decoder_list.c
libdecoder_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(VORBIS_CFLAGS) $(TREMOR_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(SNDFILE_CFLAGS) \
	$(AUDIOFILE_CFLAGS) \
	$(LIBMIKMOD_CFLAGS) \
	$(GME_CFLAGS) \
	$(SIDPLAY_CFLAGS) \
	$(FLUIDSYNTH_CFLAGS) \
	$(WILDMIDI_CFLAGS) \
	$(WAVPACK_CFLAGS) \
	$(MAD_CFLAGS) \
	$(MPG123_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(MPCDEC_CFLAGS) \
	$(FAAD_CFLAGS)

DECODER_LIBS = \
	libdecoder_plugins.a \
	$(VORBIS_LIBS) $(TREMOR_LIBS) \
	$(FLAC_LIBS) \
	$(SNDFILE_LIBS) \
	$(AUDIOFILE_LIBS) $(LIBMIKMOD_LIBS) \
	$(GME_LIBS) \
	$(SIDPLAY_LIBS) \
	$(FLUIDSYNTH_LIBS) \
	$(WILDMIDI_LIBS) \
	$(WAVPACK_LIBS) \
	$(MAD_LIBS) \
	$(MPG123_LIBS) \
	$(MP4FF_LIBS) \
	$(FFMPEG_LIBS) \
	$(MPCDEC_LIBS) \
	$(FAAD_LIBS)

DECODER_SRC =

if HAVE_MAD
libdecoder_plugins_a_SOURCES += src/decoder/mad_decoder_plugin.c
endif

if HAVE_MPG123
libdecoder_plugins_a_SOURCES += src/decoder/mpg123_decoder_plugin.c
endif

if HAVE_MPCDEC
libdecoder_plugins_a_SOURCES += src/decoder/mpcdec_decoder_plugin.c
endif

if HAVE_WAVPACK
libdecoder_plugins_a_SOURCES += src/decoder/wavpack_decoder_plugin.c
endif

if HAVE_FAAD
libdecoder_plugins_a_SOURCES += src/decoder/faad_decoder_plugin.c
endif

if HAVE_MP4
libdecoder_plugins_a_SOURCES += src/decoder/mp4ff_decoder_plugin.c
endif

if HAVE_OGG_COMMON
libdecoder_plugins_a_SOURCES += src/decoder/_ogg_common.c
endif

if HAVE_FLAC_COMMON
libdecoder_plugins_a_SOURCES += \
	src/decoder/flac_metadata.c \
	src/decoder/flac_pcm.c \
	src/decoder/_flac_common.c
endif

if ENABLE_VORBIS_DECODER
libdecoder_plugins_a_SOURCES += \
	src/decoder/vorbis_comments.c \
	src/decoder/vorbis_comments.h \
	src/decoder/vorbis_decoder_plugin.c
endif

if HAVE_FLAC
libdecoder_plugins_a_SOURCES += src/decoder/flac_decoder_plugin.c
endif

if HAVE_AUDIOFILE
libdecoder_plugins_a_SOURCES += src/decoder/audiofile_decoder_plugin.c
endif

if ENABLE_MIKMOD_DECODER
libdecoder_plugins_a_SOURCES += src/decoder/mikmod_decoder_plugin.c
endif

if HAVE_MODPLUG
libmodplug_decoder_plugin_a_SOURCES = src/decoder/modplug_decoder_plugin.c
libmodplug_decoder_plugin_a_CFLAGS = $(src_mpd_CFLAGS) $(MODPLUG_CFLAGS)
libmodplug_decoder_plugin_a_CPPFLAGS = $(src_mpd_CPPFLAGS)
noinst_LIBRARIES += libmodplug_decoder_plugin.a
DECODER_LIBS += libmodplug_decoder_plugin.a $(MODPLUG_LIBS)
endif

if ENABLE_SIDPLAY
libdecoder_plugins_a_SOURCES += src/decoder/sidplay_decoder_plugin.cxx
DECODER_SRC += src/dummy.cxx
endif

if ENABLE_FLUIDSYNTH
libdecoder_plugins_a_SOURCES += src/decoder/fluidsynth_decoder_plugin.c
endif

if ENABLE_WILDMIDI
libdecoder_plugins_a_SOURCES += src/decoder/wildmidi_decoder_plugin.c
endif

if HAVE_FFMPEG
libdecoder_plugins_a_SOURCES += \
	src/decoder/ffmpeg_metadata.c \
	src/decoder/ffmpeg_metadata.h \
	src/decoder/ffmpeg_decoder_plugin.c
endif

if ENABLE_SNDFILE
libdecoder_plugins_a_SOURCES += src/decoder/sndfile_decoder_plugin.c
endif

if HAVE_GME
libdecoder_plugins_a_SOURCES += src/decoder/gme_decoder_plugin.c
endif

# encoder plugins

if ENABLE_ENCODER

noinst_LIBRARIES += libencoder_plugins.a

libencoder_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LAME_CFLAGS) \
	$(TWOLAME_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(VORBISENC_CFLAGS)

ENCODER_LIBS = \
	libencoder_plugins.a \
	$(LAME_LIBS) \
	$(TWOLAME_LIBS) \
	$(FLAC_LIBS) \
	$(VORBISENC_LIBS)

libencoder_plugins_a_SOURCES =

libencoder_plugins_a_SOURCES += src/encoder_list.c
libencoder_plugins_a_SOURCES += src/encoder/null_encoder.c

if ENABLE_WAVE_ENCODER
libencoder_plugins_a_SOURCES += src/encoder/wave_encoder.c
endif

if ENABLE_VORBIS_ENCODER
libencoder_plugins_a_SOURCES += src/encoder/vorbis_encoder.c
endif

if ENABLE_LAME_ENCODER
libencoder_plugins_a_SOURCES += src/encoder/lame_encoder.c
endif

if ENABLE_TWOLAME_ENCODER
libencoder_plugins_a_SOURCES += src/encoder/twolame_encoder.c
endif

if ENABLE_FLAC_ENCODER
libencoder_plugins_a_SOURCES += src/encoder/flac_encoder.c
endif

else
ENCODER_LIBS =
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

#
# input plugins
#

libinput_a_SOURCES = \
	src/input_init.c \
	src/input_registry.c \
	src/input_stream.c \
	src/input_internal.c src/input_internal.h \
	src/input/rewind_input_plugin.c \
	src/input/file_input_plugin.c

libinput_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(CURL_CFLAGS) \
	$(SOUP_CFLAGS) \
	$(CDIO_PARANOIA_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(DESPOTIFY_CFLAGS) \
	$(MMS_CFLAGS)

INPUT_LIBS = \
	libinput.a \
	$(CURL_LIBS) \
	$(SOUP_LIBS) \
	$(CDIO_PARANOIA_LIBS) \
	$(FFMPEG_LIBS) \
	$(DESPOTIFY_LIBS) \
	$(MMS_LIBS)

if ENABLE_CURL
libinput_a_SOURCES += src/input/curl_input_plugin.c \
	src/icy_metadata.c
endif

if ENABLE_SOUP
libinput_a_SOURCES += \
	src/input/soup_input_plugin.c \
	src/input/soup_input_plugin.h
endif

if ENABLE_CDIO_PARANOIA
libinput_a_SOURCES += src/input/cdio_paranoia_input_plugin.c
endif

if HAVE_FFMPEG
libinput_a_SOURCES += src/input/ffmpeg_input_plugin.c
endif

if ENABLE_MMS
libinput_a_SOURCES += src/input/mms_input_plugin.c
endif

if ENABLE_DESPOTIFY
libinput_a_SOURCES += src/input/despotify_input_plugin.c
endif


liboutput_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(AO_CFLAGS) \
	$(ALSA_CFLAGS) \
	$(FFADO_CFLAGS) \
	$(JACK_CFLAGS) \
	$(OPENAL_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(PULSE_CFLAGS) \
	$(SHOUT_CFLAGS)

OUTPUT_LIBS = \
	liboutput_plugins.a \
	$(LIBWRAP_LDFLAGS) \
	$(AO_LIBS) \
	$(ALSA_LIBS) \
	$(ROAR_LIBS) \
	$(FFADO_LIBS) \
	$(JACK_LIBS) \
	$(OPENAL_LIBS) \
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
	src/output_plugin.c src/output_plugin.h \
	src/output_finish.c \
	src/output_init.c

liboutput_plugins_a_SOURCES = \
	src/output/null_output_plugin.h \
	src/output/null_output_plugin.c

MIXER_LIBS = \
	libmixer_plugins.a \
	$(PULSE_LIBS)

MIXER_API_SRC = \
	src/mixer_control.c \
	src/mixer_type.c \
	src/mixer_all.c \
	src/mixer_api.c

libmixer_plugins_a_SOURCES = \
	src/mixer/software_mixer_plugin.c
libmixer_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ALSA_CFLAGS) \
	$(PULSE_CFLAGS)

if HAVE_ALSA
liboutput_plugins_a_SOURCES += \
	src/output/alsa_output_plugin.c src/output/alsa_output_plugin.h
libmixer_plugins_a_SOURCES += src/mixer/alsa_mixer_plugin.c
endif

if HAVE_ROAR
liboutput_plugins_a_SOURCES += \
	src/output/roar_output_plugin.c src/output/roar_output_plugin.h
libmixer_plugins_a_SOURCES += src/mixer/roar_mixer_plugin.c
endif

if ENABLE_FFADO_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/ffado_output_plugin.c src/output/ffado_output_plugin.h
endif

if HAVE_AO
liboutput_plugins_a_SOURCES += \
	src/output/ao_output_plugin.c src/output/ao_output_plugin.h
endif

if HAVE_FIFO
liboutput_plugins_a_SOURCES += \
	src/output/fifo_output_plugin.c src/output/fifo_output_plugin.h
endif

if ENABLE_PIPE_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/pipe_output_plugin.c src/output/pipe_output_plugin.h
endif

if HAVE_JACK
liboutput_plugins_a_SOURCES += \
	src/output/jack_output_plugin.c src/output/jack_output_plugin.h
endif

if HAVE_MVP
liboutput_plugins_a_SOURCES += \
	src/output/mvp_output_plugin.c src/output/mvp_output_plugin.h
endif

if HAVE_OSS
liboutput_plugins_a_SOURCES += \
	src/output/oss_output_plugin.c src/output/oss_output_plugin.h
libmixer_plugins_a_SOURCES += src/mixer/oss_mixer_plugin.c
endif

if HAVE_OPENAL
liboutput_plugins_a_SOURCES += \
	src/output/openal_output_plugin.c src/output/openal_output_plugin.h
endif

if HAVE_OSX
liboutput_plugins_a_SOURCES += \
	src/output/osx_output_plugin.c src/output/osx_output_plugin.h
endif

if HAVE_PULSE
liboutput_plugins_a_SOURCES += \
	src/output/pulse_output_plugin.c src/output/pulse_output_plugin.h
libmixer_plugins_a_SOURCES += src/mixer/pulse_mixer_plugin.c
endif

if HAVE_SHOUT
liboutput_plugins_a_SOURCES += \
	src/output/shout_output_plugin.c src/output/shout_output_plugin.h
endif

if ENABLE_RECORDER_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/recorder_output_plugin.c src/output/recorder_output_plugin.h
endif

if ENABLE_HTTPD_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/icy_server.c \
	src/output/httpd_client.c \
	src/output/httpd_output_plugin.c src/output/httpd_output_plugin.h
endif

if ENABLE_SOLARIS_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/solaris_output_plugin.c src/output/solaris_output_plugin.h
endif

if ENABLE_WINMM_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/winmm_output_plugin.c src/output/winmm_output_plugin.h
libmixer_plugins_a_SOURCES += src/mixer/winmm_mixer_plugin.c
endif


#
# Playlist plugins
#

libplaylist_plugins_a_SOURCES = \
	src/playlist/extm3u_playlist_plugin.c \
	src/playlist/m3u_playlist_plugin.c \
	src/playlist/pls_playlist_plugin.c \
	src/playlist/xspf_playlist_plugin.c \
	src/playlist/asx_playlist_plugin.c \
	src/playlist/rss_playlist_plugin.c \
	src/playlist/cue_playlist_plugin.c \
	src/playlist/embcue_playlist_plugin.c \
	src/playlist/embcue_playlist_plugin.h \
	src/playlist_list.c
libplaylist_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(YAJL_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS))

PLAYLIST_LIBS = \
	libplaylist_plugins.a \
	$(FLAC_LIBS)

if ENABLE_LASTFM
libplaylist_plugins_a_SOURCES += src/playlist/lastfm_playlist_plugin.c
endif

if ENABLE_DESPOTIFY
libplaylist_plugins_a_SOURCES += src/playlist/despotify_playlist_plugin.c
endif

if ENABLE_SOUNDCLOUD
libplaylist_plugins_a_SOURCES += \
	src/playlist/soundcloud_playlist_plugin.h \
	src/playlist/soundcloud_playlist_plugin.c
PLAYLIST_LIBS += $(YAJL_LIBS)
endif

#
# Filter plugins
#

libfilter_plugins_a_SOURCES = \
	src/filter/null_filter_plugin.c \
	src/filter/chain_filter_plugin.c \
	src/filter/autoconvert_filter_plugin.c \
	src/filter/convert_filter_plugin.c \
	src/filter/route_filter_plugin.c \
	src/filter/normalize_filter_plugin.c \
	src/filter/replay_gain_filter_plugin.c \
	src/filter/volume_filter_plugin.c

FILTER_LIBS = \
	libfilter_plugins.a \
	$(PCM_LIBS)


#
# systemd unit
#

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	mpd.service
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
SPARSE_SRC = $(addprefix $(top_srcdir)/,$(filter %.c,$(src_mpd_SOURCES)))
sparse-check:
	$(SPARSE) -I. $(src_mpd_CFLAGS) $(src_mpd_CPPFLAGS) $(SPARSE_FLAGS) $(SPARSE_CPPFLAGS) $(SPARSE_SRC)

.PHONY: sparse-check


#
# Test programs
#

if ENABLE_TEST

C_TESTS = \
	test/test_byte_reverse \
	test/test_pcm \
	test/test_queue_priority

TESTS = $(C_TESTS)

noinst_PROGRAMS = \
	$(C_TESTS) \
	test/read_conf \
	test/run_resolver \
	test/run_input \
	test/dump_text_file \
	test/dump_playlist \
	test/run_decoder \
	test/read_tags \
	test/run_filter \
	test/run_output \
	test/run_convert \
	test/run_normalize \
	test/software_volume

if HAVE_ID3TAG
noinst_PROGRAMS += test/dump_rva2
endif

if HAVE_ALSA
# this debug program is still ALSA specific
noinst_PROGRAMS += test/read_mixer
endif

test_read_conf_LDADD = \
	$(GLIB_LIBS)
test_read_conf_SOURCES = test/read_conf.c \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c

test_run_resolver_LDADD = \
	$(GLIB_LIBS)
test_run_resolver_SOURCES = test/run_resolver.c \
	src/resolver.c

test_run_input_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(GLIB_LIBS)
test_run_input_SOURCES = test/run_input.c \
	test/stdbin.h \
	src/io_thread.c src/io_thread.h \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c\
	src/tag.c src/tag_pool.c src/tag_save.c \
	src/fd_util.c

test_dump_text_file_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(GLIB_LIBS)
test_dump_text_file_SOURCES = test/dump_text_file.c \
	test/stdbin.h \
	src/io_thread.c src/io_thread.h \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c\
	src/tag.c src/tag_pool.c \
	src/text_input_stream.c src/fifo_buffer.c \
	src/fd_util.c

test_dump_playlist_LDADD = \
	$(PLAYLIST_LIBS) \
	$(FLAC_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(DECODER_LIBS) \
	$(TAG_LIBS) \
	libutil.a \
	$(GLIB_LIBS)
test_dump_playlist_SOURCES = test/dump_playlist.c \
	$(DECODER_SRC) \
	src/io_thread.c src/io_thread.h \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c\
	src/uri.c \
	src/song.c src/tag.c src/tag_pool.c src/tag_save.c \
	src/tag_handler.c src/tag_file.c \
	src/audio_check.c src/pcm_buffer.c \
	src/text_input_stream.c src/fifo_buffer.c \
	src/cue/cue_parser.c src/cue/cue_parser.h \
	src/fd_util.c

if HAVE_FLAC
test_dump_playlist_SOURCES += \
	src/replay_gain_info.c \
	src/decoder/flac_metadata.c
endif

test_run_decoder_LDADD = \
	$(DECODER_LIBS) \
	libpcm.a \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libutil.a \
	$(GLIB_LIBS)
test_run_decoder_SOURCES = test/run_decoder.c \
	test/stdbin.h \
	src/io_thread.c src/io_thread.h \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c src/log.c \
	src/tag.c src/tag_pool.c src/tag_handler.c \
	src/replay_gain_info.c \
	src/uri.c \
	src/fd_util.c \
	src/audio_check.c \
	src/audio_format.c \
	$(ARCHIVE_SRC) \
	$(INPUT_SRC) \
	$(TAG_SRC) \
	$(DECODER_SRC)

test_read_tags_LDADD = \
	$(DECODER_LIBS) \
	libpcm.a \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libutil.a \
	$(GLIB_LIBS)
test_read_tags_SOURCES = test/read_tags.c \
	src/io_thread.c src/io_thread.h \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c src/log.c \
	src/tag.c src/tag_pool.c src/tag_handler.c \
	src/replay_gain_info.c \
	src/uri.c \
	src/fd_util.c \
	src/audio_check.c \
	$(DECODER_SRC)

if HAVE_ID3TAG
test_dump_rva2_LDADD = \
	$(ID3TAG_LIBS) \
	$(GLIB_LIBS)
test_dump_rva2_SOURCES = test/dump_rva2.c \
	src/riff.c src/aiff.c \
	src/tag_handler.c \
	src/tag_id3.c \
	src/tag_rva2.c
endif

test_run_filter_LDADD = \
	$(FILTER_LIBS) \
	$(GLIB_LIBS)
test_run_filter_SOURCES = test/run_filter.c \
	test/stdbin.h \
	src/filter_plugin.c \
	src/filter_registry.c \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c \
	src/audio_check.c \
	src/audio_format.c \
	src/audio_parser.c \
	src/replay_gain_config.c \
	src/replay_gain_info.c \
	src/AudioCompress/compress.c

if ENABLE_DESPOTIFY
test_read_tags_SOURCES += \
	src/despotify_utils.c
test_run_input_SOURCES += \
	src/despotify_utils.c
test_dump_text_file_SOURCES += \
	src/despotify_utils.c
test_dump_playlist_SOURCES += \
	src/despotify_utils.c
test_run_decoder_SOURCES += \
	src/despotify_utils.c
endif

if ENABLE_ENCODER
noinst_PROGRAMS += test/run_encoder
test_run_encoder_SOURCES = test/run_encoder.c \
	test/stdbin.h \
	src/fifo_buffer.c src/growing_fifo.c \
	src/conf.c src/tokenizer.c \
	src/utils.c src/string_util.c \
	src/tag.c src/tag_pool.c \
	src/audio_check.c \
	src/audio_format.c \
	src/audio_parser.c
test_run_encoder_LDADD = \
	$(ENCODER_LIBS) \
	libpcm.a \
	$(TAG_LIBS) \
	$(GLIB_LIBS)
endif

if ENABLE_VORBIS_ENCODER
noinst_PROGRAMS += test/test_vorbis_encoder
test_test_vorbis_encoder_SOURCES = test/test_vorbis_encoder.c \
	test/stdbin.h \
	src/conf.c src/tokenizer.c \
	src/utils.c \
	src/string_util.c \
	src/tag.c src/tag_pool.c \
	src/audio_check.c \
	src/audio_format.c \
	src/audio_parser.c \
	src/pcm_buffer.c \
	src/fifo_buffer.c src/growing_fifo.c \
	$(ENCODER_SRC)
test_test_vorbis_encoder_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ENCODER_CFLAGS)
test_test_vorbis_encoder_LDADD = $(MPD_LIBS) \
	$(ENCODER_LIBS) \
	$(GLIB_LIBS)
endif

test_software_volume_SOURCES = test/software_volume.c \
	test/stdbin.h \
	src/audio_check.c \
	src/audio_parser.c
test_software_volume_LDADD = \
	$(PCM_LIBS) \
	$(GLIB_LIBS)

test_run_normalize_SOURCES = test/run_normalize.c \
	test/stdbin.h \
	src/audio_check.c \
	src/audio_parser.c \
	src/AudioCompress/compress.c
test_run_normalize_LDADD = \
	$(GLIB_LIBS)

test_run_convert_SOURCES = test/run_convert.c \
	src/dsd2pcm/dsd2pcm.c \
	src/fifo_buffer.c \
	src/audio_format.c \
	src/audio_check.c \
	src/audio_parser.c
test_run_convert_LDADD = \
	$(PCM_LIBS) \
	libutil.a \
	$(GLIB_LIBS)

test_run_output_LDADD = $(MPD_LIBS) \
	$(OUTPUT_LIBS) \
	$(ENCODER_LIBS) \
	libmixer_plugins.a \
	$(FILTER_LIBS) \
	libutil.a \
	$(GLIB_LIBS)
test_run_output_SOURCES = test/run_output.c \
	test/stdbin.h \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c src/log.c \
	src/io_thread.c src/io_thread.h \
	src/audio_check.c \
	src/audio_format.c \
	src/audio_parser.c \
	src/timer.c src/clock.c \
	src/tag.c src/tag_pool.c \
	src/fifo_buffer.c src/growing_fifo.c \
	src/page.c \
	src/socket_util.c \
	src/resolver.c \
	src/output_init.c src/output_finish.c src/output_list.c \
	src/output_plugin.c \
	src/mixer_api.c \
	src/mixer_control.c \
	src/mixer_type.c \
	src/filter_plugin.c \
	src/filter_config.c \
	src/AudioCompress/compress.c \
	src/replay_gain_info.c \
	src/replay_gain_config.c \
	src/fd_util.c \
	src/server_socket.c

test_read_mixer_LDADD = \
	libpcm.a \
	libmixer_plugins.a \
	$(OUTPUT_LIBS) \
	$(GLIB_LIBS)
test_read_mixer_SOURCES = test/read_mixer.c \
	src/conf.c src/tokenizer.c src/utils.c src/string_util.c src/log.c \
	src/mixer_control.c src/mixer_api.c \
	src/filter_plugin.c \
	src/filter/volume_filter_plugin.c \
	src/fd_util.c

if ENABLE_BZIP2_TEST
TESTS += test/test_archive_bzip2.sh
endif

if ENABLE_ZZIP_TEST
TESTS += test/test_archive_zzip.sh
endif

if ENABLE_ISO9660_TEST
TESTS += test/test_archive_iso9660.sh
endif

if ENABLE_INOTIFY
noinst_PROGRAMS += test/run_inotify
test_run_inotify_SOURCES = test/run_inotify.c \
	src/fd_util.c \
	src/fifo_buffer.c \
	src/inotify_source.c
test_run_inotify_LDADD = $(GLIB_LIBS)
endif

test_test_byte_reverse_SOURCES = \
	test/test_byte_reverse.c
test_test_byte_reverse_LDADD = \
	libutil.a \
	$(GLIB_LIBS)

test_test_pcm_SOURCES = \
	test/test_pcm_dither.c \
	test/test_pcm_pack.c \
	test/test_pcm_channels.c \
	test/test_pcm_volume.c \
	test/test_pcm_all.h \
	test/test_pcm_main.c
test_test_pcm_LDADD = \
	$(PCM_LIBS) \
	libutil.a \
	$(GLIB_LIBS)

test_test_queue_priority_SOURCES = \
	src/queue.c \
	test/test_queue_priority.c
test_test_queue_priority_LDADD = \
	$(GLIB_LIBS)

if HAVE_CXX
noinst_PROGRAMS += src/dsd2pcm/dsd2pcm

src_dsd2pcm_dsd2pcm_SOURCES = \
	src/dsd2pcm/dsd2pcm.c src/dsd2pcm/dsd2pcm.h \
	src/dsd2pcm/noiseshape.c src/dsd2pcm/noiseshape.h \
	src/dsd2pcm/main.cpp
src_dsd2pcm_dsd2pcm_LDADD = libutil.a
endif

endif


#
# Documentation
#

man_MANS = doc/mpd.1 doc/mpd.conf.5
doc_DATA = AUTHORS COPYING NEWS README UPGRADING doc/mpdconf.example

DOCBOOK_FILES = doc/protocol.xml doc/user.xml doc/developer.xml

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
	$(INSTALL_DATA) -c -m 644 doc/api/html/*.* \
		$(DESTDIR)$(docdir)/api/html

uninstall-local:
	rm -f $(DESTDIR)$(docdir)/api/html/*.*

upload: $(DOCBOOK_HTML) doc/api/html/index.html
	rsync -vpruz --delete doc/ www.musicpd.org:/var/www/doc/ \
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
	$(man_MANS) $(DOCBOOK_FILES) doc/mpdconf.example doc/doxygen.conf \
	src/win/mpd_win32_rc.rc.in src/win/mpd.ico
