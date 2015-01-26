ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.11 dist-xz subdir-objects

AM_CPPFLAGS += -I$(srcdir)/src $(GLIB_CFLAGS)

AM_CPPFLAGS += -DSYSTEM_CONFIG_FILE_LOCATION='"$(sysconfdir)/mpd.conf"'

bin_PROGRAMS = src/mpd

noinst_LIBRARIES = \
	libutil.a \
	libthread.a \
	libsystem.a \
	libevent.a \
	libpcm.a \
	libconf.a \
	libtag.a \
	libinput.a \
	libfs.a \
	libdb_plugins.a \
	libplaylist_plugins.a \
	libdecoder_plugins.a \
	libfilter_plugins.a \
	libmixer_plugins.a \
	liboutput_plugins.a

src_mpd_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBMPDCLIENT_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(LIBWRAP_CFLAGS) \
	$(SQLITE_CFLAGS)
src_mpd_LDADD = \
	$(DB_LIBS) \
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
	libconf.a \
	libevent.a \
	libthread.a \
	libsystem.a \
	libutil.a \
	libfs.a \
	$(SYSTEMD_DAEMON_LIBS) \
	$(GLIB_LIBS)

mpd_headers = \
	src/check.h \
	src/gerror.h \
	src/TextInputStream.hxx \
	src/AudioCompress/config.h \
	src/AudioCompress/compress.h \
	src/open.h \
	src/Playlist.hxx \
	src/poison.h \
	src/TimePrint.cxx src/TimePrint.hxx \
	src/Timer.hxx

src_mpd_SOURCES = \
	$(mpd_headers) \
	$(DECODER_SRC) \
	$(OUTPUT_API_SRC) \
	$(MIXER_API_SRC) \
	src/Compiler.h \
	src/notify.cxx src/notify.hxx \
	src/AudioConfig.cxx src/AudioConfig.hxx \
	src/CheckAudioFormat.cxx src/CheckAudioFormat.hxx \
	src/AudioFormat.cxx src/AudioFormat.hxx \
	src/AudioParser.cxx src/AudioParser.hxx \
	src/protocol/Ack.cxx src/protocol/Ack.hxx \
	src/protocol/ArgParser.cxx src/protocol/ArgParser.hxx \
	src/protocol/Result.cxx src/protocol/Result.hxx \
	src/command/CommandResult.hxx \
	src/command/CommandError.cxx src/command/CommandError.hxx \
	src/command/AllCommands.cxx src/command/AllCommands.hxx \
	src/command/QueueCommands.cxx src/command/QueueCommands.hxx \
	src/command/PlayerCommands.cxx src/command/PlayerCommands.hxx \
	src/command/PlaylistCommands.cxx src/command/PlaylistCommands.hxx \
	src/command/DatabaseCommands.cxx src/command/DatabaseCommands.hxx \
	src/command/FileCommands.cxx src/command/FileCommands.hxx \
	src/command/OutputCommands.cxx src/command/OutputCommands.hxx \
	src/command/MessageCommands.cxx src/command/MessageCommands.hxx \
	src/command/OtherCommands.cxx src/command/OtherCommands.hxx \
	src/command/CommandListBuilder.cxx src/command/CommandListBuilder.hxx \
	src/Idle.cxx src/Idle.hxx \
	src/CommandLine.cxx src/CommandLine.hxx \
	src/CrossFade.cxx src/CrossFade.hxx \
	src/cue/CueParser.cxx src/cue/CueParser.hxx \
	src/DecoderError.cxx src/DecoderError.hxx \
	src/DecoderThread.cxx src/DecoderThread.hxx \
	src/DecoderCommand.hxx \
	src/DecoderControl.cxx src/DecoderControl.hxx \
	src/DecoderAPI.cxx src/DecoderAPI.hxx \
	src/DecoderPlugin.hxx \
	src/DecoderInternal.cxx src/DecoderInternal.hxx \
	src/DecoderPrint.cxx src/DecoderPrint.hxx \
	src/Directory.cxx src/Directory.hxx \
	src/DirectorySave.cxx src/DirectorySave.hxx \
	src/DatabaseSimple.hxx \
	src/DatabaseGlue.cxx src/DatabaseGlue.hxx \
	src/DatabasePrint.cxx src/DatabasePrint.hxx \
	src/DatabaseQueue.cxx src/DatabaseQueue.hxx \
	src/DatabasePlaylist.cxx src/DatabasePlaylist.hxx \
	src/DatabaseError.cxx src/DatabaseError.hxx \
	src/DatabaseLock.cxx src/DatabaseLock.hxx \
	src/DatabaseSave.cxx src/DatabaseSave.hxx \
	src/DatabasePlugin.hxx \
	src/DatabaseVisitor.hxx \
	src/DatabaseSelection.cxx src/DatabaseSelection.hxx \
	src/ExcludeList.cxx src/ExcludeList.hxx \
	src/FilterConfig.cxx src/FilterConfig.hxx \
	src/FilterPlugin.cxx src/FilterPlugin.hxx \
	src/FilterInternal.hxx \
	src/FilterRegistry.cxx src/FilterRegistry.hxx \
	src/UpdateDomain.cxx src/UpdateDomain.hxx \
	src/UpdateGlue.cxx src/UpdateGlue.hxx \
	src/UpdateQueue.cxx src/UpdateQueue.hxx \
	src/UpdateIO.cxx src/UpdateIO.hxx \
	src/UpdateDatabase.cxx src/UpdateDatabase.hxx \
	src/UpdateWalk.cxx src/UpdateWalk.hxx \
	src/UpdateSong.cxx src/UpdateSong.hxx \
	src/UpdateContainer.cxx src/UpdateContainer.hxx \
	src/UpdateInternal.hxx \
	src/UpdateRemove.cxx src/UpdateRemove.hxx \
	src/Client.cxx src/Client.hxx \
	src/ClientInternal.hxx \
	src/ClientEvent.cxx \
	src/ClientExpire.cxx \
	src/ClientGlobal.cxx \
	src/ClientIdle.cxx \
	src/ClientList.cxx src/ClientList.hxx \
	src/ClientNew.cxx \
	src/ClientProcess.cxx \
	src/ClientRead.cxx \
	src/ClientWrite.cxx \
	src/ClientMessage.cxx src/ClientMessage.hxx \
	src/ClientSubscribe.cxx \
	src/ClientFile.cxx src/ClientFile.hxx \
	src/Listen.cxx src/Listen.hxx \
	src/LogInit.cxx src/LogInit.hxx \
	src/Log.cxx src/Log.hxx src/LogV.hxx \
	src/ls.cxx src/ls.hxx \
	src/IOThread.cxx src/IOThread.hxx \
	src/Main.cxx src/Main.hxx \
	src/Instance.cxx src/Instance.hxx \
	src/win32/Win32Main.cxx \
	src/GlobalEvents.cxx src/GlobalEvents.hxx \
	src/Daemon.cxx src/Daemon.hxx \
	src/AudioCompress/compress.c \
	src/MixRampInfo.hxx \
	src/MusicBuffer.cxx src/MusicBuffer.hxx \
	src/MusicPipe.cxx src/MusicPipe.hxx \
	src/MusicChunk.cxx src/MusicChunk.hxx \
	src/Mapper.cxx src/Mapper.hxx \
	src/Page.cxx src/Page.hxx \
	src/Partition.cxx src/Partition.hxx \
	src/Permission.cxx src/Permission.hxx \
	src/PlayerThread.cxx src/PlayerThread.hxx \
	src/PlayerControl.cxx src/PlayerControl.hxx \
	src/Playlist.cxx \
	src/PlaylistError.cxx src/PlaylistError.hxx \
	src/PlaylistGlobal.cxx src/PlaylistGlobal.hxx \
	src/PlaylistControl.cxx \
	src/PlaylistEdit.cxx \
	src/PlaylistPrint.cxx src/PlaylistPrint.hxx \
	src/PlaylistSave.cxx src/PlaylistSave.hxx \
	src/PlaylistMapper.cxx src/PlaylistMapper.hxx \
	src/PlaylistAny.cxx src/PlaylistAny.hxx \
	src/PlaylistSong.cxx src/PlaylistSong.hxx \
	src/PlaylistState.cxx src/PlaylistState.hxx \
	src/PlaylistQueue.cxx src/PlaylistQueue.hxx \
	src/PlaylistVector.cxx src/PlaylistVector.hxx \
	src/PlaylistInfo.hxx \
	src/PlaylistDatabase.cxx src/PlaylistDatabase.hxx \
	src/PlaylistUpdate.cxx \
	src/BulkEdit.hxx \
	src/IdTable.hxx \
	src/Queue.cxx src/Queue.hxx \
	src/QueuePrint.cxx src/QueuePrint.hxx \
	src/QueueSave.cxx src/QueueSave.hxx \
	src/ReplayGainConfig.cxx src/ReplayGainConfig.hxx \
	src/ReplayGainInfo.cxx src/ReplayGainInfo.hxx \
	src/SignalHandlers.cxx src/SignalHandlers.hxx \
	src/Song.cxx src/Song.hxx \
	src/SongUpdate.cxx \
	src/SongPrint.cxx src/SongPrint.hxx \
	src/SongSave.cxx src/SongSave.hxx \
	src/SongSort.cxx src/SongSort.hxx \
	src/StateFile.cxx src/StateFile.hxx \
	src/Stats.cxx src/Stats.hxx \
	src/TagPrint.cxx src/TagPrint.hxx \
	src/TagSave.cxx src/TagSave.hxx \
	src/TagFile.cxx src/TagFile.hxx \
	src/TextFile.cxx src/TextFile.hxx \
	src/TextInputStream.cxx \
	src/Volume.cxx src/Volume.hxx \
	src/SongFilter.cxx src/SongFilter.hxx \
	src/SongPointer.hxx \
	src/PlaylistFile.cxx src/PlaylistFile.hxx \
	src/Timer.cxx

