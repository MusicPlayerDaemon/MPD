bin_PROGRAMS = mpd
SUBDIRS = $(ID3_SUBDIR) $(MAD_SUBDIR) $(MP4FF_SUBDIR)

mpd_inputPlugins = inputPlugins/mp3_plugin.c inputPlugins/ogg_plugin.c

mpd_headers = buffer2array.h interface.h command.h playlist.h ls.h \
	song.h list.h directory.h tables.h utils.h path.h \
	tag.h player.h listen.h conf.h volume.h flac_decode.h \
	audio.h playerData.h stats.h myfprintf.h sig_handlers.h decode.h log.h \
	audiofile_decode.h charConv.h permission.h mpd_types.h pcm_utils.h \
	mp4_decode.h aac_decode.h signal_check.h utf8.h inputStream.h \
	outputBuffer.h replayGain.h inputStream_file.h inputStream_http.h \
	inputPlugin.h
mpd_SOURCES = main.c buffer2array.c interface.c command.c playlist.c ls.c \
	song.c list.c directory.c tables.c utils.c path.c \
	tag.c player.c listen.c conf.c volume.c flac_decode.c \
	audio.c playerData.c stats.c myfprintf.c sig_handlers.c decode.c log.c \
	audiofile_decode.c charConv.c permission.c pcm_utils.c mp4_decode.c \
	aac_decode.c signal_check.c utf8.c inputStream.c outputBuffer.c \
	replayGain.c inputStream_file.c inputStream_http.c inputPlugin.c \
	$(mpd_headers) $(mpd_inputPlugins)

mpd_CFLAGS = $(MPD_CFLAGS)
mpd_LDADD = $(MPD_LIBS) $(ID3_LIB) $(MAD_LIB) $(MP4FF_LIB)

DIST_SUBDIRS = mp4ff $(ID3_SUBDIR) $(MAD_SUBDIR)
