android_package = 'org.musicpd'
android_package_path = join_paths(android_package.split('.'))

android_ndk = get_option('android_ndk')
android_sdk = get_option('android_sdk')
android_abi = get_option('android_abi')

android_sdk_build_tools_version = '29.0.3'
android_sdk_platform = 'android-29'

android_build_tools_dir = join_paths(android_sdk, 'build-tools', android_sdk_build_tools_version)
android_sdk_platform_dir = join_paths(android_sdk, 'platforms', android_sdk_platform)


android_gradlew = join_paths(meson.current_source_dir(), 'gradlew')

javac = find_program('javac')

common_cppflags += '-I' + join_paths(meson.current_build_dir(), 'include')

bridge_header = custom_target(
  'org_musicpd_Bridge.h',
  output: 'org_musicpd_Bridge.h',
  input: [
    'app/src/main/java/org/musicpd/Bridge.java',
  ],
  command: [
    join_paths(meson.current_source_dir(), 'run-javac.sh'),
    javac,
    join_paths(android_sdk_platform_dir, 'android.jar'),
    '@OUTDIR@',
    '@INPUT@',
  ],
)
