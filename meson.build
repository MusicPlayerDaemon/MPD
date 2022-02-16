project(
  'mpd',
  ['c', 'cpp'],
  version: '0.24',
  meson_version: '>= 0.56.0',
  default_options: [
    'c_std=c11',
    'build.c_std=c11',
    'cpp_std=c++17',
    'build.cpp_std=c++17',
    'warning_level=3',

    # If we build those libraries as Meson subproject, they shall be
    # linked statically into the MPD executable.
    'expat:default_library=static',
    'fmt:default_library=static',
    'gtest:default_library=static',
    'sqlite3:default_library=static',
    'vorbis:default_library=static',

    # Not interested in compiler warnings from subprojects.
    'expat:werror=false',
    'expat:warning_level=0',
    'fmt:warning_level=0',
    'gtest:warning_level=0',
    'sqlite3:warning_level=0',
    'vorbis:warning_level=0',
  ],
  license: 'GPLv2+',
)

version_cxx = vcs_tag(input: 'src/GitVersion.cxx', output: 'GitVersion.cxx')

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

if compiler.get_id() == 'gcc' and compiler.version().version_compare('<8')
  warning('Your GCC version is too old.  You need at least version 8.')
elif compiler.get_id() == 'clang' and compiler.version().version_compare('<7')
  warning('Your clang version is too old.  You need at least version 7.')
endif

version_conf = configuration_data()
version_conf.set_quoted('PACKAGE', meson.project_name())
version_conf.set_quoted('PACKAGE_NAME', meson.project_name())
version_conf.set_quoted('VERSION', meson.project_version())
version_conf.set_quoted('PROTOCOL_VERSION', '0.24.0')
configure_file(output: 'Version.h', configuration: version_conf)

conf = configuration_data()
conf.set_quoted('SYSTEM_CONFIG_FILE_LOCATION', join_paths(get_option('prefix'), get_option('sysconfdir'), 'mpd.conf'))

common_cppflags = [
  '-D_GNU_SOURCE',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

test_common_flags = [
  '-ffast-math',
  '-ftree-vectorize',

  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wmissing-declarations',
  '-Wshadow',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_cxxflags = test_global_common_flags + [
]

test_global_cflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

if compiler.get_id() != 'gcc' or compiler.version().version_compare('>=9')
  # The GCC 8 implementation of this flag is buggy: it complains even
  # if "final" is present, which implies "override".
  test_cxxflags += '-Wsuggest-override'
endif

test_cflags = test_common_flags + [
  '-Wmissing-prototypes',
  '-Wstrict-prototypes',
]

test_ldflags = [
  # make relocations read-only (hardening)
  '-Wl,-z,relro',

  # no lazy binding, please - not worth it for a daemon
  '-Wl,-z,now',
]

if get_option('buildtype') != 'debug'
  test_global_cxxflags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]
  test_global_cflags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]
  test_ldflags += [
    '-Wl,--gc-sections',
  ]
endif

if get_option('fuzzer')
  fuzzer_flags = ['-fsanitize=fuzzer']
  if get_option('b_sanitize') == 'none'
    fuzzer_flags += ['-fsanitize=address,undefined']
  endif
  add_global_arguments(fuzzer_flags, language: 'cpp')
  add_global_arguments(fuzzer_flags, language: 'c')
  add_global_link_arguments(fuzzer_flags, language: 'cpp')
endif

add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_global_arguments(c_compiler.get_supported_arguments(test_global_cflags), language: 'c')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_project_arguments(c_compiler.get_supported_arguments(test_cflags), language: 'c')
add_project_link_arguments(compiler.get_supported_link_arguments(test_ldflags), language: 'cpp')

is_linux = host_machine.system() == 'linux'
is_android = get_option('android_ndk') != ''
is_darwin = host_machine.system() == 'darwin'
is_windows = host_machine.system() == 'windows'
is_haiku = host_machine.system() == 'haiku'

if is_android
  common_cppflags += '-DANDROID'
endif

