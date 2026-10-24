if not upnp_dep.found()
  subdir_done()
endif

test(
  'TestUpnp',
  executable(
    'TestUpnp',
    'TestUpnpCallback.cxx',
    'TestUpnpDevice.cxx',
    'TestUpnpDirectory.cxx',
    '../../src/db/plugins/upnp/Directory.cxx',
    '../../src/db/plugins/upnp/Object.cxx',
    '../../src/db/plugins/upnp/Tags.cxx',
    include_directories: inc,
    dependencies: [
      upnp_dep,
      expat_dep,
      tag_dep,
      protocol_dep,
      fs_path_dep,
      uri_dep,
      util_dep,
      thread_dep,
      gtest_dep,
    ],
  ),
  protocol: 'gtest',
)
