test(
  'TestMixRamp',
  executable(
    'TestMixRamp',
    'TestMixRampParser.cxx',
    'TestMixRampInterpolate.cxx',
    include_directories: inc,
    dependencies: [
      log_dep,
      tag_dep,
      util_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)

test(
  'TestTagBuilder',
  executable(
    'TestTagBuilder',
    'TestTagBuilder.cxx',
    include_directories: inc,
    dependencies: [
      tag_dep,
      fmt_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
