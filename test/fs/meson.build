test(
  'TestFs',
  executable(
    'TestFs',
    'TestGlob.cxx',
    'TestLookupFile.cxx',
    'TestPath.cxx',
    include_directories: inc,
    dependencies: [
      fs_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

test(
  'TestParsePath',
  executable(
    'TestParsePath',
    'TestParsePath.cxx',
    '../../src/config/Path.cxx',
    include_directories: inc,
    dependencies: [
      fmt_dep,
      fs_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
