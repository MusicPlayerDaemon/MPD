bin_PROGRAMS = mpd
SUBDIRS = $(ID3_SUBDIR) $(MAD_SUBDIR) $(MP4FF_SUBDIR)

mpd_audioOutputs = \
	audioOutputs/audioOutput_ao.c \
	audioOutputs/audioOutput_oss.c \
	audioOutputs/audioOutput_shout.c

mpd_inputPlugins = \
	inputPlugins/aac_plugin.c \
	inputPlugins/audiofile_plugin.c \
	inputPlugins/flac_plugin.c \
	inputPlugins/mod_plugin.c \
	inputPlugins/mp3_plugin.c \
	inputPlugins/mp4_plugin.c \
	inputPlugins/ogg_plugin.c


mpd_headers = \
	ack.h \
	audio.h \
	audioOutput.h \
	buffer2array.h \
	charConv.h \
	command.h \
	conf.h \
	dbUtils.h \
	decode.h \
	directory.h \
	inputPlugin.h \
	inputStream.h \
	inputStream_file.h \
	inputStream_http.h \
	interface.h \
	list.h \
	listen.h \
	log.h \
	ls.h \
	metadataChunk.h \
	mpd_types.h \
	myfprintf.h \
	outputBuffer.h \
	path.h \
	pcm_utils.h \
	permission.h \
	player.h \
	playerData.h \
	playlist.h \
	replayGain.h \
	signal_check.h \
	sig_handlers.h \
	song.h \
	stats.h \
	tag.h \
	tagTracker.h \
	utf8.h \
	utils.h \
	volume.h


mpd_SOURCES = \
	$(mpd_headers) \
	$(mpd_audioOutputs) \
	$(mpd_inputPlugins) \
	audio.c \
	audioOutput.c \
	buffer2array.c \
	charConv.c \
	command.c \
	conf.c \
	dbUtils.c \
	decode.c \
	directory.c \
	inputPlugin.c \
	inputStream.c \
	inputStream_file.c \
	inputStream_http.c \
	interface.c \
	list.c \
	listen.c \
	log.c \
	ls.c \
	main.c \
	metadataChunk.c \
	myfprintf.c \
	outputBuffer.c \
	path.c \
	pcm_utils.c \
	permission.c \
	player.c \
	playerData.c \
	playlist.c \
	replayGain.c \
	sig_handlers.c \
	signal_check.c \
	song.c \
	stats.c \
	tag.c \
	tagTracker.c \
	utils.c \
	volume.c \
	utf8.c


mpd_CFLAGS = $(MPD_CFLAGS)
mpd_LDADD = $(MPD_LIBS) $(ID3_LIB) $(MAD_LIB) $(MP4FF_LIB)

DIST_SUBDIRS = mp4ff $(ID3_SUBDIR) $(MAD_SUBDIR)
