test_time_sources = [
  'TestISO8601.cxx',
]

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
)