#
# Windows resource file
#

src/win32/mpd_win32_rc.$(OBJEXT): src/win32/mpd_win32_rc.rc
	$(WINDRES) -i $< -o $@

if HAVE_WINDOWS
noinst_DATA = src/win32/mpd_win32_rc.rc

EXTRA_src_mpd_DEPENDENCIES = src/win32/mpd_win32_rc.$(OBJEXT)
src_mpd_LDFLAGS = -Wl,src/win32/mpd_win32_rc.$(OBJEXT)
endif

if ENABLE_INOTIFY
src_mpd_SOURCES += \
	src/InotifyDomain.cxx src/InotifyDomain.hxx \
	src/InotifySource.cxx src/InotifySource.hxx \
	src/InotifyQueue.cxx src/InotifyQueue.hxx \
	src/InotifyUpdate.cxx src/InotifyUpdate.hxx
endif

if ENABLE_SQLITE
src_mpd_SOURCES += \
	src/command/StickerCommands.cxx src/command/StickerCommands.hxx \
	src/StickerDatabase.cxx src/StickerDatabase.hxx \
	src/StickerPrint.cxx src/StickerPrint.hxx \
	src/SongSticker.cxx src/SongSticker.hxx
endif

# Generic utility library

libutil_a_SOURCES = \
	src/util/Macros.hxx \
	src/util/Error.cxx src/util/Error.hxx \
	src/util/Domain.hxx \
	src/util/ReusableArray.hxx \
	src/util/ASCII.hxx \
	src/util/CharUtil.hxx \
	src/util/NumberParser.hxx \
	src/util/StringUtil.cxx src/util/StringUtil.hxx \
	src/util/FormatString.cxx src/util/FormatString.hxx \
	src/util/Tokenizer.cxx src/util/Tokenizer.hxx \
	src/util/UriUtil.cxx src/util/UriUtil.hxx \
	src/util/Manual.hxx \
	src/util/RefCount.hxx \
	src/util/fifo_buffer.c src/util/fifo_buffer.h \
	src/util/FifoBuffer.hxx \
	src/util/WritableBuffer.hxx \
	src/util/growing_fifo.c src/util/growing_fifo.h \
	src/util/LazyRandomEngine.cxx src/util/LazyRandomEngine.hxx \
	src/util/SliceBuffer.hxx \
	src/util/HugeAllocator.cxx src/util/HugeAllocator.hxx \
	src/util/PeakBuffer.cxx src/util/PeakBuffer.hxx \
	src/util/list.h \
	src/util/list_sort.c src/util/list_sort.h \
	src/util/ByteReverse.cxx src/util/ByteReverse.hxx \
	src/util/bit_reverse.c src/util/bit_reverse.h

# Multi-threading library

libthread_a_SOURCES = \
	src/thread/Mutex.hxx \
	src/thread/PosixMutex.hxx \
	src/thread/CriticalSection.hxx \
	src/thread/GLibMutex.hxx \
	src/thread/Cond.hxx \
	src/thread/PosixCond.hxx \
	src/thread/WindowsCond.hxx \
	src/thread/GLibCond.hxx \
	src/thread/Thread.cxx src/thread/Thread.hxx \
	src/thread/Id.hxx

# System library

