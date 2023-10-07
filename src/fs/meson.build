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
    shlwapi_dep,
    icu_dep,
    fmt_dep,
  ],
)

fs_dep = declare_dependency(
  link_with: fs,
  dependencies: [
  ],
)
