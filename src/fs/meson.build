fs_sources = [
  'Domain.cxx',
  'Traits.cxx',
  'Config.cxx',
  'Charset.cxx',
  'Glob.cxx',
  'Path.cxx',
  'Path2.cxx',
  'AllocatedPath.cxx',
  'NarrowPath.cxx',
  'FileSystem.cxx',
  'List.cxx',
  'StandardDirectory.cxx',
  'CheckFile.cxx',
  'LookupFile.cxx',
  'DirectoryReader.cxx',
  'io/TextFile.cxx',
]

if is_windows
  shlwapi_dep = c_compiler.find_library('shlwapi')
else
  shlwapi_dep = dependency('', required: false)
endif

fs = static_library(
  'fs',
  fs_sources,
  include_directories: inc,
  dependencies: [
    zlib_dep,
    log_dep,
  ],
)

fs_dep = declare_dependency(
  link_with: fs,
  dependencies: [
    io_dep,
    system_dep,
    icu_dep,
    shlwapi_dep,
  ],
)