libsystem_a_SOURCES = \
	src/system/ByteOrder.hxx \
	src/system/FatalError.cxx src/system/FatalError.hxx \
	src/system/fd_util.c src/system/fd_util.h \
	src/system/SocketUtil.cxx src/system/SocketUtil.hxx \
	src/system/SocketError.cxx src/system/SocketError.hxx \
	src/system/Resolver.cxx src/system/Resolver.hxx \
	src/system/EventPipe.cxx src/system/EventPipe.hxx \
	src/system/EventFD.cxx src/system/EventFD.hxx \
	src/system/SignalFD.cxx src/system/SignalFD.hxx \
	src/system/EPollFD.cxx src/system/EPollFD.hxx \
	src/system/Clock.cxx src/system/Clock.hxx

# Event loop library

libevent_a_SOURCES = \
	src/event/WakeFD.hxx \
	src/event/SignalMonitor.hxx src/event/SignalMonitor.cxx \
	src/event/TimeoutMonitor.hxx src/event/TimeoutMonitor.cxx \
	src/event/IdleMonitor.hxx src/event/IdleMonitor.cxx \
	src/event/DeferredMonitor.hxx src/event/DeferredMonitor.cxx \
	src/event/SocketMonitor.cxx src/event/SocketMonitor.hxx \
	src/event/BufferedSocket.cxx src/event/BufferedSocket.hxx \
	src/event/FullyBufferedSocket.cxx src/event/FullyBufferedSocket.hxx \
	src/event/MultiSocketMonitor.cxx src/event/MultiSocketMonitor.hxx \
	src/event/ServerSocket.cxx src/event/ServerSocket.hxx \
	src/event/Call.hxx src/event/Call.cxx \
	src/event/Loop.cxx src/event/Loop.hxx

# PCM library

libpcm_a_SOURCES = \
	src/pcm/PcmBuffer.cxx src/pcm/PcmBuffer.hxx \
	src/pcm/PcmExport.cxx src/pcm/PcmExport.hxx \
	src/pcm/PcmConvert.cxx src/pcm/PcmConvert.hxx \
	src/pcm/dsd2pcm/dsd2pcm.c src/pcm/dsd2pcm/dsd2pcm.h \
	src/pcm/PcmDsd.cxx src/pcm/PcmDsd.hxx \
	src/pcm/PcmDsdUsb.cxx src/pcm/PcmDsdUsb.hxx \
	src/pcm/PcmVolume.cxx src/pcm/PcmVolume.hxx \
	src/pcm/PcmMix.cxx src/pcm/PcmMix.hxx \
	src/pcm/PcmChannels.cxx src/pcm/PcmChannels.hxx \
	src/pcm/PcmPack.cxx src/pcm/PcmPack.hxx \
	src/pcm/PcmFormat.cxx src/pcm/PcmFormat.hxx \
	src/pcm/PcmResample.cxx src/pcm/PcmResample.hxx \
	src/pcm/PcmResampleFallback.cxx \
	src/pcm/PcmResampleInternal.hxx \
	src/pcm/PcmDither.cxx src/pcm/PcmDither.hxx \
	src/pcm/PcmPrng.hxx \
	src/pcm/PcmUtils.hxx
libpcm_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(SAMPLERATE_CFLAGS)

PCM_LIBS = \
	libpcm.a \
	$(SAMPLERATE_LIBS)

if HAVE_LIBSAMPLERATE
libpcm_a_SOURCES += src/pcm/PcmResampleLibsamplerate.cxx
endif

# File system library

libfs_a_SOURCES = \
	src/fs/Domain.cxx src/fs/Domain.hxx \
	src/fs/Limits.hxx \
	src/fs/Traits.cxx src/fs/Traits.hxx \
	src/fs/Config.cxx src/fs/Config.hxx \
	src/fs/Charset.cxx src/fs/Charset.hxx \
	src/fs/Path.cxx src/fs/Path.hxx \
	src/fs/AllocatedPath.cxx src/fs/AllocatedPath.hxx \
	src/fs/FileSystem.cxx src/fs/FileSystem.hxx \
	src/fs/DirectoryReader.hxx

# database plugins

libdb_plugins_a_SOURCES = \
	src/DatabaseRegistry.cxx src/DatabaseRegistry.hxx \
	src/DatabaseHelpers.cxx src/DatabaseHelpers.hxx \
	src/db/SimpleDatabasePlugin.cxx src/db/SimpleDatabasePlugin.hxx

if HAVE_LIBMPDCLIENT
libdb_plugins_a_SOURCES += \
	src/db/ProxyDatabasePlugin.cxx src/db/ProxyDatabasePlugin.hxx
endif

DB_LIBS = \
	libdb_plugins.a \
	$(LIBMPDCLIENT_LIBS)

# archive plugins

if ENABLE_ARCHIVE

noinst_LIBRARIES += libarchive.a

src_mpd_SOURCES +=  \
	src/UpdateArchive.cxx src/UpdateArchive.hxx

libarchive_a_SOURCES = \
	src/ArchiveDomain.cxx src/ArchiveDomain.hxx \
	src/ArchiveLookup.cxx src/ArchiveLookup.hxx \
	src/ArchiveList.cxx src/ArchiveList.hxx \
	src/ArchivePlugin.cxx src/ArchivePlugin.hxx \
	src/ArchiveVisitor.hxx \
	src/ArchiveFile.hxx \
	src/input/ArchiveInputPlugin.cxx src/input/ArchiveInputPlugin.hxx
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
libarchive_a_SOURCES += \
	src/archive/Bzip2ArchivePlugin.cxx \
	src/archive/Bzip2ArchivePlugin.hxx
endif

if HAVE_ZZIP
libarchive_a_SOURCES += \
	src/archive/ZzipArchivePlugin.cxx \
	src/archive/ZzipArchivePlugin.hxx
endif

if HAVE_ISO9660
libarchive_a_SOURCES += \
	src/archive/Iso9660ArchivePlugin.cxx \
	src/archive/Iso9660ArchivePlugin.hxx
endif

else
ARCHIVE_LIBS =
endif

# configuration library

libconf_a_SOURCES = \
	src/ConfigDefaults.hxx \
	src/ConfigPath.cxx src/ConfigPath.hxx \
	src/ConfigData.cxx src/ConfigData.hxx \
	src/ConfigParser.cxx src/ConfigParser.hxx \
	src/ConfigGlobal.cxx src/ConfigGlobal.hxx \
	src/ConfigFile.cxx src/ConfigFile.hxx \
	src/ConfigTemplates.cxx src/ConfigTemplates.hxx \
	src/ConfigError.cxx src/ConfigError.hxx \
	src/ConfigOption.hxx

# tag plugins

libtag_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ID3TAG_CFLAGS)
TAG_LIBS = \
	libtag.a \
	$(ID3TAG_LIBS)

