ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.11 dist-xz subdir-objects

AM_CPPFLAGS += -I$(srcdir)/src $(BOOST_CPPFLAGS)

AM_CPPFLAGS += -DSYSTEM_CONFIG_FILE_LOCATION='"$(sysconfdir)/mpd.conf"'

APK_NAME = mpd
if ANDROID
else
bin_PROGRAMS = src/mpd
endif

noinst_LIBRARIES = \
	libmpd.a \
	libutil.a \
	libthread.a \
	libnet.a \
	libsystem.a \
	libevent.a \
	libicu.a \
	libpcm.a \
	libbasic.a \
	libconf.a \
	libtag.a \
	libinput.a \
	libfs.a \
	libplaylist_plugins.a \
	libdecoder.a \
	libfilter_plugins.a \
	libmixer_plugins.a \
	liboutput_plugins.a

libmpd_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBMPDCLIENT_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(LIBWRAP_CFLAGS) \
	$(SQLITE_CFLAGS)

src_mpd_LDADD = \
	libmpd.a \
	$(NEIGHBOR_LIBS) \
	$(DB_LIBS) \
	$(STORAGE_LIBS) \
	$(PLAYLIST_LIBS) \
	$(AVAHI_LIBS) \
	$(LIBWRAP_LDFLAGS) \
	$(SQLITE_LIBS) \
	$(DECODER_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(OUTPUT_LIBS) \
	$(TAG_LIBS) \
	$(FILTER_LIBS) \
	$(ENCODER_LIBS) \
	$(MIXER_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	libthread.a \
	libnet.a \
	$(FS_LIBS) \
	libsystem.a \
	libutil.a \
	$(ICU_LDADD) \
	$(SYSTEMD_DAEMON_LIBS)

src_mpd_SOURCES = \
	src/Main.cxx src/Main.hxx

libmpd_a_SOURCES = \
	$(OUTPUT_API_SRC) \
	$(MIXER_API_SRC) \
	src/check.h \
	src/Compiler.h \
	src/open.h \
	src/poison.h \
	src/notify.cxx src/notify.hxx \
	src/protocol/Ack.cxx src/protocol/Ack.hxx \
	src/protocol/ArgParser.cxx src/protocol/ArgParser.hxx \
	src/protocol/Result.cxx src/protocol/Result.hxx \
	src/command/Request.hxx \
	src/command/CommandResult.hxx \
	src/command/CommandError.cxx src/command/CommandError.hxx \
	src/command/AllCommands.cxx src/command/AllCommands.hxx \
	src/command/QueueCommands.cxx src/command/QueueCommands.hxx \
	src/command/TagCommands.cxx src/command/TagCommands.hxx \
	src/command/PlayerCommands.cxx src/command/PlayerCommands.hxx \
	src/command/PlaylistCommands.cxx src/command/PlaylistCommands.hxx \
	src/command/FileCommands.cxx src/command/FileCommands.hxx \
	src/command/OutputCommands.cxx src/command/OutputCommands.hxx \
	src/command/MessageCommands.cxx src/command/MessageCommands.hxx \
	src/command/ClientCommands.cxx src/command/ClientCommands.hxx \
	src/command/PartitionCommands.cxx src/command/PartitionCommands.hxx \
	src/command/OtherCommands.cxx src/command/OtherCommands.hxx \
	src/command/CommandListBuilder.cxx src/command/CommandListBuilder.hxx \
	src/Idle.cxx src/Idle.hxx \
	src/IdleFlags.cxx src/IdleFlags.hxx \
	src/decoder/DecoderError.cxx src/decoder/DecoderError.hxx \
	src/decoder/DecoderThread.cxx src/decoder/DecoderThread.hxx \
	src/decoder/DecoderCommand.hxx \
	src/decoder/DecoderControl.cxx src/decoder/DecoderControl.hxx \
	src/decoder/Client.hxx \
	src/decoder/DecoderPlugin.hxx \
	src/decoder/Bridge.cxx src/decoder/Bridge.hxx \
	src/decoder/DecoderPrint.cxx src/decoder/DecoderPrint.hxx \
	src/filter/FilterConfig.cxx src/filter/FilterConfig.hxx \
	src/filter/FilterPlugin.cxx src/filter/FilterPlugin.hxx \
	src/filter/FilterInternal.hxx \
	src/filter/FilterRegistry.cxx src/filter/FilterRegistry.hxx \
	src/filter/Observer.cxx src/filter/Observer.hxx \
	src/client/Client.cxx src/client/Client.hxx \
	src/client/ClientInternal.hxx \
	src/client/ClientEvent.cxx \
	src/client/ClientExpire.cxx \
	src/client/ClientGlobal.cxx \
	src/client/ClientIdle.cxx \
	src/client/ClientList.cxx src/client/ClientList.hxx \
	src/client/ClientNew.cxx \
	src/client/ClientProcess.cxx \
	src/client/ClientRead.cxx \
	src/client/ClientWrite.cxx \
	src/client/ClientMessage.cxx src/client/ClientMessage.hxx \
	src/client/ClientSubscribe.cxx \
	src/client/ClientFile.cxx \
	src/client/Response.cxx src/client/Response.hxx \
	src/Listen.cxx src/Listen.hxx \
	src/LogInit.cxx src/LogInit.hxx \
	src/LogBackend.cxx src/LogBackend.hxx \
	src/Log.cxx src/Log.hxx src/LogV.hxx \
	src/LogLevel.hxx \
	src/ls.cxx src/ls.hxx \
	src/Instance.cxx src/Instance.hxx \
	src/win32/Win32Main.cxx \
	src/MixRampInfo.hxx \
	src/MusicBuffer.cxx src/MusicBuffer.hxx \
	src/MusicPipe.cxx src/MusicPipe.hxx \
	src/MusicChunk.cxx src/MusicChunk.hxx \
	src/Mapper.cxx src/Mapper.hxx \
	src/Partition.cxx src/Partition.hxx \
	src/Permission.cxx src/Permission.hxx \
	src/player/CrossFade.cxx src/player/CrossFade.hxx \
	src/player/Thread.cxx src/player/Thread.hxx \
	src/player/Control.cxx src/player/Control.hxx \
	src/player/Listener.hxx \
	src/PlaylistError.cxx src/PlaylistError.hxx \
	src/PlaylistPrint.cxx src/PlaylistPrint.hxx \
	src/PlaylistSave.cxx src/PlaylistSave.hxx \
	src/playlist/PlaylistStream.cxx src/playlist/PlaylistStream.hxx \
	src/playlist/PlaylistMapper.cxx src/playlist/PlaylistMapper.hxx \
	src/playlist/PlaylistAny.cxx src/playlist/PlaylistAny.hxx \
	src/playlist/PlaylistSong.cxx src/playlist/PlaylistSong.hxx \
	src/playlist/PlaylistQueue.cxx src/playlist/PlaylistQueue.hxx \
	src/playlist/Print.cxx src/playlist/Print.hxx \
	src/BulkEdit.hxx \
	src/db/PlaylistVector.cxx src/db/PlaylistVector.hxx \
	src/db/PlaylistInfo.hxx \
	src/queue/IdTable.hxx \
	src/queue/Queue.cxx src/queue/Queue.hxx \
	src/queue/QueuePrint.cxx src/queue/QueuePrint.hxx \
	src/queue/QueueSave.cxx src/queue/QueueSave.hxx \
	src/queue/Playlist.cxx src/queue/Playlist.hxx \
	src/queue/PlaylistControl.cxx \
	src/queue/PlaylistEdit.cxx \
	src/queue/PlaylistTag.cxx \
	src/queue/PlaylistState.cxx src/queue/PlaylistState.hxx \
	src/queue/Listener.hxx \
	src/PluginUnavailable.hxx \
	src/ReplayGainGlobal.cxx src/ReplayGainGlobal.hxx \
	src/DetachedSong.cxx src/DetachedSong.hxx \
	src/LocateUri.cxx src/LocateUri.hxx \
	src/SongUpdate.cxx \
	src/SongLoader.cxx src/SongLoader.hxx \
	src/SongPrint.cxx src/SongPrint.hxx \
	src/SongSave.cxx src/SongSave.hxx \
	src/StateFile.cxx src/StateFile.hxx \
	src/Stats.cxx src/Stats.hxx \
	src/TagPrint.cxx src/TagPrint.hxx \
	src/TagSave.cxx src/TagSave.hxx \
	src/TagFile.cxx src/TagFile.hxx \
	src/TagStream.cxx src/TagStream.hxx \
	src/TimePrint.cxx src/TimePrint.hxx \
	src/mixer/Volume.cxx src/mixer/Volume.hxx \
	src/Chrono.hxx \
	src/SongFilter.cxx src/SongFilter.hxx \
	src/PlaylistFile.cxx src/PlaylistFile.hxx

if ANDROID
else
libmpd_a_SOURCES += \
	src/unix/SignalHandlers.cxx src/unix/SignalHandlers.hxx \
	src/CommandLine.cxx src/CommandLine.hxx

if ENABLE_DAEMON
libmpd_a_SOURCES += \
	src/unix/Daemon.cxx src/unix/Daemon.hxx \
	src/unix/PidFile.hxx
endif

endif

if ENABLE_DATABASE
libmpd_a_SOURCES += \
	src/queue/PlaylistUpdate.cxx \
	src/command/StorageCommands.cxx src/command/StorageCommands.hxx \
	src/command/DatabaseCommands.cxx src/command/DatabaseCommands.hxx \
	src/db/Count.cxx src/db/Count.hxx \
	src/db/LightSong.cxx src/db/LightSong.hxx \
	src/db/LightDirectory.hxx \
	src/db/update/UpdateDomain.cxx src/db/update/UpdateDomain.hxx \
	src/db/update/Service.cxx src/db/update/Service.hxx \
	src/db/update/Queue.cxx src/db/update/Queue.hxx \
	src/db/update/UpdateIO.cxx src/db/update/UpdateIO.hxx \
	src/db/update/Editor.cxx src/db/update/Editor.hxx \
	src/db/update/Walk.cxx src/db/update/Walk.hxx \
	src/db/update/UpdateSong.cxx \
	src/db/update/Container.cxx \
	src/db/update/Remove.cxx src/db/update/Remove.hxx \
	src/db/update/ExcludeList.cxx src/db/update/ExcludeList.hxx \
	src/db/Uri.hxx \
	src/db/DatabaseGlue.cxx src/db/DatabaseGlue.hxx \
	src/db/Configured.cxx src/db/Configured.hxx \
	src/db/DatabaseSong.cxx src/db/DatabaseSong.hxx \
	src/db/DatabasePrint.cxx src/db/DatabasePrint.hxx \
	src/db/DatabaseQueue.cxx src/db/DatabaseQueue.hxx \
	src/db/DatabasePlaylist.cxx src/db/DatabasePlaylist.hxx \
	src/db/DatabaseError.hxx \
	src/db/DatabaseLock.cxx src/db/DatabaseLock.hxx \
	src/db/DatabasePlugin.hxx \
	src/db/Interface.hxx \
	src/db/Stats.hxx \
	src/db/DatabaseListener.hxx \
	src/db/Visitor.hxx \
	src/db/Selection.cxx src/db/Selection.hxx
endif

CURL_SOURCES = \
	src/lib/curl/Version.cxx src/lib/curl/Version.hxx \
	src/lib/curl/Global.cxx src/lib/curl/Global.hxx \
	src/lib/curl/Request.cxx src/lib/curl/Request.hxx \
	src/lib/curl/Handler.hxx \
	src/lib/curl/Easy.hxx \
	src/lib/curl/Multi.hxx \
	src/lib/curl/Slist.hxx

UPNP_SOURCES = \
	src/lib/upnp/Init.cxx src/lib/upnp/Init.hxx \
	src/lib/upnp/ClientInit.cxx src/lib/upnp/ClientInit.hxx \
	src/lib/upnp/Device.cxx src/lib/upnp/Device.hxx \
	src/lib/upnp/ContentDirectoryService.cxx src/lib/upnp/ContentDirectoryService.hxx \
	src/lib/upnp/Discovery.cxx src/lib/upnp/Discovery.hxx \
	src/lib/upnp/ixmlwrap.cxx src/lib/upnp/ixmlwrap.hxx \
	src/lib/upnp/Callback.hxx \
	src/lib/upnp/Util.cxx src/lib/upnp/Util.hxx \
	src/lib/upnp/UniqueIxml.hxx \
	src/lib/upnp/WorkQueue.hxx \
	src/lib/upnp/Action.hxx

ALSA_SOURCES = \
	src/lib/alsa/Version.cxx src/lib/alsa/Version.hxx \
	src/lib/alsa/NonBlock.cxx src/lib/alsa/NonBlock.hxx

#
# Android native library
#

if ANDROID

noinst_LIBRARIES += libjava.a
libjava_a_SOURCES = \
	src/java/Class.hxx \
	src/java/Exception.hxx \
	src/java/Global.cxx src/java/Global.hxx \
	src/java/Object.hxx \
	src/java/Ref.hxx \
	src/java/File.cxx src/java/File.hxx \
	src/java/String.cxx src/java/String.hxx

noinst_LIBRARIES += libandroid.a
libandroid_a_SOURCES = \
	src/android/Context.cxx src/android/Context.hxx \
	src/android/Environment.cxx src/android/Environment.hxx
libandroid_a_CPPFLAGS = $(AM_CPPFLAGS) -Iandroid/build/include

noinst_LIBRARIES += libmain.a
libmain_a_SOURCES = \
	src/Main.cxx src/Main.hxx
libmain_a_CPPFLAGS = $(AM_CPPFLAGS) -Iandroid/build/include

src_mpd_LDADD += libandroid.a libjava.a

all-local: android/build/bin/$(APK_NAME)-debug.apk
clean-local:
	rm -rf android/build

libmpd.so: $(filter %.a,$(src_mpd_LDADD)) libmain.a
	$(AM_V_CXXLD)$(CXXLD) -shared -Wl,--no-undefined,-shared,-Bsymbolic -llog -lz -o $@ $(AM_CXXFLAGS) $(CXXFLAGS) $(LDFLAGS) src/libmain_a-Main.o $(src_mpd_LDADD) $(LIBS)

android/build/build.xml: android/AndroidManifest.xml
	rm -rf android/build
	mkdir -p android/build/include android/build/res android/build/src/org
	ln -s $(abs_srcdir)/android/AndroidManifest.xml $(abs_srcdir)/android/custom_rules.xml android/build
	ln -s $(abs_srcdir)/android/src android/build/src/org/musicpd
	ln -s $(abs_srcdir)/android/res/values $(abs_srcdir)/android/res/layout android/build/res
	$(ANDROID_SDK)/tools/android update project --path android/build --target android-17 --name $(APK_NAME)

android/build/bin/classes/org/musicpd/Bridge.class: android/src/Bridge.java android/build/build.xml android/build/res/drawable/icon.png
	cd android/build && ant compile-jni-classes

android/build/include/org_musicpd_Bridge.h: android/build/bin/classes/org/musicpd/Bridge.class
	javah -classpath $(ANDROID_SDK)/platforms/android-17/android.jar:android/build/bin/classes -d $(@D) org.musicpd.Bridge

BUILT_SOURCES = android/build/include/org_musicpd_Bridge.h

android/build/libs/armeabi-v7a/libmpd.so: libmpd.so android/build/build.xml
	mkdir -p $(@D)
	rm -f $@
	$(STRIP) -o $@ $<

android/build/res/drawable/icon.png: mpd.svg
	mkdir -p $(@D)
	rsvg-convert --width=48 --height=48 $< -o $@

APK_DEPS = android/build/res/drawable/icon.png \
	android/build/libs/armeabi-v7a/libmpd.so \
	$(wildcard $(srcdir)/android/src/*.java) \
	android/build/build.xml

android/build/bin/$(APK_NAME)-debug.apk: $(APK_DEPS)
	cd android/build && ant nodeps debug

android/build/bin/$(APK_NAME)-release-unsigned.apk: $(APK_DEPS)
	cd android/build && ant nodeps release

android/build/bin/$(APK_NAME)-release-unaligned.apk: android/build/bin/$(APK_NAME)-release-unsigned.apk
	jarsigner -digestalg SHA1 -sigalg MD5withRSA -storepass:env ANDROID_KEYSTORE_PASS -keystore $(ANDROID_KEYSTORE) -signedjar $@ $< $(ANDROID_KEY_ALIAS)

ANDROID_SDK_BUILD_TOOLS_VERSION = 20.0.0

android/build/bin/$(APK_NAME).apk: android/build/bin/$(APK_NAME)-release-unaligned.apk
	$(ANDROID_SDK)/build-tools/$(ANDROID_SDK_BUILD_TOOLS_VERSION)/zipalign -f 4 $< $@

endif

#
# Windows resource file
#

win32/res/mpd.$(OBJEXT): %.$(OBJEXT): %.rc
	$(WINDRES) -i $< -o $@

if HAVE_WINDOWS
noinst_DATA = win32/res/mpd.rc

EXTRA_src_mpd_DEPENDENCIES = win32/res/mpd.$(OBJEXT)
src_mpd_LDFLAGS = -Wl,win32/res/mpd.$(OBJEXT)
endif

#
# Haiku resource file
#

src/haiku/mpd.rsrc: src/haiku/mpd.rdef
	$(RC) -o $@ $<

if ENABLE_HAIKU
noinst_DATA = src/haiku/mpd.rdef

EXTRA_src_mpd_DEPENDENCIES = src/haiku/mpd.rsrc

src/mpd.haiku-rsrc-done: src/mpd src/haiku/mpd.rsrc
	$(XRES) -o src/mpd src/haiku/mpd.rsrc
	@touch src/mpd.haiku-rsrc-done

all-local: src/mpd.haiku-rsrc-done

clean-local:
	rm -rf src/haiku/mpd.rsrc src/mpd.haiku-rsrc-done
endif

if ENABLE_DATABASE
if ENABLE_INOTIFY
libmpd_a_SOURCES += \
	src/db/update/InotifyDomain.cxx src/db/update/InotifyDomain.hxx \
	src/db/update/InotifySource.cxx src/db/update/InotifySource.hxx \
	src/db/update/InotifyQueue.cxx src/db/update/InotifyQueue.hxx \
	src/db/update/InotifyUpdate.cxx src/db/update/InotifyUpdate.hxx
endif
endif

if ENABLE_SQLITE
libmpd_a_SOURCES += \
	src/command/StickerCommands.cxx src/command/StickerCommands.hxx \
	src/lib/sqlite/Error.cxx src/lib/sqlite/Error.hxx \
	src/lib/sqlite/Util.hxx \
	src/sticker/Match.hxx \
	src/sticker/StickerDatabase.cxx src/sticker/StickerDatabase.hxx \
	src/sticker/StickerPrint.cxx src/sticker/StickerPrint.hxx \
	src/sticker/SongSticker.cxx src/sticker/SongSticker.hxx
endif

# Generic utility library

libutil_a_SOURCES = \
	src/util/Exception.cxx src/util/Exception.hxx \
	src/util/RuntimeError.hxx \
	src/util/Macros.hxx \
	src/util/BindMethod.hxx \
	src/util/Cast.hxx \
	src/util/Chrono.hxx \
	src/util/Clamp.hxx \
	src/util/DeleteDisposer.hxx \
	src/util/Alloc.cxx src/util/Alloc.hxx \
	src/util/AllocatedArray.hxx \
	src/util/VarSize.hxx \
	src/util/ScopeExit.hxx \
	src/util/Domain.hxx \
	src/util/ReusableArray.hxx \
	src/util/ASCII.hxx \
	src/util/UTF8.cxx src/util/UTF8.hxx \
	src/util/CharUtil.hxx \
	src/util/NumberParser.hxx \
	src/util/MimeType.cxx src/util/MimeType.hxx \
	src/util/StringBuffer.hxx \
	src/util/StringPointer.hxx \
	src/util/StringView.cxx src/util/StringView.hxx \
	src/util/ConcatString.hxx \
	src/util/AllocatedString.cxx src/util/AllocatedString.hxx \
	src/util/TruncateString.cxx src/util/TruncateString.hxx \
	src/util/StringStrip.cxx src/util/StringStrip.hxx \
	src/util/StringUtil.cxx src/util/StringUtil.hxx \
	src/util/StringCompare.cxx src/util/StringCompare.hxx \
	src/util/WStringCompare.cxx src/util/WStringCompare.hxx \
	src/util/StringAPI.hxx \
	src/util/WStringAPI.hxx \
	src/util/DivideString.cxx src/util/DivideString.hxx \
	src/util/SplitString.cxx src/util/SplitString.hxx \
	src/util/IterableSplitString.hxx \
	src/util/FormatString.cxx src/util/FormatString.hxx \
	src/util/Tokenizer.cxx src/util/Tokenizer.hxx \
	src/util/TextFile.hxx \
	src/util/TimeParser.cxx src/util/TimeParser.hxx \
	src/util/UriUtil.cxx src/util/UriUtil.hxx \
	src/util/Manual.hxx \
	src/util/RefCount.hxx \
	src/util/StaticFifoBuffer.hxx \
	src/util/ForeignFifoBuffer.hxx \
	src/util/DynamicFifoBuffer.hxx \
	src/util/ConstBuffer.hxx \
	src/util/WritableBuffer.hxx \
	src/util/CircularBuffer.hxx \
	src/util/LazyRandomEngine.cxx src/util/LazyRandomEngine.hxx \
	src/util/SliceBuffer.hxx \
	src/util/HugeAllocator.cxx src/util/HugeAllocator.hxx \
	src/util/PeakBuffer.cxx src/util/PeakBuffer.hxx \
	src/util/OptionParser.cxx src/util/OptionParser.hxx \
	src/util/OptionDef.hxx \
	src/util/ByteReverse.cxx src/util/ByteReverse.hxx \
	src/util/format.c src/util/format.h \
	src/util/bit_reverse.c src/util/bit_reverse.h

# Multi-threading library

libthread_a_SOURCES = \
	src/thread/Util.cxx src/thread/Util.hxx \
	src/thread/Name.hxx \
	src/thread/Slack.hxx \
	src/thread/Mutex.hxx \
	src/thread/PosixMutex.hxx \
	src/thread/CriticalSection.hxx \
	src/thread/Cond.hxx \
	src/thread/PosixCond.hxx \
	src/thread/WindowsCond.hxx \
	src/thread/Thread.cxx src/thread/Thread.hxx \
	src/thread/Id.hxx

# Networking library

libnet_a_SOURCES = \
	src/net/Features.hxx \
	src/net/ToString.cxx src/net/ToString.hxx \
	src/net/Resolver.cxx src/net/Resolver.hxx \
	src/net/StaticSocketAddress.cxx src/net/StaticSocketAddress.hxx \
	src/net/AllocatedSocketAddress.cxx src/net/AllocatedSocketAddress.hxx \
	src/net/SocketAddress.cxx src/net/SocketAddress.hxx \
	src/net/SocketUtil.cxx src/net/SocketUtil.hxx \
	src/net/SocketDescriptor.cxx src/net/SocketDescriptor.hxx \
	src/net/UniqueSocketDescriptor.hxx \
	src/net/SocketError.cxx src/net/SocketError.hxx

# System library

libsystem_a_SOURCES = \
	src/system/ByteOrder.hxx \
	src/system/Error.hxx \
	src/system/FatalError.cxx src/system/FatalError.hxx \
	src/system/FileDescriptor.cxx src/system/FileDescriptor.hxx \
	src/system/UniqueFileDescriptor.hxx \
	src/system/fd_util.c src/system/fd_util.h \
	src/system/EventPipe.cxx src/system/EventPipe.hxx \
	src/system/EventFD.cxx src/system/EventFD.hxx \
	src/system/SignalFD.cxx src/system/SignalFD.hxx \
	src/system/EPollFD.cxx src/system/EPollFD.hxx \
	src/system/PeriodClock.hxx \
	src/system/Clock.cxx src/system/Clock.hxx

# Event loop library

libevent_a_SOURCES = \
	src/event/WakeFD.hxx \
	src/event/PollGroup.hxx \
	src/event/PollGroupEPoll.hxx \
	src/event/PollGroupPoll.hxx src/event/PollGroupPoll.cxx \
	src/event/PollGroupWinSelect.hxx src/event/PollGroupWinSelect.cxx \
	src/event/PollResultGeneric.hxx \
	src/event/SignalMonitor.hxx src/event/SignalMonitor.cxx \
	src/event/TimeoutMonitor.hxx src/event/TimeoutMonitor.cxx \
	src/event/IdleMonitor.hxx src/event/IdleMonitor.cxx \
	src/event/DeferredMonitor.hxx src/event/DeferredMonitor.cxx \
	src/event/DeferredCall.hxx \
	src/event/MaskMonitor.hxx src/event/MaskMonitor.cxx \
	src/event/SocketMonitor.cxx src/event/SocketMonitor.hxx \
	src/event/BufferedSocket.cxx src/event/BufferedSocket.hxx \
	src/event/FullyBufferedSocket.cxx src/event/FullyBufferedSocket.hxx \
	src/event/MultiSocketMonitor.cxx src/event/MultiSocketMonitor.hxx \
	src/event/ServerSocket.cxx src/event/ServerSocket.hxx \
	src/event/Call.hxx src/event/Call.cxx \
	src/event/Thread.cxx src/event/Thread.hxx \
	src/event/Loop.cxx src/event/Loop.hxx

# UTF-8 library

libicu_a_SOURCES = \
	src/lib/icu/Collate.cxx src/lib/icu/Collate.hxx \
	src/lib/icu/Converter.cxx src/lib/icu/Converter.hxx

if HAVE_ICU
libicu_a_SOURCES += \
	src/lib/icu/Util.cxx src/lib/icu/Util.hxx \
	src/lib/icu/Init.cxx src/lib/icu/Init.hxx
endif

if HAVE_WINDOWS
libicu_a_SOURCES += \
	src/lib/icu/Win32.cxx src/lib/icu/Win32.hxx
endif

libicu_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ICU_CFLAGS)

ICU_LDADD = libicu.a $(ICU_LIBS)

# PCM library

libpcm_a_SOURCES = \
	src/CheckAudioFormat.cxx src/CheckAudioFormat.hxx \
	src/AudioFormat.cxx src/AudioFormat.hxx \
	src/AudioParser.cxx src/AudioParser.hxx \
	src/pcm/SampleFormat.cxx src/pcm/SampleFormat.hxx \
	src/pcm/Traits.hxx \
	src/pcm/Interleave.cxx src/pcm/Interleave.hxx \
	src/pcm/PcmBuffer.cxx src/pcm/PcmBuffer.hxx \
	src/pcm/PcmExport.cxx src/pcm/PcmExport.hxx \
	src/pcm/PcmConvert.cxx src/pcm/PcmConvert.hxx \
	src/pcm/PcmDop.cxx src/pcm/PcmDop.hxx \
	src/pcm/Volume.cxx src/pcm/Volume.hxx \
	src/pcm/Silence.cxx src/pcm/Silence.hxx \
	src/pcm/PcmMix.cxx src/pcm/PcmMix.hxx \
	src/pcm/PcmChannels.cxx src/pcm/PcmChannels.hxx \
	src/pcm/PcmPack.cxx src/pcm/PcmPack.hxx \
	src/pcm/PcmFormat.cxx src/pcm/PcmFormat.hxx \
	src/pcm/FloatConvert.hxx \
	src/pcm/ShiftConvert.hxx \
	src/pcm/Neon.hxx \
	src/pcm/FormatConverter.cxx src/pcm/FormatConverter.hxx \
	src/pcm/ChannelsConverter.cxx src/pcm/ChannelsConverter.hxx \
	src/pcm/Order.cxx src/pcm/Order.hxx \
	src/pcm/Resampler.hxx \
	src/pcm/GlueResampler.cxx src/pcm/GlueResampler.hxx \
	src/pcm/FallbackResampler.cxx src/pcm/FallbackResampler.hxx \
	src/pcm/ConfiguredResampler.cxx src/pcm/ConfiguredResampler.hxx \
	src/pcm/PcmDither.cxx src/pcm/PcmDither.hxx \
	src/pcm/PcmPrng.hxx \
	src/pcm/PcmUtils.hxx
libpcm_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(SOXR_CFLAGS) \
	$(LIBSAMPLERATE_CFLAGS)

PCM_LIBS = \
	libpcm.a \
	$(SOXR_LIBS) \
	$(LIBSAMPLERATE_LIBS)

if ENABLE_DSD
libpcm_a_SOURCES += \
	src/pcm/Dsd16.cxx src/pcm/Dsd16.hxx \
	src/pcm/Dsd32.cxx src/pcm/Dsd32.hxx \
	src/pcm/PcmDsd.cxx src/pcm/PcmDsd.hxx \
	src/pcm/dsd2pcm/dsd2pcm.c src/pcm/dsd2pcm/dsd2pcm.h
endif

if ENABLE_LIBSAMPLERATE
libpcm_a_SOURCES += \
	src/pcm/LibsamplerateResampler.cxx src/pcm/LibsamplerateResampler.hxx
endif

if ENABLE_SOXR
libpcm_a_SOURCES += \
	src/pcm/SoxrResampler.cxx src/pcm/SoxrResampler.hxx
endif

# Xiph codec support library

if HAVE_XIPH

noinst_LIBRARIES += libxiph.a

libxiph_a_SOURCES = \
	src/lib/xiph/VorbisComment.hxx \
	src/lib/xiph/VorbisComments.cxx src/lib/xiph/VorbisComments.hxx \
	src/lib/xiph/XiphTags.cxx src/lib/xiph/XiphTags.hxx
libxiph_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(OGG_CFLAGS)

if HAVE_OGG
libxiph_a_SOURCES += \
	src/lib/xiph/OggVisitor.cxx src/lib/xiph/OggVisitor.hxx \
	src/lib/xiph/OggSerial.cxx src/lib/xiph/OggSerial.hxx \
	src/lib/xiph/OggSyncState.cxx src/lib/xiph/OggSyncState.hxx \
	src/lib/xiph/OggFind.cxx src/lib/xiph/OggFind.hxx \
	src/lib/xiph/OggPage.hxx \
	src/lib/xiph/OggPacket.cxx src/lib/xiph/OggPacket.hxx \
	src/lib/xiph/OggStreamState.hxx
endif

XIPH_LIBS = libxiph.a \
	$(OGG_LIBS)

endif

# File system library

FS_LIBS = libfs.a

libfs_a_SOURCES = \
	src/fs/io/Reader.hxx \
	src/fs/io/PeekReader.cxx src/fs/io/PeekReader.hxx \
	src/fs/io/FileReader.cxx src/fs/io/FileReader.hxx \
	src/fs/io/BufferedReader.cxx src/fs/io/BufferedReader.hxx \
	src/fs/io/TextFile.cxx src/fs/io/TextFile.hxx \
	src/fs/io/OutputStream.hxx \
	src/fs/io/StdioOutputStream.hxx \
	src/fs/io/FileOutputStream.cxx src/fs/io/FileOutputStream.hxx \
	src/fs/io/BufferedOutputStream.cxx src/fs/io/BufferedOutputStream.hxx \
	src/fs/Domain.cxx src/fs/Domain.hxx \
	src/fs/Limits.hxx \
	src/fs/Traits.cxx src/fs/Traits.hxx \
	src/fs/Config.cxx src/fs/Config.hxx \
	src/fs/Charset.cxx src/fs/Charset.hxx \
	src/fs/Path.cxx src/fs/Path2.cxx src/fs/Path.hxx \
	src/fs/AllocatedPath.cxx src/fs/AllocatedPath.hxx \
	src/fs/NarrowPath.hxx \
	src/fs/FileSystem.cxx src/fs/FileSystem.hxx \
	src/fs/FileInfo.hxx \
	src/fs/Glob.hxx \
	src/fs/StandardDirectory.cxx src/fs/StandardDirectory.hxx \
	src/fs/CheckFile.cxx src/fs/CheckFile.hxx \
	src/fs/DirectoryReader.cxx src/fs/DirectoryReader.hxx
libfs_a_CPPFLAGS = $(AM_CPPFLAGS) $(ZLIB_CFLAGS)

if ENABLE_ZLIB
libfs_a_SOURCES += \
	src/lib/zlib/Error.cxx src/lib/zlib/Error.hxx \
	src/fs/io/GunzipReader.cxx src/fs/io/GunzipReader.hxx \
	src/fs/io/AutoGunzipReader.cxx src/fs/io/AutoGunzipReader.hxx \
	src/fs/io/GzipOutputStream.cxx src/fs/io/GzipOutputStream.hxx
FS_LIBS += $(ZLIB_LIBS)
endif

if HAVE_WINDOWS
# for PathMatchSpec()
FS_LIBS += -lshlwapi
endif

# Storage library

SMBCLIENT_SOURCES = \
	src/lib/smbclient/Domain.cxx src/lib/smbclient/Domain.hxx \
	src/lib/smbclient/Mutex.cxx src/lib/smbclient/Mutex.hxx \
	src/lib/smbclient/Init.cxx src/lib/smbclient/Init.hxx

NFS_SOURCES = \
	src/lib/nfs/Callback.hxx \
	src/lib/nfs/Cancellable.hxx \
	src/lib/nfs/Lease.hxx \
	src/lib/nfs/Connection.cxx src/lib/nfs/Connection.hxx \
	src/lib/nfs/Manager.cxx src/lib/nfs/Manager.hxx \
	src/lib/nfs/Glue.cxx src/lib/nfs/Glue.hxx \
	src/lib/nfs/Base.cxx src/lib/nfs/Base.hxx \
	src/lib/nfs/FileReader.cxx src/lib/nfs/FileReader.hxx \
	src/lib/nfs/Blocking.cxx src/lib/nfs/Blocking.hxx

if ENABLE_DATABASE

noinst_LIBRARIES += libstorage.a

libstorage_a_SOURCES = \
	src/storage/StoragePlugin.hxx \
	src/storage/Registry.cxx src/storage/Registry.hxx \
	src/storage/StorageInterface.cxx src/storage/StorageInterface.hxx \
	src/storage/CompositeStorage.cxx src/storage/CompositeStorage.hxx \
	src/storage/MemoryDirectoryReader.cxx src/storage/MemoryDirectoryReader.hxx \
	src/storage/Configured.cxx src/storage/Configured.hxx \
	src/storage/plugins/LocalStorage.cxx src/storage/plugins/LocalStorage.hxx \
	src/storage/FileInfo.hxx

libstorage_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(NFS_CFLAGS) \
	$(SMBCLIENT_CFLAGS)

STORAGE_LIBS = \
	libstorage.a \
	$(CURL_LIBS) \
	$(EXPAT_LIBS) \
	$(NFS_LIBS) \
	$(SMBCLIENT_LIBS)

if ENABLE_SMBCLIENT
libstorage_a_SOURCES += \
	$(SMBCLIENT_SOURCES) \
	src/storage/plugins/SmbclientStorage.cxx src/storage/plugins/SmbclientStorage.hxx
endif

if ENABLE_NFS
libstorage_a_SOURCES += \
	$(NFS_SOURCES) \
	src/storage/plugins/NfsStorage.cxx src/storage/plugins/NfsStorage.hxx
endif

if ENABLE_WEBDAV
libstorage_a_SOURCES += \
	src/lib/expat/ExpatParser.cxx \
	src/storage/plugins/CurlStorage.cxx src/storage/plugins/CurlStorage.hxx
endif

endif

# neighbor plugins

if ENABLE_NEIGHBOR_PLUGINS

libmpd_a_SOURCES += \
	src/command/NeighborCommands.cxx \
	src/command/NeighborCommands.hxx

noinst_LIBRARIES += libneighbor.a

libneighbor_a_SOURCES = \
	src/neighbor/Registry.cxx src/neighbor/Registry.hxx \
	src/neighbor/Glue.cxx src/neighbor/Glue.hxx \
	src/neighbor/Info.hxx \
	src/neighbor/Listener.hxx \
	src/neighbor/Explorer.hxx \
	src/neighbor/NeighborPlugin.hxx

libneighbor_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(SMBCLIENT_CFLAGS)

if ENABLE_SMBCLIENT
libneighbor_a_SOURCES += \
	$(SMBCLIENT_SOURCES) \
	src/neighbor/plugins/SmbclientNeighborPlugin.cxx src/neighbor/plugins/SmbclientNeighborPlugin.hxx
endif

NEIGHBOR_LIBS = \
	$(SMBCLIENT_LIBS) \
	libneighbor.a

if ENABLE_UPNP
libneighbor_a_SOURCES += \
	$(UPNP_SOURCES) \
	src/neighbor/plugins/UpnpNeighborPlugin.cxx src/neighbor/plugins/UpnpNeighborPlugin.hxx
NEIGHBOR_LIBS += \
	$(EXPAT_LIBS) \
	$(UPNP_LIBS)
endif

endif

# database plugins

if ENABLE_DATABASE

noinst_LIBRARIES += libdb_plugins.a

libdb_plugins_a_SOURCES = \
	src/PlaylistDatabase.cxx src/PlaylistDatabase.hxx \
	src/db/Registry.cxx src/db/Registry.hxx \
	src/db/Helpers.cxx src/db/Helpers.hxx \
	src/db/UniqueTags.cxx src/db/UniqueTags.hxx \
	src/db/plugins/simple/DatabaseSave.cxx \
	src/db/plugins/simple/DatabaseSave.hxx \
	src/db/plugins/simple/DirectorySave.cxx \
	src/db/plugins/simple/DirectorySave.hxx \
	src/db/plugins/simple/Directory.cxx \
	src/db/plugins/simple/Directory.hxx \
	src/db/plugins/simple/Song.cxx \
	src/db/plugins/simple/Song.hxx \
	src/db/plugins/simple/SongSort.cxx \
	src/db/plugins/simple/SongSort.hxx \
	src/db/plugins/simple/Mount.cxx \
	src/db/plugins/simple/Mount.hxx \
	src/db/plugins/simple/PrefixedLightSong.hxx \
	src/db/plugins/simple/SimpleDatabasePlugin.cxx \
	src/db/plugins/simple/SimpleDatabasePlugin.hxx

if ENABLE_LIBMPDCLIENT
libdb_plugins_a_SOURCES += \
	src/db/plugins/ProxyDatabasePlugin.cxx src/db/plugins/ProxyDatabasePlugin.hxx
endif

DB_LIBS = \
	libdb_plugins.a \
	$(LIBMPDCLIENT_LIBS)

if ENABLE_UPNP
libdb_plugins_a_SOURCES += \
	$(UPNP_SOURCES) \
	src/db/plugins/upnp/UpnpDatabasePlugin.cxx src/db/plugins/upnp/UpnpDatabasePlugin.hxx \
	src/db/plugins/upnp/Tags.cxx src/db/plugins/upnp/Tags.hxx \
	src/db/plugins/upnp/ContentDirectoryService.cxx \
	src/db/plugins/upnp/Directory.cxx src/db/plugins/upnp/Directory.hxx \
	src/db/plugins/upnp/Object.cxx src/db/plugins/upnp/Object.hxx
DB_LIBS += \
	$(EXPAT_LIBS) \
	$(UPNP_LIBS)
endif

endif

# archive plugins

if ENABLE_ARCHIVE

noinst_LIBRARIES += libarchive.a

libmpd_a_SOURCES +=  \
	src/TagArchive.cxx src/TagArchive.hxx \
	src/db/update/Archive.cxx

libarchive_a_SOURCES = \
	src/archive/ArchiveDomain.cxx src/archive/ArchiveDomain.hxx \
	src/archive/ArchiveLookup.cxx src/archive/ArchiveLookup.hxx \
	src/archive/ArchiveList.cxx src/archive/ArchiveList.hxx \
	src/archive/ArchivePlugin.cxx src/archive/ArchivePlugin.hxx \
	src/archive/ArchiveVisitor.hxx \
	src/archive/ArchiveFile.hxx \
	src/input/plugins/ArchiveInputPlugin.cxx src/input/plugins/ArchiveInputPlugin.hxx
libarchive_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(BZ2_CFLAGS) \
	$(ISO9660_CFLAGS) \
	$(ZZIP_CFLAGS)

ARCHIVE_LIBS = \
	libarchive.a \
	$(BZ2_LIBS) \
	$(ISO9660_LIBS) \
	$(ZZIP_LIBS)

if ENABLE_BZ2
libarchive_a_SOURCES += \
	src/archive/plugins/Bzip2ArchivePlugin.cxx \
	src/archive/plugins/Bzip2ArchivePlugin.hxx
endif

if ENABLE_ZZIP
libarchive_a_SOURCES += \
	src/archive/plugins/ZzipArchivePlugin.cxx \
	src/archive/plugins/ZzipArchivePlugin.hxx
endif

if ENABLE_ISO9660
libarchive_a_SOURCES += \
	src/archive/plugins/Iso9660ArchivePlugin.cxx \
	src/archive/plugins/Iso9660ArchivePlugin.hxx
endif

else
ARCHIVE_LIBS =
endif

libbasic_a_SOURCES = \
	src/ReplayGainConfig.hxx \
	src/ReplayGainMode.cxx src/ReplayGainMode.hxx \
	src/ReplayGainInfo.cxx src/ReplayGainInfo.hxx

# configuration library

libconf_a_SOURCES = \
	src/config/ConfigDefaults.hxx \
	src/config/ConfigPath.cxx src/config/ConfigPath.hxx \
	src/config/Data.cxx src/config/Data.hxx \
	src/config/Block.cxx src/config/Block.hxx \
	src/config/Param.cxx src/config/Param.hxx \
	src/config/ConfigParser.cxx src/config/ConfigParser.hxx \
	src/config/ConfigGlobal.cxx src/config/ConfigGlobal.hxx \
	src/config/ConfigFile.cxx src/config/ConfigFile.hxx \
	src/config/ConfigTemplates.cxx src/config/ConfigTemplates.hxx \
	src/config/ConfigError.cxx src/config/ConfigError.hxx \
	src/config/ConfigOption.hxx

# tag plugins

libtag_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ID3TAG_CFLAGS)
TAG_LIBS = \
	libtag.a \
	$(ID3TAG_LIBS)

libtag_a_SOURCES =\
	src/tag/Type.h \
	src/tag/Tag.cxx src/tag/Tag.hxx \
	src/tag/Builder.cxx src/tag/Builder.hxx \
	src/tag/Item.hxx \
	src/tag/Handler.cxx src/tag/Handler.hxx \
	src/tag/Mask.hxx \
	src/tag/Settings.cxx src/tag/Settings.hxx \
	src/tag/Config.cxx src/tag/Config.hxx \
	src/tag/ParseName.cxx src/tag/ParseName.hxx \
	src/tag/Names.c \
	src/tag/FixString.cxx src/tag/FixString.hxx \
	src/tag/Pool.cxx src/tag/Pool.hxx \
	src/tag/Table.cxx src/tag/Table.hxx \
	src/tag/Set.cxx src/tag/Set.hxx \
	src/tag/Format.cxx src/tag/Format.hxx \
	src/tag/VorbisComment.cxx src/tag/VorbisComment.hxx \
	src/tag/ReplayGain.cxx src/tag/ReplayGain.hxx \
	src/tag/MixRamp.cxx src/tag/MixRamp.hxx \
	src/tag/Generic.cxx src/tag/Generic.hxx \
	src/tag/Id3MusicBrainz.cxx src/tag/Id3MusicBrainz.hxx \
	src/tag/ApeLoader.cxx src/tag/ApeLoader.hxx \
	src/tag/ApeReplayGain.cxx src/tag/ApeReplayGain.hxx \
	src/tag/ApeTag.cxx src/tag/ApeTag.hxx

if ENABLE_ID3TAG
libtag_a_SOURCES += \
	src/tag/Id3Load.cxx src/tag/Id3Load.hxx \
	src/tag/Id3Unique.hxx \
	src/tag/Id3Scan.cxx src/tag/Id3Scan.hxx \
	src/tag/Rva2.cxx src/tag/Rva2.hxx \
	src/tag/Riff.cxx src/tag/Riff.hxx \
	src/tag/Aiff.cxx src/tag/Aiff.hxx
endif

# ffmpeg

if ENABLE_FFMPEG
noinst_LIBRARIES += libffmpeg.a
libffmpeg_a_SOURCES = \
	src/lib/ffmpeg/Init.cxx src/lib/ffmpeg/Init.hxx \
	src/lib/ffmpeg/Time.hxx \
	src/lib/ffmpeg/Buffer.hxx \
	src/lib/ffmpeg/LogError.cxx src/lib/ffmpeg/LogError.hxx \
	src/lib/ffmpeg/LogCallback.cxx src/lib/ffmpeg/LogCallback.hxx \
	src/lib/ffmpeg/Error.cxx src/lib/ffmpeg/Error.hxx \
	src/lib/ffmpeg/Domain.cxx src/lib/ffmpeg/Domain.hxx
libffmpeg_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(FFMPEG_CFLAGS)
FFMPEG_LIBS2 = libffmpeg.a $(FFMPEG_LIBS)
endif

# decoder plugins

libdecoder_a_SOURCES = \
	src/decoder/plugins/PcmDecoderPlugin.cxx \
	src/decoder/plugins/PcmDecoderPlugin.hxx \
	src/decoder/DecoderAPI.cxx src/decoder/DecoderAPI.hxx \
	src/decoder/Reader.cxx src/decoder/Reader.hxx \
	src/decoder/DecoderBuffer.cxx src/decoder/DecoderBuffer.hxx \
	src/decoder/DecoderPlugin.cxx \
	src/decoder/DecoderList.cxx src/decoder/DecoderList.hxx
libdecoder_a_CPPFLAGS = $(AM_CPPFLAGS) \
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
	libdecoder.a \
	$(XIPH_LIBS) \
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
	$(FFMPEG_LIBS2) \
	$(MPCDEC_LIBS) \
	$(ADPLUG_LIBS) \
	$(FAAD_LIBS)

if ENABLE_DSD
libdecoder_a_SOURCES += \
	src/decoder/plugins/DsdiffDecoderPlugin.cxx \
	src/decoder/plugins/DsdiffDecoderPlugin.hxx \
	src/decoder/plugins/DsfDecoderPlugin.cxx \
	src/decoder/plugins/DsfDecoderPlugin.hxx \
	src/decoder/plugins/DsdLib.cxx \
	src/decoder/plugins/DsdLib.hxx
endif

if ENABLE_MAD
libdecoder_a_SOURCES += \
	src/decoder/plugins/MadDecoderPlugin.cxx \
	src/decoder/plugins/MadDecoderPlugin.hxx
endif

if ENABLE_MPG123
libdecoder_a_SOURCES += \
	src/decoder/plugins/Mpg123DecoderPlugin.cxx \
	src/decoder/plugins/Mpg123DecoderPlugin.hxx
endif

if ENABLE_MPCDEC
libdecoder_a_SOURCES += \
	src/decoder/plugins/MpcdecDecoderPlugin.cxx \
	src/decoder/plugins/MpcdecDecoderPlugin.hxx
endif

if ENABLE_OPUS
libdecoder_a_SOURCES += \
	src/decoder/plugins/OpusDomain.cxx src/decoder/plugins/OpusDomain.hxx \
	src/decoder/plugins/OpusReader.hxx \
	src/decoder/plugins/OpusHead.hxx \
	src/decoder/plugins/OpusHead.cxx \
	src/decoder/plugins/OpusTags.cxx \
	src/decoder/plugins/OpusTags.hxx \
	src/decoder/plugins/OpusDecoderPlugin.cxx \
	src/decoder/plugins/OpusDecoderPlugin.h
endif

if ENABLE_WAVPACK
libdecoder_a_SOURCES += \
	src/decoder/plugins/WavpackDecoderPlugin.cxx \
	src/decoder/plugins/WavpackDecoderPlugin.hxx
endif

if ENABLE_ADPLUG
libdecoder_a_SOURCES += \
	src/decoder/plugins/AdPlugDecoderPlugin.cxx \
	src/decoder/plugins/AdPlugDecoderPlugin.h
endif

if ENABLE_FAAD
libdecoder_a_SOURCES += \
	src/decoder/plugins/FaadDecoderPlugin.cxx src/decoder/plugins/FaadDecoderPlugin.hxx
endif

if HAVE_OGG
libdecoder_a_SOURCES += \
	src/decoder/plugins/OggDecoder.cxx src/decoder/plugins/OggDecoder.hxx
endif

if HAVE_XIPH
libdecoder_a_SOURCES += \
	src/decoder/plugins/OggCodec.cxx src/decoder/plugins/OggCodec.hxx
endif

if ENABLE_VORBIS_DECODER
libdecoder_a_SOURCES += \
	src/decoder/plugins/VorbisDomain.cxx src/decoder/plugins/VorbisDomain.hxx \
	src/decoder/plugins/VorbisDecoderPlugin.cxx src/decoder/plugins/VorbisDecoderPlugin.h
endif

if ENABLE_FLAC
libdecoder_a_SOURCES += \
	src/decoder/plugins/FlacInput.cxx src/decoder/plugins/FlacInput.hxx \
	src/decoder/plugins/FlacIOHandle.cxx src/decoder/plugins/FlacIOHandle.hxx \
	src/decoder/plugins/FlacMetadata.cxx src/decoder/plugins/FlacMetadata.hxx \
	src/decoder/plugins/FlacPcm.cxx src/decoder/plugins/FlacPcm.hxx \
	src/decoder/plugins/FlacDomain.cxx src/decoder/plugins/FlacDomain.hxx \
	src/decoder/plugins/FlacCommon.cxx src/decoder/plugins/FlacCommon.hxx \
	src/decoder/plugins/FlacStreamDecoder.hxx \
	src/decoder/plugins/FlacDecoderPlugin.cxx \
	src/decoder/plugins/FlacDecoderPlugin.h
endif

if ENABLE_AUDIOFILE
libdecoder_a_SOURCES += \
	src/decoder/plugins/AudiofileDecoderPlugin.cxx \
	src/decoder/plugins/AudiofileDecoderPlugin.hxx
endif

if ENABLE_MIKMOD_DECODER
libdecoder_a_SOURCES += \
	src/decoder/plugins/MikmodDecoderPlugin.cxx \
	src/decoder/plugins/MikmodDecoderPlugin.hxx
endif

if ENABLE_MODPLUG
libmodplug_decoder_plugin_a_SOURCES = \
	src/decoder/plugins/ModplugDecoderPlugin.cxx \
	src/decoder/plugins/ModplugDecoderPlugin.hxx
libmodplug_decoder_plugin_a_CXXFLAGS = $(AM_CXXFLAGS) $(MODPLUG_CFLAGS)
libmodplug_decoder_plugin_a_CPPFLAGS = $(AM_CPPFLAGS)
noinst_LIBRARIES += libmodplug_decoder_plugin.a
DECODER_LIBS += libmodplug_decoder_plugin.a $(MODPLUG_LIBS)
endif

if ENABLE_SIDPLAY
libdecoder_a_SOURCES += \
	src/decoder/plugins/SidplayDecoderPlugin.cxx \
	src/decoder/plugins/SidplayDecoderPlugin.hxx
endif

if ENABLE_FLUIDSYNTH
libdecoder_a_SOURCES += \
	src/decoder/plugins/FluidsynthDecoderPlugin.cxx \
	src/decoder/plugins/FluidsynthDecoderPlugin.hxx
endif

if ENABLE_WILDMIDI
libdecoder_a_SOURCES += \
	src/decoder/plugins/WildmidiDecoderPlugin.cxx \
	src/decoder/plugins/WildmidiDecoderPlugin.hxx
endif

if ENABLE_FFMPEG
libdecoder_a_SOURCES += \
	src/decoder/plugins/FfmpegIo.cxx \
	src/decoder/plugins/FfmpegIo.hxx \
	src/decoder/plugins/FfmpegMetaData.cxx \
	src/decoder/plugins/FfmpegMetaData.hxx \
	src/decoder/plugins/FfmpegDecoderPlugin.cxx \
	src/decoder/plugins/FfmpegDecoderPlugin.hxx
endif

if ENABLE_SNDFILE
libdecoder_a_SOURCES += \
	src/decoder/plugins/SndfileDecoderPlugin.cxx \
	src/decoder/plugins/SndfileDecoderPlugin.hxx
endif

if ENABLE_GME
libdecoder_a_SOURCES += \
	src/decoder/plugins/GmeDecoderPlugin.cxx src/decoder/plugins/GmeDecoderPlugin.hxx
endif

# encoder plugins

if ENABLE_ENCODER

noinst_LIBRARIES += libencoder_plugins.a

libencoder_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LAME_CFLAGS) \
	$(TWOLAME_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS)) \
	$(OPUS_CFLAGS) \
	$(SHINE_CFLAGS) \
	$(VORBISENC_CFLAGS)

ENCODER_LIBS = \
	libencoder_plugins.a \
	$(XIPH_LIBS) \
	$(LAME_LIBS) \
	$(TWOLAME_LIBS) \
	$(FLAC_LIBS) \
	$(OPUS_LIBS) \
	$(SHINE_LIBS) \
	$(VORBISENC_LIBS)

libencoder_plugins_a_SOURCES = \
	src/encoder/EncoderAPI.hxx \
	src/encoder/EncoderInterface.hxx \
	src/encoder/EncoderPlugin.hxx \
	src/encoder/ToOutputStream.cxx src/encoder/ToOutputStream.hxx \
	src/encoder/plugins/NullEncoderPlugin.cxx \
	src/encoder/plugins/NullEncoderPlugin.hxx \
	src/encoder/EncoderList.cxx src/encoder/EncoderList.hxx

if ENABLE_WAVE_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/WaveEncoderPlugin.cxx \
	src/encoder/plugins/WaveEncoderPlugin.hxx
endif

if HAVE_OGG
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/OggEncoder.hxx
endif

if ENABLE_VORBISENC
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/VorbisEncoderPlugin.cxx \
	src/encoder/plugins/VorbisEncoderPlugin.hxx
endif

if ENABLE_OPUS
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/OpusEncoderPlugin.cxx \
	src/encoder/plugins/OpusEncoderPlugin.hxx
endif

if ENABLE_LAME
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/LameEncoderPlugin.cxx \
	src/encoder/plugins/LameEncoderPlugin.hxx
endif

if ENABLE_TWOLAME
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/TwolameEncoderPlugin.cxx \
	src/encoder/plugins/TwolameEncoderPlugin.hxx
endif

if ENABLE_FLAC_ENCODER
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/FlacEncoderPlugin.cxx \
	src/encoder/plugins/FlacEncoderPlugin.hxx
endif

if ENABLE_SHINE
libencoder_plugins_a_SOURCES += \
	src/encoder/plugins/ShineEncoderPlugin.cxx \
	src/encoder/plugins/ShineEncoderPlugin.hxx
endif

else
ENCODER_LIBS =
endif


if HAVE_ZEROCONF
libmpd_a_SOURCES += \
	src/zeroconf/ZeroconfInternal.hxx \
	src/zeroconf/ZeroconfGlue.cxx src/zeroconf/ZeroconfGlue.hxx

if HAVE_AVAHI
libmpd_a_SOURCES += \
	src/zeroconf/AvahiPoll.cxx src/zeroconf/AvahiPoll.hxx \
	src/zeroconf/ZeroconfAvahi.cxx src/zeroconf/ZeroconfAvahi.hxx
endif

if HAVE_BONJOUR
libmpd_a_SOURCES += src/zeroconf/ZeroconfBonjour.cxx src/zeroconf/ZeroconfBonjour.hxx
endif
endif

#
# input plugins
#

libinput_a_SOURCES = \
	src/input/Domain.cxx src/input/Domain.hxx \
	src/input/Init.cxx src/input/Init.hxx \
	src/input/Registry.cxx src/input/Registry.hxx \
	src/input/Open.cxx \
	src/input/LocalOpen.cxx src/input/LocalOpen.hxx \
	src/input/Offset.hxx \
	src/input/InputStream.cxx src/input/InputStream.hxx \
	src/input/Ptr.hxx \
	src/input/InputPlugin.hxx \
	src/input/Reader.cxx src/input/Reader.hxx \
	src/input/TextInputStream.cxx src/input/TextInputStream.hxx \
	src/input/ThreadInputStream.cxx src/input/ThreadInputStream.hxx \
	src/input/AsyncInputStream.cxx src/input/AsyncInputStream.hxx \
	src/input/ProxyInputStream.cxx src/input/ProxyInputStream.hxx \
	src/input/plugins/RewindInputPlugin.cxx src/input/plugins/RewindInputPlugin.hxx \
	src/input/plugins/FileInputPlugin.cxx src/input/plugins/FileInputPlugin.hxx

libinput_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(CURL_CFLAGS) \
	$(SMBCLIENT_CFLAGS) \
	$(NFS_CFLAGS) \
	$(CDIO_PARANOIA_CFLAGS) \
	$(FFMPEG_CFLAGS) \
	$(MMS_CFLAGS)

INPUT_LIBS = \
	libinput.a \
	$(CURL_LIBS) \
	$(SMBCLIENT_LIBS) \
	$(NFS_LIBS) \
	$(CDIO_PARANOIA_LIBS) \
	$(FFMPEG_LIBS2) \
	$(MMS_LIBS)

if ENABLE_ALSA
libinput_a_SOURCES += \
	$(ALSA_SOURCES) \
	src/input/plugins/AlsaInputPlugin.cxx \
	src/input/plugins/AlsaInputPlugin.hxx
INPUT_LIBS += $(ALSA_LIBS)
endif


if ENABLE_CURL
libinput_a_SOURCES += \
	src/input/IcyInputStream.cxx src/input/IcyInputStream.hxx \
	src/input/plugins/CurlInputPlugin.cxx src/input/plugins/CurlInputPlugin.hxx \
	$(CURL_SOURCES) \
	src/IcyMetaDataParser.cxx src/IcyMetaDataParser.hxx
endif

if ENABLE_SMBCLIENT
libinput_a_SOURCES += \
	$(SMBCLIENT_SOURCES) \
	src/input/plugins/SmbclientInputPlugin.cxx src/input/plugins/SmbclientInputPlugin.hxx
endif

if ENABLE_NFS
libinput_a_SOURCES += \
	$(NFS_SOURCES) \
	src/input/plugins/NfsInputPlugin.cxx src/input/plugins/NfsInputPlugin.hxx
endif

if ENABLE_CDIO_PARANOIA
libinput_a_SOURCES += \
	src/input/plugins/CdioParanoiaInputPlugin.cxx \
	src/input/plugins/CdioParanoiaInputPlugin.hxx
endif

if ENABLE_FFMPEG
libinput_a_SOURCES += \
	src/input/plugins/FfmpegInputPlugin.cxx src/input/plugins/FfmpegInputPlugin.hxx
endif

if ENABLE_MMS
libinput_a_SOURCES += \
	src/input/plugins/MmsInputPlugin.cxx src/input/plugins/MmsInputPlugin.hxx
endif

liboutput_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(AO_CFLAGS) \
	$(ALSA_CFLAGS) \
	$(JACK_CFLAGS) \
	$(HAIKU_CFLAGS) \
	$(OPENAL_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(PULSE_CFLAGS) \
	$(SNDIO_CFLAGS) \
	$(SHOUT_CFLAGS)

OUTPUT_LIBS = \
	liboutput_plugins.a \
	$(LIBWRAP_LDFLAGS) \
	$(AO_LIBS) \
	$(ALSA_LIBS) \
	$(ROAR_LIBS) \
	$(JACK_LIBS) \
	$(HAIKU_LIBS) \
	$(OPENAL_LIBS) \
	$(SNDIO_LIBS) \
	$(SHOUT_LIBS)

OUTPUT_API_SRC = \
	src/output/Client.hxx \
	src/output/OutputAPI.hxx \
	src/output/Interface.hxx \
	src/output/Filtered.cxx src/output/Filtered.hxx \
	src/output/Registry.cxx src/output/Registry.hxx \
	src/output/MultipleOutputs.cxx src/output/MultipleOutputs.hxx \
	src/output/SharedPipeConsumer.cxx src/output/SharedPipeConsumer.hxx \
	src/output/Source.cxx src/output/Source.hxx \
	src/output/Thread.cxx \
	src/output/Domain.cxx src/output/Domain.hxx \
	src/output/Control.cxx src/output/Control.hxx \
	src/output/State.cxx src/output/State.hxx \
	src/output/Print.cxx src/output/Print.hxx \
	src/output/OutputCommand.cxx src/output/OutputCommand.hxx \
	src/output/OutputPlugin.cxx src/output/OutputPlugin.hxx \
	src/output/Finish.cxx \
	src/output/Init.cxx

liboutput_plugins_a_SOURCES = \
	src/output/Timer.cxx src/output/Timer.hxx \
	src/output/plugins/NullOutputPlugin.cxx \
	src/output/plugins/NullOutputPlugin.hxx

MIXER_LIBS = \
	libmixer_plugins.a \
	$(ALSA_LIBS) \
	$(PULSE_LIBS2)

MIXER_API_SRC = \
	src/mixer/Listener.hxx \
	src/mixer/MixerPlugin.hxx \
	src/mixer/MixerList.hxx \
	src/mixer/MixerControl.cxx src/mixer/MixerControl.hxx \
	src/mixer/MixerType.cxx src/mixer/MixerType.hxx \
	src/mixer/MixerAll.cxx \
	src/mixer/MixerInternal.hxx

libmixer_plugins_a_SOURCES = \
	src/mixer/plugins/NullMixerPlugin.cxx \
	src/mixer/plugins/SoftwareMixerPlugin.cxx \
	src/mixer/plugins/SoftwareMixerPlugin.hxx

libmixer_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ALSA_CFLAGS) \
	$(PULSE_CFLAGS)

if ENABLE_ALSA
liboutput_plugins_a_SOURCES += \
	$(ALSA_SOURCES) \
	src/output/plugins/AlsaOutputPlugin.cxx \
	src/output/plugins/AlsaOutputPlugin.hxx
libmixer_plugins_a_SOURCES += \
	$(ALSA_SOURCES) \
	src/mixer/plugins/volume_mapping.h \
	src/mixer/plugins/volume_mapping.c \
	src/mixer/plugins/AlsaMixerPlugin.cxx
endif

if ANDROID
liboutput_plugins_a_SOURCES += \
	src/output/plugins/sles/Object.hxx \
	src/output/plugins/sles/Engine.hxx \
	src/output/plugins/sles/Play.hxx \
	src/output/plugins/sles/AndroidSimpleBufferQueue.hxx \
	src/output/plugins/sles/SlesOutputPlugin.cxx \
	src/output/plugins/sles/SlesOutputPlugin.hxx
OUTPUT_LIBS += -lOpenSLES
endif

if ENABLE_ROAR
liboutput_plugins_a_SOURCES += \
	src/output/plugins/RoarOutputPlugin.cxx \
	src/output/plugins/RoarOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/plugins/RoarMixerPlugin.cxx
endif

if ENABLE_AO
liboutput_plugins_a_SOURCES += \
	src/output/plugins/AoOutputPlugin.cxx \
	src/output/plugins/AoOutputPlugin.hxx
endif

if HAVE_FIFO
liboutput_plugins_a_SOURCES += \
	src/output/plugins/FifoOutputPlugin.cxx \
	src/output/plugins/FifoOutputPlugin.hxx
endif

if ENABLE_SNDIO
liboutput_plugins_a_SOURCES += \
	src/output/plugins/SndioOutputPlugin.cxx \
	src/output/plugins/SndioOutputPlugin.hxx
endif

if ENABLE_HAIKU
liboutput_plugins_a_SOURCES += \
	src/output/plugins/HaikuOutputPlugin.cxx \
	src/output/plugins/HaikuOutputPlugin.hxx
libmixer_plugins_a_SOURCES += \
	src/mixer/plugins/HaikuMixerPlugin.cxx
endif

if ENABLE_PIPE_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/plugins/PipeOutputPlugin.cxx \
	src/output/plugins/PipeOutputPlugin.hxx
endif

if ENABLE_JACK
liboutput_plugins_a_SOURCES += \
	src/output/plugins/JackOutputPlugin.cxx \
	src/output/plugins/JackOutputPlugin.hxx
endif

if HAVE_OSS
liboutput_plugins_a_SOURCES += \
	src/output/plugins/OssOutputPlugin.cxx \
	src/output/plugins/OssOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/plugins/OssMixerPlugin.cxx
endif

if HAVE_OPENAL
liboutput_plugins_a_SOURCES += \
	src/output/plugins/OpenALOutputPlugin.cxx \
	src/output/plugins/OpenALOutputPlugin.hxx
endif

if HAVE_OSX
liboutput_plugins_a_SOURCES += \
	src/output/plugins/OSXOutputPlugin.cxx \
	src/output/plugins/OSXOutputPlugin.hxx
endif

if ENABLE_PULSE
liboutput_plugins_a_SOURCES += \
	src/output/plugins/PulseOutputPlugin.cxx \
	src/output/plugins/PulseOutputPlugin.hxx
libmixer_plugins_a_SOURCES += \
	src/mixer/plugins/PulseMixerPlugin.cxx src/mixer/plugins/PulseMixerPlugin.hxx

noinst_LIBRARIES += libpulse.a
libpulse_a_SOURCES = \
	src/lib/pulse/LockGuard.hxx \
	src/lib/pulse/LogError.cxx src/lib/pulse/LogError.hxx \
	src/lib/pulse/Error.cxx src/lib/pulse/Error.hxx \
	src/lib/pulse/Domain.cxx src/lib/pulse/Domain.hxx
libpulse_a_CPPFLAGS = $(AM_CPPFLAGS) $(PULSE_CFLAGS)
PULSE_LIBS2 = libpulse.a $(PULSE_LIBS)
OUTPUT_LIBS += $(PULSE_LIBS2)
endif

if HAVE_SHOUT
liboutput_plugins_a_SOURCES += \
	src/output/plugins/ShoutOutputPlugin.cxx \
	src/output/plugins/ShoutOutputPlugin.hxx
endif

if ENABLE_RECORDER_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/plugins/RecorderOutputPlugin.cxx \
	src/output/plugins/RecorderOutputPlugin.hxx
endif

if ENABLE_HTTPD_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/plugins/httpd/IcyMetaDataServer.cxx \
	src/output/plugins/httpd/IcyMetaDataServer.hxx \
	src/output/plugins/httpd/Page.cxx src/output/plugins/httpd/Page.hxx \
	src/output/plugins/httpd/HttpdInternal.hxx \
	src/output/plugins/httpd/HttpdClient.cxx \
	src/output/plugins/httpd/HttpdClient.hxx \
	src/output/plugins/httpd/HttpdOutputPlugin.cxx \
	src/output/plugins/httpd/HttpdOutputPlugin.hxx
endif

if ENABLE_SOLARIS_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/plugins/SolarisOutputPlugin.cxx src/output/plugins/SolarisOutputPlugin.hxx
endif

if ENABLE_WINMM_OUTPUT
liboutput_plugins_a_SOURCES += \
	src/output/plugins/WinmmOutputPlugin.cxx \
	src/output/plugins/WinmmOutputPlugin.hxx
libmixer_plugins_a_SOURCES += src/mixer/plugins/WinmmMixerPlugin.cxx
endif


#
# Playlist plugins
#

libplaylist_plugins_a_SOURCES = \
	src/playlist/PlaylistPlugin.hxx \
	src/playlist/SongEnumerator.hxx \
	src/playlist/MemorySongEnumerator.cxx \
	src/playlist/MemorySongEnumerator.hxx \
	src/playlist/plugins/ExtM3uPlaylistPlugin.cxx \
	src/playlist/plugins/ExtM3uPlaylistPlugin.hxx \
	src/playlist/plugins/M3uPlaylistPlugin.cxx \
	src/playlist/plugins/M3uPlaylistPlugin.hxx \
	src/playlist/plugins/PlsPlaylistPlugin.cxx \
	src/playlist/plugins/PlsPlaylistPlugin.hxx \
	src/playlist/PlaylistRegistry.cxx src/playlist/PlaylistRegistry.hxx
libplaylist_plugins_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(EXPAT_CFLAGS) \
	$(YAJL_CFLAGS) \
	$(patsubst -I%/FLAC,-I%,$(FLAC_CFLAGS))

PLAYLIST_LIBS = \
	libplaylist_plugins.a \
	$(EXPAT_LIBS) \
	$(FLAC_LIBS)

if ENABLE_FLAC
libplaylist_plugins_a_SOURCES += \
	src/playlist/plugins/FlacPlaylistPlugin.cxx \
	src/playlist/plugins/FlacPlaylistPlugin.hxx
endif

if ENABLE_CUE
libplaylist_plugins_a_SOURCES += \
	src/playlist/cue/CueParser.cxx src/playlist/cue/CueParser.hxx \
	src/playlist/plugins/CuePlaylistPlugin.cxx \
	src/playlist/plugins/CuePlaylistPlugin.hxx \
	src/playlist/plugins/EmbeddedCuePlaylistPlugin.cxx \
	src/playlist/plugins/EmbeddedCuePlaylistPlugin.hxx
endif

if ENABLE_SOUNDCLOUD
libplaylist_plugins_a_SOURCES += \
	src/playlist/plugins/SoundCloudPlaylistPlugin.cxx \
	src/playlist/plugins/SoundCloudPlaylistPlugin.hxx
PLAYLIST_LIBS += $(YAJL_LIBS)
endif

if ENABLE_EXPAT
libplaylist_plugins_a_SOURCES += \
	src/lib/expat/StreamExpatParser.cxx \
	src/lib/expat/ExpatParser.cxx src/lib/expat/ExpatParser.hxx \
	src/playlist/plugins/XspfPlaylistPlugin.cxx \
	src/playlist/plugins/XspfPlaylistPlugin.hxx \
	src/playlist/plugins/AsxPlaylistPlugin.cxx \
	src/playlist/plugins/AsxPlaylistPlugin.hxx \
	src/playlist/plugins/RssPlaylistPlugin.cxx \
	src/playlist/plugins/RssPlaylistPlugin.hxx
endif

#
# Filter plugins
#

libfilter_plugins_a_SOURCES = \
	src/AudioCompress/config.h \
	src/AudioCompress/compress.h \
	src/AudioCompress/compress.c \
	src/filter/plugins/NullFilterPlugin.cxx \
	src/filter/plugins/ChainFilterPlugin.cxx \
	src/filter/plugins/ChainFilterPlugin.hxx \
	src/filter/plugins/AutoConvertFilterPlugin.cxx \
	src/filter/plugins/AutoConvertFilterPlugin.hxx \
	src/filter/plugins/ConvertFilterPlugin.cxx \
	src/filter/plugins/ConvertFilterPlugin.hxx \
	src/filter/plugins/RouteFilterPlugin.cxx \
	src/filter/plugins/NormalizeFilterPlugin.cxx \
	src/filter/plugins/NormalizeFilterPlugin.hxx \
	src/filter/plugins/ReplayGainFilterPlugin.cxx \
	src/filter/plugins/ReplayGainFilterPlugin.hxx \
	src/filter/plugins/VolumeFilterPlugin.cxx \
	src/filter/plugins/VolumeFilterPlugin.hxx

FILTER_LIBS = \
	libfilter_plugins.a \
	$(PCM_LIBS)


#
# systemd unit
#

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	systemd/system/mpd.socket \
	systemd/system/mpd.service
endif

if HAVE_SYSTEMD_USER
systemduserunit_DATA = \
	systemd/user/mpd.service
endif


#
# Test programs
#

if ENABLE_TEST

C_TESTS = \
	test/test_util \
	test/test_byte_reverse \
	test/test_rewind \
	test/test_mixramp \
	test/test_pcm \
	test/test_protocol \
	test/test_queue_priority \
	test/TestFs \
	test/TestIcu

if ENABLE_CURL
C_TESTS += test/test_icy_parser
endif

if ENABLE_DATABASE
C_TESTS += test/test_translate_song
endif

if ENABLE_ARCHIVE
C_TESTS += test/test_archive
endif

TESTS = $(C_TESTS)

noinst_PROGRAMS = \
	$(C_TESTS) \
	test/read_conf \
	test/run_resolver \
	test/run_input \
	test/WriteFile \
	test/dump_text_file \
	test/dump_playlist \
	test/run_decoder \
	test/read_tags \
	test/ReadApeTags \
	test/ContainerScan \
	test/run_filter \
	test/run_output \
	test/run_convert \
	test/run_normalize \
	test/software_volume

if ENABLE_DATABASE
noinst_PROGRAMS += test/DumpDatabase
noinst_PROGRAMS += test/run_storage
endif

if ENABLE_NEIGHBOR_PLUGINS
noinst_PROGRAMS += test/run_neighbor_explorer
endif

if HAVE_AVAHI
noinst_PROGRAMS += test/run_avahi
endif

if ENABLE_ARCHIVE
noinst_PROGRAMS += test/visit_archive
endif

if ENABLE_ID3TAG
noinst_PROGRAMS += test/dump_rva2
endif

if ENABLE_ALSA
# this debug program is still ALSA specific
noinst_PROGRAMS += test/read_mixer
endif

test_read_conf_LDADD = \
	libconf.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_read_conf_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/read_conf.cxx

test_run_resolver_LDADD = \
	libnet.a \
	libutil.a
test_run_resolver_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/run_resolver.cxx

if ENABLE_DATABASE

test_DumpDatabase_LDADD = \
	$(DB_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	$(FS_LIBS) \
	libsystem.a \
	$(ICU_LDADD) \
	libutil.a
test_DumpDatabase_SOURCES = test/DumpDatabase.cxx \
	src/protocol/Ack.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/db/Registry.cxx \
	src/db/Selection.cxx \
	src/db/PlaylistVector.cxx \
	src/db/DatabaseLock.cxx \
	src/SongSave.cxx \
	src/DetachedSong.cxx \
	src/TagSave.cxx \
	src/SongFilter.cxx

if ENABLE_UPNP
test_DumpDatabase_SOURCES += src/lib/expat/ExpatParser.cxx
endif

test_run_storage_LDADD = \
	$(STORAGE_LIBS) \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libevent.a \
	libthread.a \
	libsystem.a \
	libutil.a
test_run_storage_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/run_storage.cxx

if ENABLE_WEBDAV
test_run_storage_SOURCES += $(CURL_SOURCES)
endif

endif

test_run_input_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_run_input_SOURCES = test/run_input.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/TagSave.cxx

if ENABLE_NEIGHBOR_PLUGINS

test_run_neighbor_explorer_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/run_neighbor_explorer.cxx
test_run_neighbor_explorer_LDADD = $(AM_LDADD) \
	$(NEIGHBOR_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	libtag.a \
	libconf.a \
	libevent.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libthread.a \
	libutil.a

if ENABLE_UPNP
test_run_neighbor_explorer_SOURCES += src/lib/expat/ExpatParser.cxx
endif

endif

if ENABLE_ARCHIVE

test_visit_archive_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_visit_archive_SOURCES = test/visit_archive.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/input/Open.cxx

endif

if ENABLE_ZLIB

noinst_PROGRAMS += test/run_gzip test/run_gunzip

test_run_gzip_LDADD = \
	libutil.a \
	$(FS_LIBS)
test_run_gzip_SOURCES = test/run_gzip.cxx \
	src/Log.cxx src/LogBackend.cxx

test_run_gunzip_SOURCES = test/run_gunzip.cxx \
	src/Log.cxx src/LogBackend.cxx
test_run_gunzip_LDADD = \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a

endif

test_WriteFile_LDADD = \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_WriteFile_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/WriteFile.cxx

test_dump_text_file_LDADD = \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libevent.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libthread.a \
	libutil.a
test_dump_text_file_SOURCES = test/dump_text_file.cxx \
	src/Log.cxx src/LogBackend.cxx

test_dump_playlist_LDADD = \
	$(PLAYLIST_LIBS) \
	$(FLAC_LIBS) \
	$(DECODER_LIBS) \
	$(TAG_LIBS) \
	$(XIPH_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a \
	libpcm.a
test_dump_playlist_SOURCES = test/dump_playlist.cxx \
	test/FakeDecoderAPI.cxx test/FakeDecoderAPI.hxx \
	src/Log.cxx src/LogBackend.cxx \
	src/TagSave.cxx \
	src/TagFile.cxx \
	src/DetachedSong.cxx

if ENABLE_FLAC
test_dump_playlist_SOURCES += \
	src/ReplayGainInfo.cxx \
	src/decoder/plugins/FlacMetadata.cxx
endif

test_run_decoder_LDADD = \
	$(DECODER_LIBS) \
	libpcm.a \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_run_decoder_SOURCES = test/run_decoder.cxx \
	test/FakeDecoderAPI.cxx test/FakeDecoderAPI.hxx \
	src/DetachedSong.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/ReplayGainInfo.cxx

test_read_tags_LDADD = \
	$(DECODER_LIBS) \
	libpcm.a \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_read_tags_SOURCES = test/read_tags.cxx \
	test/FakeDecoderAPI.cxx test/FakeDecoderAPI.hxx \
	src/DetachedSong.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/ReplayGainInfo.cxx

test_ReadApeTags_LDADD = \
	$(TAG_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_ReadApeTags_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/ReadApeTags.cxx

test_ContainerScan_LDADD = \
	$(DECODER_LIBS) \
	libpcm.a \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_ContainerScan_SOURCES = test/ContainerScan.cxx \
	src/DetachedSong.cxx \
	src/SongSave.cxx src/TagSave.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/ReplayGainInfo.cxx \
	$(DECODER_SRC)

if ENABLE_ID3TAG
test_dump_rva2_LDADD = \
	$(TAG_LIBS) \
	$(INPUT_LIBS) \
	$(ARCHIVE_LIBS) \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_dump_rva2_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/dump_rva2.cxx
endif

test_run_filter_LDADD = \
	$(FILTER_LIBS) \
	libconf.a \
	libbasic.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_run_filter_SOURCES = test/run_filter.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/filter/FilterPlugin.cxx src/filter/FilterRegistry.cxx

if ENABLE_ENCODER
noinst_PROGRAMS += test/run_encoder
test_run_encoder_SOURCES = test/run_encoder.cxx \
	src/Log.cxx src/LogBackend.cxx
test_run_encoder_LDADD = \
	$(ENCODER_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libbasic.a \
	libpcm.a \
	libthread.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
endif

if ENABLE_VORBISENC
noinst_PROGRAMS += test/test_vorbis_encoder
test_test_vorbis_encoder_SOURCES = test/test_vorbis_encoder.cxx \
	src/Log.cxx src/LogBackend.cxx
test_test_vorbis_encoder_CPPFLAGS = $(AM_CPPFLAGS) \
	$(ENCODER_CFLAGS)
test_test_vorbis_encoder_LDADD = $(MPD_LIBS) \
	$(ENCODER_LIBS) \
	$(PCM_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libbasic.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
endif

test_software_volume_SOURCES = test/software_volume.cxx \
	src/Log.cxx src/LogBackend.cxx
test_software_volume_LDADD = \
	$(PCM_LIBS) \
	libbasic.a \
	libutil.a

test_run_avahi_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	src/zeroconf/ZeroconfAvahi.cxx src/zeroconf/AvahiPoll.cxx \
	test/ShutdownHandler.cxx test/ShutdownHandler.hxx \
	test/run_avahi.cxx
test_run_avahi_CPPFLAGS = $(AM_CPPFLAGS) \
	$(AVAHI_CFLAGS)
test_run_avahi_LDADD = \
	libevent.a \
	libsystem.a \
	libutil.a \
	$(AVAHI_LIBS)

test_run_normalize_SOURCES = test/run_normalize.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/CheckAudioFormat.cxx \
	src/AudioCompress/compress.c \
	src/AudioParser.cxx
test_run_normalize_LDADD = \
	libutil.a

test_run_convert_SOURCES = test/run_convert.cxx \
	src/Log.cxx src/LogBackend.cxx
test_run_convert_LDADD = \
	$(PCM_LIBS) \
	libconf.a \
	libbasic.a \
	$(FS_LIBS) \
	libsystem.a \
	$(ICU_LDADD) \
	libutil.a

test_run_output_LDADD = $(MPD_LIBS) \
	$(OUTPUT_LIBS) \
	$(ENCODER_LIBS) \
	libmixer_plugins.a \
	$(PCM_LIBS) \
	$(TAG_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libnet.a \
	libsystem.a \
	libthread.a \
	libutil.a
test_run_output_SOURCES = test/run_output.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/output/Domain.cxx \
	src/output/Registry.cxx \
	src/output/OutputPlugin.cxx \
	src/mixer/MixerControl.cxx \
	src/mixer/MixerType.cxx

test_read_mixer_LDADD = \
	libpcm.a \
	libmixer_plugins.a \
	$(OUTPUT_LIBS) \
	libconf.a \
	libbasic.a \
	libevent.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a
test_read_mixer_SOURCES = test/read_mixer.cxx \
	src/Log.cxx src/LogBackend.cxx \
	src/mixer/MixerControl.cxx \
	src/filter/FilterPlugin.cxx \
	src/filter/plugins/VolumeFilterPlugin.cxx

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
	src/Log.cxx src/LogBackend.cxx \
	src/db/update/InotifyDomain.cxx \
	src/db/update/InotifySource.cxx
test_run_inotify_LDADD = \
	libevent.a \
	libsystem.a \
	libutil.a
endif

test_test_util_SOURCES = \
	test/DivideStringTest.hxx \
	test/SplitStringTest.hxx \
	test/UriUtilTest.hxx \
	test/MimeTypeTest.hxx \
	test/TestCircularBuffer.hxx \
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

test_test_rewind_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/test_rewind.cxx
test_test_rewind_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_rewind_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_rewind_LDADD = \
	$(INPUT_LIBS) \
	libthread.a \
	libtag.a \
	libutil.a \
	$(CPPUNIT_LIBS)

test_test_mixramp_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/test_mixramp.cxx
test_test_mixramp_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_mixramp_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_mixramp_LDADD = \
	libutil.a \
	$(CPPUNIT_LIBS)

if ENABLE_CURL
test_test_icy_parser_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/test_icy_parser.cxx
test_test_icy_parser_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_icy_parser_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_icy_parser_LDADD = \
	libtag.a \
	libutil.a \
	$(CPPUNIT_LIBS)
endif

test_test_pcm_SOURCES = \
	test/TestAudioFormat.cxx test/TestAudioFormat.hxx \
	test/test_pcm_util.hxx \
	test/test_pcm_dither.cxx \
	test/test_pcm_pack.cxx \
	test/test_pcm_channels.cxx \
	test/test_pcm_format.cxx \
	test/test_pcm_volume.cxx \
	test/test_pcm_mix.cxx \
	test/test_pcm_interleave.cxx \
	test/test_pcm_export.cxx \
	test/test_pcm_all.hxx \
	test/test_pcm_main.cxx
test_test_pcm_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_pcm_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_pcm_LDADD = \
	$(PCM_LIBS) \
	libbasic.a \
	libutil.a \
	$(CPPUNIT_LIBS)

test_test_archive_SOURCES = \
	src/Log.cxx src/LogBackend.cxx \
	test/test_archive.cxx
test_test_archive_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_archive_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_archive_LDADD = \
	libarchive.a \
	libutil.a \
	$(CPPUNIT_LIBS)

if ENABLE_DATABASE

test_test_translate_song_SOURCES = \
	src/playlist/PlaylistSong.cxx \
	src/PlaylistError.cxx \
	src/DetachedSong.cxx \
	src/SongLoader.cxx \
	src/LocateUri.cxx \
	src/Log.cxx \
	test/test_translate_song.cxx
test_test_translate_song_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_translate_song_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_translate_song_LDADD = \
	$(STORAGE_LIBS) \
	libtag.a \
	$(FS_LIBS) \
	$(ICU_LDADD) \
	libsystem.a \
	libutil.a \
	$(CPPUNIT_LIBS)

endif

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
	src/queue/Queue.cxx \
	src/DetachedSong.cxx \
	test/test_queue_priority.cxx
test_test_queue_priority_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_test_queue_priority_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_test_queue_priority_LDADD = \
	libsystem.a \
	libutil.a \
	$(CPPUNIT_LIBS)

test_TestFs_SOURCES = \
	test/TestFs.cxx
test_TestFs_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_TestFs_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_TestFs_LDADD = \
	$(FS_LIBS) \
	$(CPPUNIT_LIBS)

test_TestIcu_SOURCES = \
	test/TestIcu.cxx
test_TestIcu_CPPFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -DCPPUNIT_HAVE_RTTI=0
test_TestIcu_CXXFLAGS = $(AM_CXXFLAGS) -Wno-error=deprecated-declarations
test_TestIcu_LDADD = \
	$(ICU_LDADD) \
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
doc_DATA = AUTHORS COPYING NEWS README.md doc/mpdconf.example

DOCBOOK_FILES = doc/protocol.xml doc/user.xml doc/developer.xml

if ENABLE_DOCUMENTATION
protocoldir = $(docdir)/protocol
protocol_DATA = $(wildcard doc/protocol/*.html)

userdir = $(docdir)/user
user_DATA = $(wildcard doc/user/*.html)

developerdir = $(docdir)/developer
developer_DATA = $(wildcard doc/developer/*.html)

DOCBOOK_HTML = $(patsubst %.xml,%/index.html,$(DOCBOOK_FILES))

DOCBOOK_INCLUDES = $(wildcard $(srcdir)/doc/include/*.xml)

$(DOCBOOK_HTML): %/index.html: %.xml $(DOCBOOK_INCLUDES)
	$(XMLTO) -o $(@D) --stringparam chunker.output.encoding=utf-8 html --stringparam use.id.as.filename=1 $<

doc/api/html/index.html: doc/doxygen.conf
	@$(MKDIR_P) $(@D)
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
	rsync -vpruz --delete doc/ www.musicpd.org:/var/www/mpd/doc/ \
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
	$(wildcard $(srcdir)/scripts/*.rb) \
	$(man_MANS) $(DOCBOOK_FILES) doc/mpdconf.example doc/doxygen.conf \
	$(wildcard $(srcdir)/doc/include/*.xml) \
	systemd/system/mpd.socket \
	android/AndroidManifest.xml \
	android/build.py \
	android/custom_rules.xml \
	android/res/values/strings.xml \
	android/src/Bridge.java \
	android/src/Loader.java \
	android/src/Main.java \
	win32/res/mpd.rc.in win32/res/mpd.ico \
	src/haiku/App_MusicPD
