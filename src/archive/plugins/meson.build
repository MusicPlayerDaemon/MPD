archive_plugins_sources = []
found_archive_plugin = false

libiso9660_dep = dependency('libiso9660', required: get_option('iso9660'))
archive_features.set('ENABLE_ISO9660', libiso9660_dep.found())
if libiso9660_dep.found()
  archive_plugins_sources += 'Iso9660ArchivePlugin.cxx'
  found_archive_plugin = true
endif

libbz2_dep = c_compiler.find_library('bz2', required: get_option('bzip2'))
archive_features.set('ENABLE_BZ2', libbz2_dep.found())
if libbz2_dep.found()
  archive_plugins_sources += 'Bzip2ArchivePlugin.cxx'
  found_archive_plugin = true
endif

libzzip_dep = dependency('zziplib', version: '>= 0.13', required: get_option('zzip'))
archive_features.set('ENABLE_ZZIP', libzzip_dep.found())
if libzzip_dep.found()
  archive_plugins_sources += 'ZzipArchivePlugin.cxx'
  found_archive_plugin = true
endif

archive_plugins = static_library(
  'archive_plugins',
  archive_plugins_sources,
  include_directories: inc,
  dependencies: [
    libbz2_dep,
    libiso9660_dep,
    libzzip_dep,
  ],
)

archive_plugins_dep = declare_dependency(
  link_with: archive_plugins,
  dependencies: [
    archive_api_dep,
    input_glue_dep,
  ],
)