if is_windows
  common_cppflags += [
    # enable Windows Vista APIs
    '-DWINVER=0x0600', '-D_WIN32_WINNT=0x0600',

    # enable Unicode support (TCHAR=wchar_t) in the Windows API (macro
    # "UNICODE) and the C library (macro "_UNICODE")
    '-DUNICODE', '-D_UNICODE',

    # enable strict type checking in the Windows API headers
    '-DSTRICT',

    # reduce header bloat by disabling obscure and obsolete Windows
    # APIs
    '-DWIN32_LEAN_AND_MEAN',

    # disable more Windows APIs which are not used by MPD
    '-DNOGDI', '-DNOBITMAP', '-DNOCOMM',
    '-DNOUSER',

    # reduce COM header bloat
    '-DCOM_NO_WINDOWS_H',

    # disable Internet Explorer specific APIs
    '-D_WIN32_IE=0',
  ]

  subdir('win32')
endif

if is_android
  subdir('android')
endif

add_global_arguments(common_cppflags, language: 'c')
add_global_arguments(common_cppflags, language: 'cpp')

enable_daemon = not is_windows and not is_android and get_option('daemon')
conf.set('ENABLE_DAEMON', enable_daemon)

conf.set('HAVE_GETPWNAM_R', compiler.has_function('getpwnam_r'))
conf.set('HAVE_GETPWUID_R', compiler.has_function('getpwuid_r'))
conf.set('HAVE_INITGROUPS', compiler.has_function('initgroups'))
conf.set('HAVE_FNMATCH', compiler.has_function('fnmatch'))

# Explicitly exclude Windows in this check because
# https://github.com/mesonbuild/meson/issues/3672 (reported in 2018,
# still not fixed in 2020) causes Meson to believe it exists, because
# __builtin_strndup() exists (but strndup() still cannot be used).
conf.set('HAVE_STRNDUP', not is_windows and compiler.has_function('strndup', prefix: '#define _GNU_SOURCE\n#include <string.h>'))

conf.set('HAVE_STRCASESTR', compiler.has_function('strcasestr'))

conf.set('HAVE_PRCTL', is_linux)

if not get_option('syslog').disabled()
  if compiler.has_function('syslog')
    conf.set('HAVE_SYSLOG', true)
  elif get_option('syslog').enabled()
    error('syslog() not found')
  endif
endif

enable_database = get_option('database')
conf.set('ENABLE_DATABASE', enable_database)

enable_inotify = get_option('inotify') and is_linux and enable_database
conf.set('ENABLE_INOTIFY', enable_inotify)

conf.set('ENABLE_DSD', get_option('dsd'))

inc = include_directories(
  'src',

  # for the generated config.h
  '.',
)

boost_dep = dependency('boost', version: '>= 1.58')
if boost_dep.version() == '1.67'
  # https://github.com/MusicPlayerDaemon/MPD/pull/384
  # https://github.com/boostorg/lockfree/commit/12726cda009a855073b9bedbdce57b6ce7763da2
  warning('Your Boost version 1.67 is known to be buggy, and the MPD build will fail. Please upgrade to Boost 1.68 or later.')
endif

fmt_dep = dependency('fmt', fallback: ['fmt', 'fmt_dep'])

log = static_library(
  'log',
  'src/Log.cxx',
  'src/LogBackend.cxx',
  include_directories: inc,
  dependencies: fmt_dep,
)

log_dep = declare_dependency(
  link_with: log,
  dependencies: fmt_dep,
)