libtag_a_SOURCES =\
	src/tag/TagType.h \
	src/tag/Tag.cxx src/tag/Tag.hxx \
	src/tag/TagBuilder.cxx src/tag/TagBuilder.hxx \
	src/tag/TagItem.hxx \
	src/tag/TagHandler.cxx src/tag/TagHandler.hxx \
	src/tag/TagSettings.c src/tag/TagSettings.h \
	src/tag/TagConfig.cxx src/tag/TagConfig.hxx \
	src/tag/TagNames.c \
	src/tag/TagString.cxx src/tag/TagString.hxx \
	src/tag/TagPool.cxx src/tag/TagPool.hxx \
	src/tag/TagTable.cxx src/tag/TagTable.hxx \
	src/tag/ApeLoader.cxx src/tag/ApeLoader.hxx \
	src/tag/ApeReplayGain.cxx src/tag/ApeReplayGain.hxx \
	src/tag/ApeTag.cxx src/tag/ApeTag.hxx

if HAVE_ID3TAG
libtag_a_SOURCES += \
	src/tag/TagId3.cxx src/tag/TagId3.hxx \
	src/tag/TagRva2.cxx src/tag/TagRva2.hxx \
	src/tag/Riff.cxx src/tag/Riff.hxx \
	src/tag/Aiff.cxx src/tag/Aiff.hxx
endif

# decoder plugins

