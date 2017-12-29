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

if get_option('android_debug_keystore') != ''
  debug_apk = custom_target(
    'mpd-debug.apk',
    output: 'mpd-debug.apk',
    input: unsigned_apk,
    command: [
      jarsigner,
      '-keystore', get_option('android_debug_keystore'),
      '-storepass', 'android',
      '-signedjar', '@OUTPUT@',
      '@INPUT@',
      'androiddebugkey',
    ],
    build_by_default: true
  )
endif

if get_option('android_keystore') != '' and get_option('android_keyalias') != '' and get_option('android_keypass') != ''
  unaligned_apk = custom_target(
    'mpd-unaligned.apk',
    output: 'mpd-unaligned.apk',
    input: unsigned_apk,
    command: [
      jarsigner,
      '-digestalg', 'SHA1', '-sigalg', 'MD5withRSA',
      '-keystore', get_option('android_keystore'),
      '-storepass', get_option('android_keypass'),
      '-signedjar', '@OUTPUT@',
      '@INPUT@',
      get_option('android_keyalias'),
    ],
  )

  apk = custom_target(
    'mpd.apk',
    output: 'mpd.apk',
    input: unaligned_apk,
    command: [
      android_zipalign,
      '-f', '4',
      '@INPUT@', '@OUTPUT@',
    ],
    build_by_default: true
  )
endif
