if not alsa_dep.found()
  subdir_done()
endif

test(
  'TestAlsa',
  executable(
    'TestAlsa',
    'TestPcmCard.cxx',
    include_directories: inc,
    dependencies: [
      alsa_dep,
      libasound_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
