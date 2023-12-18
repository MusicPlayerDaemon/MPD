android_abi = get_option('android_abi')

mpd_so = custom_target(
  'libmpd.so',
  output: 'libmpd.so',
  input: mpd,
  command: [
    'cp',
    '@INPUT@',
    join_paths(meson.current_source_dir(), '../app/src/main/jnilibs/', android_abi),
  ],
  build_by_default: true
)
