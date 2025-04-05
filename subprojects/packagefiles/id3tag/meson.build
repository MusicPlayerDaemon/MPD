project(
  'libid3tag', 'c',
  version: '0.15.1b',
  license: 'GPLv2+',
)

compiler = meson.get_compiler('c')

conf = configuration_data()
conf.set('HAVE_ASSERT_H', true)
conf.set('HAVE_FTRUNCATE', compiler.has_function('ftruncate'))
conf.set('HAVE_INTTYPES_H', true)
conf.set('HAVE_LIBZ', false) # TODO
conf.set('HAVE_STDINT_H', true)
conf.set('HAVE_STDIO_H', true)
conf.set('HAVE_STDLIB_H', true)
conf.set('HAVE_STRINGS_H', true)
conf.set('HAVE_STRING_H', true)
conf.set('HAVE_SYS_STAT_H', compiler.has_header('sys/stat.h'))
conf.set('HAVE_SYS_TYPES_H', true)
conf.set('HAVE_UNISTD_H', compiler.has_header('unistd.h'))
configure_file(output: 'config.h', configuration: conf)

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

libid3tag = static_library(
  'id3tag',
  'version.c', 'ucs4.c', 'latin1.c', 'utf16.c', 'utf8.c',
  'parse.c', 'render.c', 'field.c', 'frametype.c', 'compat.c',
  'genre.c', 'frame.c', 'crc.c', 'util.c', 'tag.c', 'file.c',
)

libid3tag_dep = declare_dependency(
  link_with: libid3tag,
  include_directories: include_directories('.'),
)
