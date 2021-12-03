filter_plugins_sources = []
filter_plugins_deps = [fmt_dep]

if libavfilter_dep.found()
  filter_plugins_sources += [
    'FfmpegFilter.cxx',
    'FfmpegFilterPlugin.cxx',
    'HdcdFilterPlugin.cxx',
  ]
  filter_plugins_deps += ffmpeg_dep
endif

filter_plugins = static_library(
  'filter_plugins',
  'NullFilterPlugin.cxx',
  'TwoFilters.cxx',
  'AutoConvertFilterPlugin.cxx',
  'ConvertFilterPlugin.cxx',
  'RouteFilterPlugin.cxx',
  'NormalizeFilterPlugin.cxx',
  'ReplayGainFilterPlugin.cxx',
  'VolumeFilterPlugin.cxx',
  filter_plugins_sources,
  include_directories: inc,
  dependencies: [
    filter_plugins_deps,
    log_dep,
  ],
)

filter_plugins_dep = declare_dependency(
  link_with: filter_plugins,
  dependencies: [
    filter_api_dep,
    pcm_dep,
    config_dep,
  ] + filter_plugins_deps,
)
