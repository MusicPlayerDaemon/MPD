bin_PROGRAMS = mpd
SUBDIRS = $(MP4FF_SUBDIR)

mpd_audioOutputs = \
	audioOutputs/audioOutput_alsa.c \
	audioOutputs/audioOutput_ao.c \
	audioOutputs/audioOutput_oss.c \
	audioOutputs/audioOutput_osx.c \
	audioOutputs/audioOutput_pulse.c \
	audioOutputs/audioOutput_mvp.c \
	audioOutputs/audioOutput_shout.c \
	audioOutputs/audioOutput_jack.c

mpd_inputPlugins = \
	inputPlugins/_flac_common.c \
	inputPlugins/_ogg_common.c \
	inputPlugins/oggflac_plugin.c \
	inputPlugins/oggvorbis_plugin.c \
	inputPlugins/aac_plugin.c \
	inputPlugins/audiofile_plugin.c \
	inputPlugins/flac_plugin.c \
	inputPlugins/mod_plugin.c \
	inputPlugins/mp3_plugin.c \
	inputPlugins/mp4_plugin.c \
	inputPlugins/mpc_plugin.c


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
	gcc.h \
	inputPlugin.h \
	inputPlugins/_flac_common.h \
	inputPlugins/_ogg_common.h \
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
	normalize.h \
	compress.h \
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
	sllist.h \
	song.h \
	state_file.h \
	stats.h \
	tag.h \
	tagTracker.h \
	tree.h \
	utf8.h \
	utils.h \
	volume.h \
	ioops.h \
	zeroconf.h \
	locate.h \
	storedPlaylist.h


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
	normalize.c \
	compress.c \
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
	sllist.c \
	song.c \
	state_file.c \
	stats.c \
	tag.c \
	tagTracker.c \
	tree.c \
	utils.c \
	volume.c \
	utf8.c \
	zeroconf.c \
	locate.c \
	storedPlaylist.c


mpd_CFLAGS = $(MPD_CFLAGS)
mpd_LDADD = $(MPD_LIBS) $(MP4FF_LIB)

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

