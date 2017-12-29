filter_plugins = static_library(
  'filter_plugins',
  '../../AudioCompress/compress.c',
  'NullFilterPlugin.cxx',
  'ChainFilterPlugin.cxx',
  'AutoConvertFilterPlugin.cxx',
  'ConvertFilterPlugin.cxx',
  'RouteFilterPlugin.cxx',
  'NormalizeFilterPlugin.cxx',
  'ReplayGainFilterPlugin.cxx',
  'VolumeFilterPlugin.cxx',
  include_directories: inc,
)

filter_plugins_dep = declare_dependency(
  link_with: filter_plugins,
  dependencies: [
    filter_api_dep,
    pcm_dep,
    config_dep,
  ],
)
