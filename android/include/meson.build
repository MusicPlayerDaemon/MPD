javac = find_program('javac')

common_cppflags += '-I' + meson.current_build_dir()

bridge_header = custom_target(
  'org_musicpd_Bridge.h',
  output: 'org_musicpd_Bridge.h',
  input: [
    '../app/src/main/java/org/musicpd/Bridge.java',
  ],
  command: [
    javac,
    '-source', '1.8', '-target', '1.8',
    '-Xlint:-options',
    '-cp', join_paths(android_sdk_platform_dir, 'android.jar'),

    # not interested in *.class, we only want the JNI header
    '-d', '@PRIVATE_DIR@',

    '-h', '@OUTDIR@',
    '@INPUT@',
  ],
)
