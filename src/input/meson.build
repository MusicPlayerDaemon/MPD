input_features = configuration_data()

input_api = static_library(
  'input_api',
  'Error.cxx',
  'InputPlugin.cxx',
  'InputStream.cxx',
  include_directories: inc,
  dependencies: [
    thread_dep,
    nfs_dep,
  ],
)

input_api_dep = declare_dependency(
  link_with: input_api,
  dependencies: [
    thread_dep,
  ],
)

input_basic = static_library(
  'input_basic',
  'AsyncInputStream.cxx',
  'LastInputStream.cxx',
  'MemoryInputStream.cxx',
  'ProxyInputStream.cxx',
  'RewindInputStream.cxx',
  'TextInputStream.cxx',
  'ThreadInputStream.cxx',
  include_directories: inc,
  dependencies: [
    input_api_dep,
    thread_dep,
    event_dep,
  ],
)

input_basic_dep = declare_dependency(
  link_with: input_basic,
  dependencies: [
    input_api_dep,
    thread_dep,
    event_dep,
  ],
)

subdir('plugins')

input_glue = static_library(
  'input_glue',
  'Init.cxx',
  'Registry.cxx',
  'WaitReady.cxx',
  'Open.cxx',
  'LocalOpen.cxx',
  'ScanTags.cxx',
  'Reader.cxx',
  'BufferingInputStream.cxx',
  'BufferedInputStream.cxx',
  'MaybeBufferedInputStream.cxx',
  'cache/Config.cxx',
  'cache/Manager.cxx',
  'cache/Item.cxx',
  'cache/Stream.cxx',
  include_directories: inc,
  dependencies: [
    input_api_dep,
    input_basic_dep,
    log_dep,
  ],
)

input_glue_dep = declare_dependency(
  link_with: input_glue,
  dependencies: [
    input_api_dep,
    input_basic_dep,
    input_plugins_dep,
    fs_dep,
    config_dep,
    tag_dep,
  ],
)

configure_file(output: 'Features.h', configuration: input_features)
