project(
  'libid3tag', 'c',
  version: '0.16.4',
  license: 'GPLv2+',
)

compiler = meson.get_compiler('c')

gperf = find_program('gperf')

id3tag_c_args = ['-DHAVE_ASSERT_H']
if compiler.has_header('sys/stat.h')
  id3tag_c_args += '-DHAVE_SYS_STAT_H'
endif
if compiler.has_header('unistd.h')
  id3tag_c_args += '-DHAVE_UNISTD_H'
endif
if compiler.has_function('ftruncate')
  id3tag_c_args += '-DHAVE_FTRUNCATE'
endif
if compiler.has_function('fseeko') # 64-bit fseek
  id3tag_c_args += '-DHAVE_FSEEKO'
endif
if compiler.has_function('ftello') # 64-bit ftell
  id3tag_c_args += '-DHAVE_FTELLO'
endif

cmake_prog = find_program('cmake')

genre_dat = custom_target(
  'genre.dat',
  input: 'src/genre.dat.in',
  output: 'genre.dat',
  command: [
    cmake_prog, '-P',
    meson.current_source_dir() / 'genre.dat.cmake',
    '@INPUT@', '@OUTPUT@',
  ],
)

gperf_common_args = [
  '-L', 'ANSI-C', '-tCcTonD', '-K', 'id',
  '-s', '-3', '-k', '*',
  '--output-file=@OUTPUT@', '@INPUT@',
]

compat_c = custom_target(
  'compat.c',
  input: 'src/compat.gperf',
  output: 'compat.c',
  command: [gperf, gperf_common_args, '-N', 'id3_compat_lookup'],
)

frametype_c = custom_target(
  'frametype.c',
  input: 'src/frametype.gperf',
  output: 'frametype.c',
  command: [gperf, gperf_common_args, '-N', 'id3_frametype_lookup'],
)

id3tag_version_conf = configuration_data()
id3tag_version_conf.set('PROJECT_VERSION_MAJOR', '0')
id3tag_version_conf.set('PROJECT_VERSION_MINOR', '16')
id3tag_version_conf.set('PROJECT_VERSION_PATCH', '4')

id3tag_h = configure_file(
  input: 'include/id3tag.h.in',
  output: 'id3tag.h',
  configuration: id3tag_version_conf,
)

libid3tag = static_library(
  'id3tag',
  [
    id3tag_h, compat_c, frametype_c, genre_dat,
    'src/crc.c', 'src/debug.c', 'src/field.c', 'src/file.c',
    'src/frame.c', 'src/genre.c', 'src/latin1.c', 'src/parse.c',
    'src/render.c', 'src/tag.c', 'src/ucs4.c', 'src/utf16.c',
    'src/utf8.c', 'src/util.c', 'src/version.c',
  ],
  include_directories: include_directories('src'),
  c_args: id3tag_c_args,
)

libid3tag_dep = declare_dependency(
  link_with: libid3tag,
  dependencies: [
    dependency('zlib'),
  ],
  include_directories: [
    include_directories('src'),
    include_directories('.'),
  ],
)