libdecoder_plugins_a_SOURCES = \
	src/decoder/PcmDecoderPlugin.cxx \
	src/decoder/PcmDecoderPlugin.hxx \
	src/DecoderBuffer.cxx src/DecoderBuffer.hxx \
	src/DecoderPlugin.cxx \
	src/DecoderList.cxx src/DecoderList.hxx
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
	$(OPUS_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(MPCDEC_CFLAGS) \
	$(ADPLUG_CFLAGS) \
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
	$(OPUS_LIBS) \
	$(FFMPEG_LIBS) \
	$(MPCDEC_LIBS) \
	$(ADPLUG_LIBS) \
	$(FAAD_LIBS)

DECODER_SRC =

if ENABLE_DSD
libdecoder_plugins_a_SOURCES += \
	src/decoder/DsdiffDecoderPlugin.cxx \
	src/decoder/DsdiffDecoderPlugin.hxx \
	src/decoder/DsfDecoderPlugin.cxx \
	src/decoder/DsfDecoderPlugin.hxx \
	src/decoder/DsdLib.cxx \
	src/decoder/DsdLib.hxx
endif

if HAVE_MAD
libdecoder_plugins_a_SOURCES += \
	src/decoder/MadDecoderPlugin.cxx \
	src/decoder/MadDecoderPlugin.hxx
endif

if HAVE_MPG123
libdecoder_plugins_a_SOURCES += \
	src/decoder/Mpg123DecoderPlugin.cxx \
	src/decoder/Mpg123DecoderPlugin.hxx
endif

if HAVE_MPCDEC
libdecoder_plugins_a_SOURCES += \
	src/decoder/MpcdecDecoderPlugin.cxx \
	src/decoder/MpcdecDecoderPlugin.hxx
endif

if HAVE_OPUS
libdecoder_plugins_a_SOURCES += \
	src/decoder/OggUtil.cxx \
	src/decoder/OggUtil.hxx \
	src/decoder/OggSyncState.hxx \
	src/decoder/OggFind.cxx src/decoder/OggFind.hxx \
	src/decoder/OpusDomain.cxx src/decoder/OpusDomain.hxx \
	src/decoder/OpusReader.hxx \
	src/decoder/OpusHead.hxx \
	src/decoder/OpusHead.cxx \
	src/decoder/OpusTags.cxx \
	src/decoder/OpusTags.hxx \
	src/decoder/OpusDecoderPlugin.cxx \
	src/decoder/OpusDecoderPlugin.h
endif

if HAVE_WAVPACK
libdecoder_plugins_a_SOURCES += \
	src/decoder/WavpackDecoderPlugin.cxx \
	src/decoder/WavpackDecoderPlugin.hxx
endif

if HAVE_ADPLUG
libdecoder_plugins_a_SOURCES += \
	src/decoder/AdPlugDecoderPlugin.cxx \
	src/decoder/AdPlugDecoderPlugin.h
endif

if HAVE_FAAD
libdecoder_plugins_a_SOURCES += \
	src/decoder/FaadDecoderPlugin.cxx src/decoder/FaadDecoderPlugin.hxx
endif

if HAVE_XIPH
libdecoder_plugins_a_SOURCES += \
	src/decoder/XiphTags.cxx src/decoder/XiphTags.hxx \
	src/decoder/OggCodec.cxx src/decoder/OggCodec.hxx
endif

if ENABLE_VORBIS_DECODER
libdecoder_plugins_a_SOURCES += \
	src/decoder/VorbisDomain.cxx src/decoder/VorbisDomain.hxx \
	src/decoder/VorbisComments.cxx src/decoder/VorbisComments.hxx \
	src/decoder/VorbisDecoderPlugin.cxx src/decoder/VorbisDecoderPlugin.h
endif

if HAVE_FLAC
libdecoder_plugins_a_SOURCES += \
	src/decoder/FlacInput.cxx src/decoder/FlacInput.hxx \
	src/decoder/FlacIOHandle.cxx src/decoder/FlacIOHandle.hxx \
	src/decoder/FlacMetadata.cxx src/decoder/FlacMetadata.hxx \
	src/decoder/FlacPcm.cxx src/decoder/FlacPcm.hxx \
	src/decoder/FlacDomain.cxx src/decoder/FlacDomain.hxx \
	src/decoder/FlacCommon.cxx src/decoder/FlacCommon.hxx \
	src/decoder/FlacDecoderPlugin.cxx \
	src/decoder/FlacDecoderPlugin.h
endif

if HAVE_AUDIOFILE
libdecoder_plugins_a_SOURCES += \
	src/decoder/AudiofileDecoderPlugin.cxx \
	src/decoder/AudiofileDecoderPlugin.hxx
endif

if ENABLE_MIKMOD_DECODER
libdecoder_plugins_a_SOURCES += \
	src/decoder/MikmodDecoderPlugin.cxx \
	src/decoder/MikmodDecoderPlugin.hxx
endif

if HAVE_MODPLUG
libmodplug_decoder_plugin_a_SOURCES = \
	src/decoder/ModplugDecoderPlugin.cxx \
	src/decoder/ModplugDecoderPlugin.hxx
libmodplug_decoder_plugin_a_CXXFLAGS = $(AM_CXXFLAGS) $(MODPLUG_CFLAGS)
libmodplug_decoder_plugin_a_CPPFLAGS = $(src_mpd_CPPFLAGS)
noinst_LIBRARIES += libmodplug_decoder_plugin.a
DECODER_LIBS += libmodplug_decoder_plugin.a $(MODPLUG_LIBS)
endif

if ENABLE_SIDPLAY
libdecoder_plugins_a_SOURCES += \
	src/decoder/SidplayDecoderPlugin.cxx \
	src/decoder/SidplayDecoderPlugin.hxx
endif

if ENABLE_FLUIDSYNTH
libdecoder_plugins_a_SOURCES += \
	src/decoder/FluidsynthDecoderPlugin.cxx \
	src/decoder/FluidsynthDecoderPlugin.hxx
endif

if ENABLE_WILDMIDI
libdecoder_plugins_a_SOURCES += \
	src/decoder/WildmidiDecoderPlugin.cxx \
	src/decoder/WildmidiDecoderPlugin.hxx
endif

if HAVE_FFMPEG
libdecoder_plugins_a_SOURCES += \
	src/decoder/FfmpegMetaData.cxx \
	src/decoder/FfmpegMetaData.hxx \
	src/decoder/FfmpegDecoderPlugin.cxx \
	src/decoder/FfmpegDecoderPlugin.hxx
endif

if ENABLE_SNDFILE
libdecoder_plugins_a_SOURCES += \
	src/decoder/SndfileDecoderPlugin.cxx \
	src/decoder/SndfileDecoderPlugin.hxx
endif

if HAVE_GME
libdecoder_plugins_a_SOURCES += \
	src/decoder/GmeDecoderPlugin.cxx src/decoder/GmeDecoderPlugin.hxx
endif

# encoder plugins

if ENABLE_ENCODER

noinst_LIBRARIES += libencoder_plugins.a

libencoder_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LAME_CFLAGS) \
	$(TWOLAME_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(OPUS_CFLAGS) \
	$(VORBISENC_CFLAGS)

ENCODER_LIBS = \
	libencoder_plugins.a \
	$(LAME_LIBS) \
	$(TWOLAME_LIBS) \
	$(FLAC_LIBS) \
	$(OPUS_LIBS) \
	$(VORBISENC_LIBS)

libencoder_plugins_a_SOURCES = \
	src/EncoderAPI.hxx \
	src/EncoderPlugin.hxx \
	src/encoder/OggStream.hxx \
	src/encoder/NullEncoderPlugin.cxx src/encoder/NullEncoderPlugin.hxx \
	src/EncoderList.cxx src/EncoderList.hxx

if HAVE_OGG_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/OggSerial.cxx src/encoder/OggSerial.hxx \
	src/encoder/OggStream.hxx
endif

if ENABLE_WAVE_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/WaveEncoderPlugin.cxx \
	src/encoder/WaveEncoderPlugin.hxx
endif

if ENABLE_VORBIS_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/VorbisEncoderPlugin.cxx \
	src/encoder/VorbisEncoderPlugin.hxx
endif

if HAVE_OPUS
libencoder_plugins_a_SOURCES += \
	src/encoder/OpusEncoderPlugin.cxx \
	src/encoder/OpusEncoderPlugin.hxx
endif

if ENABLE_LAME_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/LameEncoderPlugin.cxx \
	src/encoder/LameEncoderPlugin.hxx
endif

if ENABLE_TWOLAME_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/TwolameEncoderPlugin.cxx \
	src/encoder/TwolameEncoderPlugin.hxx
endif

if ENABLE_FLAC_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/FlacEncoderPlugin.cxx src/encoder/FlacEncoderPlugin.hxx
endif

else
ENCODER_LIBS =
endif


if HAVE_ZEROCONF
src_mpd_SOURCES += \
	src/ZeroconfInternal.hxx \
	src/ZeroconfGlue.cxx src/ZeroconfGlue.hxx

if HAVE_AVAHI
src_mpd_SOURCES += \
	src/AvahiPoll.cxx src/AvahiPoll.hxx \
	src/ZeroconfAvahi.cxx src/ZeroconfAvahi.hxx
endif

if HAVE_BONJOUR
src_mpd_SOURCES += src/ZeroconfBonjour.cxx src/ZeroconfBonjour.hxx
endif
endif

#
# input plugins
#

libinput_a_SOURCES = \
	src/InputInit.cxx src/InputInit.hxx \
	src/InputRegistry.cxx src/InputRegistry.hxx \
	src/InputStream.cxx src/InputStream.hxx \
	src/InputPlugin.hxx \
	src/input/RewindInputPlugin.cxx src/input/RewindInputPlugin.hxx \
	src/input/FileInputPlugin.cxx src/input/FileInputPlugin.hxx

libinput_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(CURL_CFLAGS) \
	$(CDIO_PARANOIA_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(MMS_CFLAGS)

INPUT_LIBS = \
	libinput.a \
	$(CURL_LIBS) \
	$(CDIO_PARANOIA_LIBS) \
	$(FFMPEG_LIBS) \
	$(MMS_LIBS)

if ENABLE_CURL
libinput_a_SOURCES += \
	src/input/CurlInputPlugin.cxx src/input/CurlInputPlugin.hxx \
	src/IcyMetaDataParser.cxx src/IcyMetaDataParser.hxx
endif

if ENABLE_CDIO_PARANOIA
libinput_a_SOURCES += \
	src/input/CdioParanoiaInputPlugin.cxx \
	src/input/CdioParanoiaInputPlugin.hxx
endif

if HAVE_FFMPEG
libinput_a_SOURCES += \
	src/input/FfmpegInputPlugin.cxx src/input/FfmpegInputPlugin.hxx
endif

if ENABLE_MMS
libinput_a_SOURCES += \
	src/input/MmsInputPlugin.cxx src/input/MmsInputPlugin.hxx
endif


liboutput_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(AO_CFLAGS) \
	$(ALSA_CFLAGS) \
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
	$(JACK_LIBS) \
	$(OPENAL_LIBS) \
	$(PULSE_LIBS) \
	$(SHOUT_LIBS)

OUTPUT_API_SRC = \
	src/OutputAPI.hxx \
	src/OutputInternal.hxx \
	src/OutputList.cxx src/OutputList.hxx \
	src/OutputAll.cxx src/OutputAll.hxx \
	src/OutputThread.cxx src/OutputThread.hxx \
	src/OutputError.cxx src/OutputError.hxx \
	src/OutputControl.cxx src/OutputControl.hxx \
	src/OutputState.cxx src/OutputState.hxx \
	src/OutputPrint.cxx src/OutputPrint.hxx \
	src/OutputCommand.cxx src/OutputCommand.hxx \
	src/OutputPlugin.cxx src/OutputPlugin.hxx \
	src/OutputFinish.cxx \
	src/OutputInit.cxx

liboutput_plugins_a_SOURCES = \
	src/output/NullOutputPlugin.cxx \
	src/output/NullOutputPlugin.hxx

MIXER_LIBS = \
	libmixer_plugins.a \
	$(PULSE_LIBS)

MIXER_API_SRC = \
	src/MixerPlugin.hxx \
	src/MixerList.hxx \
	src/MixerControl.cxx src/MixerControl.hxx \
	src/MixerType.cxx src/MixerType.hxx \
	src/MixerAll.cxx src/MixerAll.hxx \
	src/MixerInternal.hxx

libmixer_plugins_a_SOURCES = \
	src/mixer/SoftwareMixerPlugin.cxx \
	src/mixer/SoftwareMixerPlugin.hxx
libmixer_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ALSA_CFLAGS) \
	$(PULSE_CFLAGS)

if HAVE_ALSA
liboutput_plugins_a_SOURCES += \
	src/output/AlsaOutputPlugin.cxx \
	src/output/AlsaOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/AlsaMixerPlugin.cxx
endif

if HAVE_ROAR
liboutput_plugins_a_SOURCES += \
	src/output/RoarOutputPlugin.cxx src/output/RoarOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/RoarMixerPlugin.cxx
endif

if HAVE_AO
liboutput_plugins_a_SOURCES += \
	src/output/AoOutputPlugin.cxx src/output/AoOutputPlugin.hxx
endif

if HAVE_FIFO
liboutput_plugins_a_SOURCES += \
	src/output/FifoOutputPlugin.cxx src/output/FifoOutputPlugin.hxx
endif

if ENABLE_PIPE_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/PipeOutputPlugin.cxx src/output/PipeOutputPlugin.hxx
endif

if HAVE_JACK
liboutput_plugins_a_SOURCES += \
	src/output/JackOutputPlugin.cxx src/output/JackOutputPlugin.hxx
endif

if HAVE_OSS
liboutput_plugins_a_SOURCES += \
	src/output/OssOutputPlugin.cxx \
	src/output/OssOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/OssMixerPlugin.cxx
endif

if HAVE_OPENAL
liboutput_plugins_a_SOURCES += \
	src/output/OpenALOutputPlugin.cxx src/output/OpenALOutputPlugin.hxx
endif

if HAVE_OSX
liboutput_plugins_a_SOURCES += \
	src/output/OSXOutputPlugin.cxx \
	src/output/OSXOutputPlugin.hxx
endif

if HAVE_PULSE
liboutput_plugins_a_SOURCES += \
	src/output/PulseOutputPlugin.cxx src/output/PulseOutputPlugin.hxx
libmixer_plugins_a_SOURCES += \
	src/mixer/PulseMixerPlugin.cxx src/mixer/PulseMixerPlugin.hxx
endif

if HAVE_SHOUT
liboutput_plugins_a_SOURCES += \
	src/output/ShoutOutputPlugin.cxx src/output/ShoutOutputPlugin.hxx
endif

if ENABLE_RECORDER_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/RecorderOutputPlugin.cxx src/output/RecorderOutputPlugin.hxx
endif

if ENABLE_HTTPD_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/IcyMetaDataServer.cxx src/IcyMetaDataServer.hxx \
	src/output/HttpdInternal.hxx \
	src/output/HttpdClient.cxx src/output/HttpdClient.hxx \
	src/output/HttpdOutputPlugin.cxx src/output/HttpdOutputPlugin.hxx
endif

if ENABLE_SOLARIS_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/SolarisOutputPlugin.cxx src/output/SolarisOutputPlugin.hxx
endif

if ENABLE_WINMM_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/WinmmOutputPlugin.cxx src/output/WinmmOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/WinmmMixerPlugin.cxx
endif


#
# Playlist plugins
#

libplaylist_plugins_a_SOURCES = \
	src/PlaylistPlugin.hxx \
	src/SongEnumerator.hxx \
	src/MemorySongEnumerator.cxx src/MemorySongEnumerator.hxx \
	src/playlist/ExtM3uPlaylistPlugin.cxx \
	src/playlist/ExtM3uPlaylistPlugin.hxx \
	src/playlist/M3uPlaylistPlugin.cxx \
	src/playlist/M3uPlaylistPlugin.hxx \
	src/playlist/PlsPlaylistPlugin.cxx \
	src/playlist/PlsPlaylistPlugin.hxx \
	src/playlist/XspfPlaylistPlugin.cxx \
	src/playlist/XspfPlaylistPlugin.hxx \
	src/playlist/AsxPlaylistPlugin.cxx \
	src/playlist/AsxPlaylistPlugin.hxx \
	src/playlist/RssPlaylistPlugin.cxx \
	src/playlist/RssPlaylistPlugin.hxx \
	src/playlist/CuePlaylistPlugin.cxx \
	src/playlist/CuePlaylistPlugin.hxx \
	src/playlist/EmbeddedCuePlaylistPlugin.cxx \
	src/playlist/EmbeddedCuePlaylistPlugin.hxx \
	src/PlaylistRegistry.cxx src/PlaylistRegistry.hxx
libplaylist_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(YAJL_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS))

