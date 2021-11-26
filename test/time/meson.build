test_time_sources = [
  'TestConvert.cxx',
  'TestISO8601.cxx',
]

if is_windows
  test_time_sources += 'TestFileTime.cxx'
endif

test(
  'TestTime',
  executable(
    'TestTime',
    test_time_sources,
    include_directories: inc,
    dependencies: [
      time_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
