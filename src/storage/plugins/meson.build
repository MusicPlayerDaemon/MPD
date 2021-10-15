storage_plugins_sources = [
  'LocalStorage.cxx',
]

webdav_option = get_option('webdav')
enable_webdav = false
if not webdav_option.disabled()
  enable_webdav = true

  if not curl_dep.found()
    if webdav_option.enabled()
      error('WebDAV requires CURL')
    endif
    enable_webdav = false
  endif

  if not expat_dep.found()
    if webdav_option.enabled()
      error('WebDAV requires Expat')
    endif
    enable_webdav = false
  endif

  if enable_webdav
    storage_plugins_sources += 'CurlStorage.cxx'
  endif
endif
conf.set('ENABLE_WEBDAV', enable_webdav)

if nfs_dep.found()
  storage_plugins_sources += 'NfsStorage.cxx'
endif

if smbclient_dep.found()
  storage_plugins_sources += 'SmbclientStorage.cxx'
endif

if enable_udisks
  storage_plugins_sources += 'UdisksStorage.cxx'
endif

storage_plugins = static_library(
  'storage_plugins',
  storage_plugins_sources,
  include_directories: inc,
  dependencies: [
    log_dep,
    curl_dep,
    dbus_dep,
    expat_dep,
    nfs_dep,
    smbclient_dep,
  ],
)

storage_plugins_dep = declare_dependency(
  link_with: storage_plugins,
  dependencies: [
    storage_api_dep,
    fs_dep,
    time_dep,
  ],
)