PLAYLIST_LIBS = \
	libplaylist_plugins.a \
	$(FLAC_LIBS)

if ENABLE_SOUNDCLOUD
libplaylist_plugins_a_SOURCES += \
	src/playlist/SoundCloudPlaylistPlugin.cxx \
	src/playlist/SoundCloudPlaylistPlugin.hxx
PLAYLIST_LIBS += $(YAJL_LIBS)
endif

#
# Filter plugins
#

libfilter_plugins_a_SOURCES = \
	src/filter/NullFilterPlugin.cxx \
	src/filter/ChainFilterPlugin.cxx \
	src/filter/ChainFilterPlugin.hxx \
	src/filter/AutoConvertFilterPlugin.cxx \
	src/filter/AutoConvertFilterPlugin.hxx \
	src/filter/ConvertFilterPlugin.cxx \
	src/filter/ConvertFilterPlugin.hxx \
	src/filter/RouteFilterPlugin.cxx \
	src/filter/NormalizeFilterPlugin.cxx \
	src/filter/ReplayGainFilterPlugin.cxx \
	src/filter/ReplayGainFilterPlugin.hxx \
	src/filter/VolumeFilterPlugin.cxx \
	src/filter/VolumeFilterPlugin.hxx

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
	test/test_util \
	test/test_byte_reverse \
	test/test_mixramp \
	test/test_icy_parser \
	test/test_pcm \
	test/test_protocol \
	test/test_queue_priority

if ENABLE_ARCHIVE
C_TESTS += test/test_archive
endif

