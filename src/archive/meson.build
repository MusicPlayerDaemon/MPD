archive_api = static_library(
  'archive_api',
  'ArchiveList.cxx',
  include_directories: inc,
)

archive_api_dep = declare_dependency(
  link_with: archive_api,
)

subdir('plugins')

conf.set('ENABLE_ARCHIVE', found_archive_plugin)
if not found_archive_plugin
  archive_glue_dep = dependency('', required: false)
  subdir_done()
endif

archive_glue = static_library(
  'archive_glue',
  'ArchivePlugin.cxx',
  '../input/plugins/ArchiveInputPlugin.cxx',
  include_directories: inc,
)

archive_glue_dep = declare_dependency(
  link_with: archive_glue,
  dependencies: [
    archive_plugins_dep,
  ],
)
