javac = find_program('javac')

common_cppflags += '-I' + meson.current_build_dir()

bridge_header = custom_target(
  'org_musicpd_Bridge.h',
  output: 'org_musicpd_Bridge.h',
  input: [
    '../app/src/main/java/org/musicpd/Bridge.java',
  ],
  command: [
    join_paths(meson.current_source_dir(), 'run-javac.sh'),
    javac,
    join_paths(android_sdk_platform_dir, 'android.jar'),
    '@OUTDIR@',
    '@INPUT@',
  ],
)
