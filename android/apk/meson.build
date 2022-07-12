unsigned_apk = custom_target(
  'mpd-unsigned.apk',
  output: 'mpd-unsigned.apk',
  input: [mpd, classes_dex, resources_apk[0]],
  command: [
    join_paths(meson.current_source_dir(), 'make-unsigned-apk.sh'),
    android_abi,
    get_option('android_strip'),
    zip,
    '@OUTPUT0@',
    '@INPUT@',
  ],
)

aligned_apk = custom_target(
  'mpd-aligned.apk',
  output: 'mpd-aligned.apk',
  input: unsigned_apk,
  command: [
    android_zipalign,
    '-f', '4',
    '@INPUT@', '@OUTPUT@',
  ],
)

if get_option('android_debug_keystore') != ''
  debug_apk = custom_target(
    'mpd-debug.apk',
    output: 'mpd-debug.apk',
    input: aligned_apk,
    command: [
      apksigner, 'sign',
      '--in', '@INPUT@',
      '--out', '@OUTPUT@',
      '--debuggable-apk-permitted',
      '-ks', get_option('android_debug_keystore'),
      '--ks-key-alias', 'androiddebugkey',
      '--ks-pass', 'pass:android',
    ],
    build_by_default: true
  )
endif

if get_option('android_keystore') != '' and get_option('android_keyalias') != '' and get_option('android_keypass') != ''
  unaligned_apk = custom_target(
    'mpd.apk',
    output: 'mpd.apk',
    input: aligned_apk,
    command: [
      apksigner, 'sign',
      '--in', '@INPUT@',
      '--out', '@OUTPUT@',
      '-ks', get_option('android_keystore'),
      '--ks-key-alias', get_option('android_keyalias'),
      '--ks-pass', 'pass:' + get_option('android_keypass'),
    ],
  )
endif