sources = [
  version_cxx,
  'src/Main.cxx',
  'src/protocol/ArgParser.cxx',
  'src/command/CommandError.cxx',
  'src/command/PositionArg.cxx',
  'src/command/AllCommands.cxx',
  'src/command/QueueCommands.cxx',
  'src/command/TagCommands.cxx',
  'src/command/PlayerCommands.cxx',
  'src/command/PlaylistCommands.cxx',
  'src/command/FileCommands.cxx',
  'src/command/OutputCommands.cxx',
  'src/command/MessageCommands.cxx',
  'src/command/ClientCommands.cxx',
  'src/command/PartitionCommands.cxx',
  'src/command/OtherCommands.cxx',
  'src/command/CommandListBuilder.cxx',
  'src/config/PartitionConfig.cxx',
  'src/config/PlayerConfig.cxx',
  'src/config/ReplayGainConfig.cxx',
  'src/Idle.cxx',
  'src/IdleFlags.cxx',
  'src/decoder/Thread.cxx',
  'src/decoder/Control.cxx',
  'src/decoder/Bridge.cxx',
  'src/decoder/DecoderPrint.cxx',
  'src/client/Listener.cxx',
  'src/client/Client.cxx',
  'src/client/Config.cxx',
  'src/client/Domain.cxx',
  'src/client/Event.cxx',
  'src/client/Expire.cxx',
  'src/client/Idle.cxx',
  'src/client/List.cxx',
  'src/client/New.cxx',
  'src/client/Process.cxx',
  'src/client/Read.cxx',
  'src/client/Write.cxx',
  'src/client/Message.cxx',
  'src/client/Subscribe.cxx',
  'src/client/File.cxx',
  'src/client/Response.cxx',
  'src/client/ThreadBackgroundCommand.cxx',
  'src/Listen.cxx',
  'src/LogInit.cxx',
  'src/ls.cxx',
  'src/Instance.cxx',
  'src/MusicBuffer.cxx',
  'src/MusicPipe.cxx',
  'src/MusicChunk.cxx',
  'src/MusicChunkPtr.cxx',
  'src/Mapper.cxx',
  'src/Partition.cxx',
  'src/Permission.cxx',
  'src/player/CrossFade.cxx',
  'src/player/Thread.cxx',
  'src/player/Control.cxx',
  'src/PlaylistError.cxx',
  'src/PlaylistPrint.cxx',
  'src/PlaylistSave.cxx',
  'src/playlist/PlaylistStream.cxx',
  'src/playlist/PlaylistMapper.cxx',
  'src/playlist/PlaylistAny.cxx',
  'src/playlist/PlaylistSong.cxx',
  'src/playlist/PlaylistQueue.cxx',
  'src/playlist/Print.cxx',
  'src/db/PlaylistVector.cxx',
  'src/queue/Queue.cxx',
  'src/queue/Print.cxx',
  'src/queue/Save.cxx',
  'src/queue/Selection.cxx',
  'src/queue/Playlist.cxx',
  'src/queue/PlaylistControl.cxx',
  'src/queue/PlaylistEdit.cxx',
  'src/queue/PlaylistTag.cxx',
  'src/queue/PlaylistState.cxx',
  'src/LocateUri.cxx',
  'src/SongUpdate.cxx',
  'src/SongLoader.cxx',
  'src/SongPrint.cxx',
  'src/SongSave.cxx',
  'src/StateFile.cxx',
  'src/StateFileConfig.cxx',
  'src/Stats.cxx',
  'src/TagPrint.cxx',
  'src/TagSave.cxx',
  'src/TagFile.cxx',
  'src/TagStream.cxx',
  'src/TagAny.cxx',
  'src/TimePrint.cxx',
  'src/mixer/Volume.cxx',
  'src/PlaylistFile.cxx',
]

if is_windows
  sources += [
    'src/win32/Win32Main.cxx',
  ]
endif

if not is_android
  sources += [
    'src/CommandLine.cxx',
    'src/unix/SignalHandlers.cxx',
  ]
else
  sources += [
    'src/android/Context.cxx',
    'src/android/AudioManager.cxx',
    'src/android/Environment.cxx',
    'src/android/LogListener.cxx',
  ]
endif

if enable_daemon
  sources += 'src/unix/Daemon.cxx'
endif

if enable_database
  sources += [
    'src/queue/PlaylistUpdate.cxx',
    'src/command/StorageCommands.cxx',
    'src/command/DatabaseCommands.cxx',
  ]
endif

subdir('src/util')
subdir('src/time')
subdir('src/io')
subdir('src/io/uring')
subdir('src/system')
subdir('src/thread')
subdir('src/net')
subdir('src/event')
subdir('src/win32')

subdir('src/apple')

subdir('src/lib/dbus')
subdir('src/lib/icu')
subdir('src/lib/smbclient')
subdir('src/lib/zlib')

subdir('src/lib/alsa')
subdir('src/lib/chromaprint')
subdir('src/lib/curl')
subdir('src/lib/expat')
subdir('src/lib/ffmpeg')
subdir('src/lib/gcrypt')
subdir('src/lib/nfs')
subdir('src/lib/oss')
subdir('src/lib/pcre')
subdir('src/lib/pipewire')
subdir('src/lib/pulse')
subdir('src/lib/sndio')
subdir('src/lib/sqlite')
subdir('src/lib/systemd')
subdir('src/lib/upnp')
subdir('src/lib/yajl')