TESTS = $(C_TESTS)

noinst_PROGRAMS = \
	$(C_TESTS) \
	test/read_conf \
	test/run_resolver \
	test/DumpDatabase \
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

if HAVE_AVAHI
noinst_PROGRAMS += test/run_avahi
endif

if ENABLE_ARCHIVE
noinst_PROGRAMS += test/visit_archive
endif

if HAVE_ID3TAG
noinst_PROGRAMS += test/dump_rva2
endif

if HAVE_ALSA
# this debug program is still ALSA specific
noinst_PROGRAMS += test/read_mixer
endif

test_read_conf_LDADD = \
	libconf.a \
	libutil.a \
	libsystem.a \
	libfs.a \
	$(GLIB_LIBS)
test_read_conf_SOURCES = \
	src/Log.cxx \
	test/read_conf.cxx

test_run_resolver_LDADD = \
	libsystem.a \
	libutil.a \
	$(GLIB_LIBS)
test_run_resolver_SOURCES = \
	src/Log.cxx \
	test/run_resolver.cxx

test_DumpDatabase_LDADD = \
	$(DB_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libutil.a \
	libsystem.a \
	libfs.a \
	$(GLIB_LIBS)
test_DumpDatabase_SOURCES = test/DumpDatabase.cxx \
	src/protocol/Ack.cxx \
	src/Log.cxx \
	src/DatabaseError.cxx \
	src/DatabaseRegistry.cxx \
	src/DatabaseSelection.cxx \
	src/Directory.cxx src/DirectorySave.cxx \
	src/PlaylistVector.cxx src/PlaylistDatabase.cxx \
	src/DatabaseLock.cxx src/DatabaseSave.cxx \
	src/Song.cxx src/SongSave.cxx src/SongSort.cxx \
	src/TagSave.cxx \
	src/SongFilter.cxx \
	src/TextFile.cxx

test_run_input_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libutil.a \
	libevent.a \
	libthread.a \
	libsystem.a \
	libfs.a \
	$(GLIB_LIBS)
test_run_input_SOURCES = test/run_input.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/IOThread.cxx \
	src/TagSave.cxx

if ENABLE_ARCHIVE

test_visit_archive_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libutil.a \
	libevent.a \
	libthread.a \
	libsystem.a \
	libfs.a \
	$(GLIB_LIBS)
test_visit_archive_SOURCES = test/visit_archive.cxx \
	src/Log.cxx \
	src/IOThread.cxx \
	src/InputStream.cxx

endif

test_dump_text_file_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	libfs.a \
	libsystem.a \
	libthread.a \
	libutil.a \
	$(GLIB_LIBS)
test_dump_text_file_SOURCES = test/dump_text_file.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/IOThread.cxx \
	src/TextInputStream.cxx

test_dump_playlist_LDADD = \
	$(PLAYLIST_LIBS) \
	$(FLAC_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(DECODER_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	libthread.a \
	libsystem.a \
	libfs.a \
	libutil.a \
	libpcm.a \
	$(GLIB_LIBS)
test_dump_playlist_SOURCES = test/dump_playlist.cxx \
	test/FakeDecoderAPI.cxx \
	$(DECODER_SRC) \
	src/Log.cxx \
	src/IOThread.cxx \
	src/Song.cxx src/TagSave.cxx \
	src/TagFile.cxx \
	src/CheckAudioFormat.cxx \
	src/TextInputStream.cxx \
	src/cue/CueParser.cxx src/cue/CueParser.hxx

if HAVE_FLAC
test_dump_playlist_SOURCES += \
	src/ReplayGainInfo.cxx \
	src/decoder/FlacMetadata.cxx
endif

test_run_decoder_LDADD = \
	$(DECODER_LIBS) \
	libpcm.a \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	libthread.a \
	libsystem.a \
	libfs.a \
	libutil.a \
	$(GLIB_LIBS)
test_run_decoder_SOURCES = test/run_decoder.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/IOThread.cxx \
	src/ReplayGainInfo.cxx \
	src/AudioFormat.cxx src/CheckAudioFormat.cxx \
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
	libconf.a \
	libevent.a \
	libthread.a \
	libsystem.a \
	libfs.a \
	libutil.a \
	$(GLIB_LIBS)
test_read_tags_SOURCES = test/read_tags.cxx \
	test/FakeDecoderAPI.cxx \
	src/Log.cxx \
	src/IOThread.cxx \
	src/ReplayGainInfo.cxx \
	src/CheckAudioFormat.cxx \
	$(DECODER_SRC)

if HAVE_ID3TAG
test_dump_rva2_LDADD = \
	$(TAG_LIBS) \
	libutil.a \
	$(GLIB_LIBS)
test_dump_rva2_SOURCES = \
	src/Log.cxx \
	test/dump_rva2.cxx
endif

test_run_filter_LDADD = \
	$(FILTER_LIBS) \
	libconf.a \
	libutil.a \
	libsystem.a \
	libfs.a \
	$(GLIB_LIBS)
test_run_filter_SOURCES = test/run_filter.cxx \
	test/FakeReplayGainConfig.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/FilterPlugin.cxx src/FilterRegistry.cxx \
	src/CheckAudioFormat.cxx \
	src/AudioFormat.cxx \
	src/AudioParser.cxx \
	src/ReplayGainInfo.cxx \
	src/AudioCompress/compress.c

if ENABLE_ENCODER
noinst_PROGRAMS += test/run_encoder
test_run_encoder_SOURCES = test/run_encoder.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/CheckAudioFormat.cxx \
	src/AudioFormat.cxx \
	src/AudioParser.cxx
test_run_encoder_LDADD = \
	$(ENCODER_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libpcm.a \
	libthread.a \
	libsystem.a \
	libfs.a \
	libutil.a \
	$(GLIB_LIBS)
endif

if ENABLE_VORBIS_ENCODER
noinst_PROGRAMS += test/test_vorbis_encoder
test_test_vorbis_encoder_SOURCES = test/test_vorbis_encoder.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/CheckAudioFormat.cxx \
	src/AudioFormat.cxx \
	src/AudioParser.cxx \
	$(ENCODER_SRC)
test_test_vorbis_encoder_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ENCODER_CFLAGS)
test_test_vorbis_encoder_LDADD = $(MPD_LIBS) \
	$(ENCODER_LIBS) \
	$(PCM_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libsystem.a \
	libfs.a \
	libutil.a \
	$(GLIB_LIBS)
endif

test_software_volume_SOURCES = test/software_volume.cxx \
	test/stdbin.h \
	src/CheckAudioFormat.cxx \
	src/AudioParser.cxx
test_software_volume_LDADD = \
	$(PCM_LIBS) \
	libutil.a \
	$(GLIB_LIBS)

test_run_avahi_SOURCES = \
	src/Log.cxx \
	src/ZeroconfAvahi.cxx src/AvahiPoll.cxx \
	test/ShutdownHandler.cxx test/ShutdownHandler.hxx \
	test/run_avahi.cxx
test_run_avahi_CPPFLAGS = $(AM_CPPFLAGS) \
	$(AVAHI_CFLAGS)
test_run_avahi_LDADD = \
	libevent.a \
	libsystem.a \
	$(GLIB_LIBS) \
	$(AVAHI_LIBS)

test_run_normalize_SOURCES = test/run_normalize.cxx \
	test/stdbin.h \
	src/CheckAudioFormat.cxx \
	src/AudioParser.cxx \
	src/AudioCompress/compress.c
test_run_normalize_LDADD = \
	libutil.a \
	$(GLIB_LIBS)

test_run_convert_SOURCES = test/run_convert.cxx \
	src/Log.cxx \
	src/AudioFormat.cxx \
	src/CheckAudioFormat.cxx \
	src/AudioParser.cxx
test_run_convert_LDADD = \
	$(PCM_LIBS) \
	libutil.a \
	$(GLIB_LIBS)

test_run_output_LDADD = $(MPD_LIBS) \
	$(PCM_LIBS) \
	$(OUTPUT_LIBS) \
	$(ENCODER_LIBS) \
	libmixer_plugins.a \
	$(FILTER_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	libfs.a \
	libsystem.a \
	libthread.a \
	libutil.a \
	$(GLIB_LIBS)
test_run_output_SOURCES = test/run_output.cxx \
	test/FakeReplayGainConfig.cxx \
	test/stdbin.h \
	src/Log.cxx \
	src/IOThread.cxx \
	src/CheckAudioFormat.cxx \
	src/AudioFormat.cxx \
	src/AudioParser.cxx \
	src/Timer.cxx \
	src/Page.cxx \
	src/OutputError.cxx \
	src/OutputInit.cxx src/OutputFinish.cxx src/OutputList.cxx \
	src/OutputPlugin.cxx \
	src/MixerControl.cxx \
	src/MixerType.cxx \
	src/FilterPlugin.cxx \
	src/FilterConfig.cxx \
	src/AudioCompress/compress.c \
	src/ReplayGainInfo.cxx

test_read_mixer_LDADD = \
	libpcm.a \
	libmixer_plugins.a \
	$(OUTPUT_LIBS) \
	libconf.a \
	libutil.a \
	libevent.a \
	libsystem.a \
	libfs.a \
	$(GLIB_LIBS)
test_read_mixer_SOURCES = test/read_mixer.cxx \
	src/Log.cxx \
	src/MixerControl.cxx \
	src/FilterPlugin.cxx \
	src/filter/VolumeFilterPlugin.cxx

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
test_run_inotify_SOURCES = test/run_inotify.cxx \
	test/ShutdownHandler.cxx test/ShutdownHandler.hxx \
	src/Log.cxx \
	src/InotifyDomain.cxx \
	src/InotifySource.cxx
test_run_inotify_LDADD = \
	libevent.a \
	libsystem.a \
	libutil.a \
	$(GLIB_LIBS)
endif

test_test_util_SOURCES = \
	test/test_util.cxx
test_test_util_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_util_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_util_LDADD = \
	libutil.a \
	$(CPPUNIT_LIBS)

test_test_byte_reverse_SOURCES = \
	test/test_byte_reverse.cxx
test_test_byte_reverse_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_byte_reverse_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_byte_reverse_LDADD = \
	libutil.a \
	$(CPPUNIT_LIBS)

test_test_mixramp_SOURCES = \
	src/Log.cxx \
	test/test_mixramp.cxx
test_test_mixramp_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_mixramp_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_mixramp_LDADD = \
	$(GLIB_LIBS) \
	$(CPPUNIT_LIBS)

test_test_icy_parser_SOURCES = \
	src/Log.cxx \
	test/test_icy_parser.cxx
test_test_icy_parser_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_icy_parser_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_icy_parser_LDADD = \
	libtag.a \
	$(GLIB_LIBS) \
	$(CPPUNIT_LIBS)

test_test_pcm_SOURCES = \
	test/test_pcm_util.hxx \
	test/test_pcm_dither.cxx \
	test/test_pcm_pack.cxx \
	test/test_pcm_channels.cxx \
	test/test_pcm_format.cxx \
	test/test_pcm_volume.cxx \
	test/test_pcm_mix.cxx \
	test/test_pcm_all.hxx \
	test/test_pcm_main.cxx
test_test_pcm_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_pcm_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_pcm_LDADD = \
	$(PCM_LIBS) \
	libutil.a \
	$(CPPUNIT_LIBS) \
	$(GLIB_LIBS)

test_test_archive_SOURCES = \
	src/Log.cxx \
	test/test_archive.cxx
test_test_archive_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_archive_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_archive_LDADD = \
	libarchive.a \
	$(GLIB_LIBS) \
	$(CPPUNIT_LIBS)

test_test_protocol_SOURCES = \
	src/protocol/ArgParser.cxx \
	test/test_protocol.cxx
test_test_protocol_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_protocol_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_protocol_LDADD = \
	libsystem.a \
	libutil.a \
	$(CPPUNIT_LIBS)

test_test_queue_priority_SOURCES = \
	src/Queue.cxx \
	test/test_queue_priority.cxx
test_test_queue_priority_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_queue_priority_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_queue_priority_LDADD = \
	libsystem.a \
	libutil.a \
	$(CPPUNIT_LIBS)

if ENABLE_DSD

noinst_PROGRAMS += src/pcm/dsd2pcm/dsd2pcm

src_pcm_dsd2pcm_dsd2pcm_SOURCES = \
	src/pcm/dsd2pcm/dsd2pcm.c src/pcm/dsd2pcm/dsd2pcm.h \
	src/pcm/dsd2pcm/dsd2pcm.hpp \
	src/pcm/dsd2pcm/noiseshape.c src/pcm/dsd2pcm/noiseshape.h \
	src/pcm/dsd2pcm/noiseshape.hpp \
	src/pcm/dsd2pcm/main.cpp
src_pcm_dsd2pcm_dsd2pcm_LDADD = libutil.a

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
	test/test_archive_bzip2.sh  \
	test/test_archive_iso9660.sh \
	test/test_archive_zzip.sh \
	$(wildcard scripts/*.sh) \
	$(man_MANS) $(DOCBOOK_FILES) doc/mpdconf.example doc/doxygen.conf \
	src/win32/mpd_win32_rc.rc.in src/win32/mpd.ico
