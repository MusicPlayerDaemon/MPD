if not sqlite_dep.found()
  subdir_done()
endif

test(
  'TestSqlite',
  executable(
    'TestSqlite',
    'TestGenerator.cxx',
    include_directories: inc,
    dependencies: [
      sqlite_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