subdir('src/lib/crypto')

subdir('src/zeroconf')

subdir('src/fs')
subdir('src/config')
subdir('src/tag')
subdir('src/pcm')
subdir('src/neighbor')
subdir('src/input')
subdir('src/archive')
subdir('src/filter')
subdir('src/mixer')
subdir('src/output')
subdir('src/lib/xiph')
subdir('src/decoder')
subdir('src/encoder')
subdir('src/song')
subdir('src/playlist')

if curl_dep.found()
  sources += 'src/RemoteTagCache.cxx'
endif

if sqlite_dep.found()
  sources += [
    'src/command/StickerCommands.cxx',
    'src/sticker/Database.cxx',
    'src/sticker/Print.cxx',
    'src/sticker/SongSticker.cxx',
  ]
endif

if chromaprint_dep.found()
  sources += [
    'src/command/FingerprintCommands.cxx',
    'src/lib/chromaprint/DecoderClient.cxx',
  ]
endif

basic = static_library(
  'basic',
  'src/ReplayGainMode.cxx',
  'src/SingleMode.cxx',
  include_directories: inc,
)

basic_dep = declare_dependency(
  link_with: basic,
)

if enable_database
  subdir('src/storage')
else
  storage_glue_dep = dependency('', required: false)
endif
subdir('src/db')

if neighbor_glue_dep.found()
  sources += 'src/command/NeighborCommands.cxx'
endif

if archive_glue_dep.found()
  sources += [
    'src/TagArchive.cxx',
  ]

  if enable_database
    sources += ['src/db/update/Archive.cxx']
  endif
endif

if is_windows
  sources += windows_resources
endif

link_args = []
more_deps = []
if is_android
  subdir('src/java')
  target_type = 'shared_library'
  target_name = 'mpd'
  link_args += [
    '-Wl,--no-undefined,-shared,-Bsymbolic',
    '-llog',
    '-lz',
  ]
  more_deps += [
    declare_dependency(sources: [classes_jar]),
    java_dep,
  ]
elif is_haiku
  target_type = 'executable'
  target_name = 'mpd.nores'
  link_args += [
    '-lnetwork',
    '-lbe',
  ]
else
  target_type = 'executable'
  target_name = 'mpd'
endif

mpd = build_target(
  target_name,
  sources,
  target_type: target_type,
  include_directories: inc,
  dependencies: [
    basic_dep,
    config_dep,
    dbus_dep,
    fs_dep,
    net_dep,
    util_dep,
    event_dep,
    thread_dep,
    neighbor_glue_dep,
    input_glue_dep,
    archive_glue_dep,
    output_glue_dep,
    mixer_glue_dep,
    decoder_glue_dep,
    encoder_glue_dep,
    playlist_glue_dep,
    db_glue_dep,
    storage_glue_dep,
    song_dep,
    systemd_dep,
    sqlite_dep,
    zeroconf_dep,
    more_deps,
    chromaprint_dep,
    fmt_dep,
  ],
  link_args: link_args,
  build_by_default: not get_option('fuzzer'),
  install: not is_android and not is_haiku,
)

if is_android
  subdir('android/apk')
endif

if is_haiku
  subdir('src/haiku')
  custom_target(
    'mpd',
    output: 'mpd',
    input: [mpd, rsrc],
    command: [addres, '@OUTPUT@', '@INPUT0@', '@INPUT1@'],
    install: true,
    install_dir: get_option('bindir'),
  )
endif

configure_file(output: 'config.h', configuration: conf)

if systemd_dep.found()
  subdir('systemd')
endif

install_data(
  'mpd.svg',
  install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps'),
)

install_data(
  'AUTHORS', 'COPYING', 'NEWS', 'README.md',
  install_dir: join_paths(get_option('datadir'), 'doc', meson.project_name()),
)

subdir('doc')

if get_option('test')
  subdir('test')
endif

if get_option('fuzzer')
  subdir('test/fuzzer')
endif
