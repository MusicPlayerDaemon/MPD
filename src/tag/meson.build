generate_parse_name = executable(
  'GenParseName',
  'GenParseName.cxx',
  'Names.c',
  native: true,
)

parse_name_cxx = custom_target(
  'RunGenParseName',
  output: ['ParseName2.cxx'],
  command : [
    generate_parse_name, '@OUTPUT@',
  ],
)

tag_sources = [
  'Tag.cxx',
  'Builder.cxx',
  'Handler.cxx',
  'Settings.cxx',
  'Sort.cxx',
  'Config.cxx',
  'ParseName.cxx',
  parse_name_cxx,
  'Names.c',
  'FixString.cxx',
  'Pool.cxx',
  'Table.cxx',
  'Format.cxx',
  'VorbisComment.cxx',
  'ReplayGainInfo.cxx',
  'ReplayGainParser.cxx',
  'MixRampParser.cxx',
  'Generic.cxx',
  'Id3MusicBrainz.cxx',
  'Id3Picture.cxx',
  'ApeLoader.cxx',
  'ApeReplayGain.cxx',
  'ApeTag.cxx',
]

libid3tag_dep = dependency('id3tag', required: get_option('id3tag'))
conf.set('ENABLE_ID3TAG', libid3tag_dep.found())
if libid3tag_dep.found()
  tag_sources += [
    'Id3Load.cxx',
    'Id3Scan.cxx',
    'Id3MixRamp.cxx',
    'Id3ReplayGain.cxx',
    'Rva2.cxx',
    'RiffId3.cxx',
    'Aiff.cxx',
  ]
endif

tag = static_library(
  'tag',
  tag_sources,
  include_directories: inc,
  dependencies: [
    libid3tag_dep,
  ],
)

tag_dep = declare_dependency(
  link_with: tag,
  dependencies: [
    time_dep,
    util_dep,
  ],
)
