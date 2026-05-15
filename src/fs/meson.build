fs_path = static_library(
  'fs_path',
  'Traits.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
    fmt_dep,
  ],
)

fs_path_dep = declare_dependency(
  link_with: fs_path,
  dependencies: [
    util_dep,
  ],
)

fs_sources = [
  'Config.cxx',
  'Charset.cxx',
  'Glob.cxx',
  'Path.cxx',
  'Path2.cxx',
  'AllocatedPath.cxx',
  'NarrowPath.cxx',
  'FileSystem.cxx',
  'List.cxx',
  'LookupFile.cxx',
  'DirectoryReader.cxx',
  'FileInfo.cxx',
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
    fs_path_dep,
    io_dep,
    shlwapi_dep,
    icu_dep,
    fmt_dep,
  ],
)

fs_dep = declare_dependency(
  link_with: fs,
  dependencies: [
    fs_path_dep,
    io_dep,
  ],
)
