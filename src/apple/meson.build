if not is_darwin
  apple_dep = dependency('', required: false)
  subdir_done()
endif

audiounit_dep = declare_dependency(
  link_args: ['-framework', 'AudioUnit', '-framework', 'CoreAudio', '-framework', 'CoreServices'],
  dependencies: [
    boost_dep,
  ],
)

apple = static_library(
  'apple',
  'AudioObject.cxx',
  'Throw.cxx',
  include_directories: inc,
  dependencies: [
    audiounit_dep,
  ],
)

apple_dep = declare_dependency(
  link_with: apple,
  dependencies: [
    audiounit_dep,
  ],
)
