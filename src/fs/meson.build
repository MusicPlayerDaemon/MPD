fs_sources = [
  'Domain.cxx',
  'Traits.cxx',
  'Config.cxx',
  'Charset.cxx',
  'Path.cxx',
  'Path2.cxx',
  'AllocatedPath.cxx',
  'FileSystem.cxx',
  'List.cxx',
  'StandardDirectory.cxx',
  'CheckFile.cxx',
  'LookupFile.cxx',
  'DirectoryReader.cxx',
  'io/PeekReader.cxx',
  'io/FileReader.cxx',
  'io/BufferedReader.cxx',
  'io/TextFile.cxx',
  'io/FileOutputStream.cxx',
  'io/BufferedOutputStream.cxx',
]

if is_windows
  shlwapi_dep = c_compiler.find_library('shlwapi')
else
  shlwapi_dep = dependency('', required: false)
endif

if zlib_dep.found()
  fs_sources += [
    'io/GunzipReader.cxx',
    'io/AutoGunzipReader.cxx',
    'io/GzipOutputStream.cxx',
  ]
endif

fs = static_library(
  'fs',
  fs_sources,
  include_directories: inc,
  dependencies: [
    zlib_dep,
  ],
)

fs_dep = declare_dependency(
  link_with: fs,
  dependencies: [
    system_dep,
    icu_dep,
    shlwapi_dep,
  ],
)
