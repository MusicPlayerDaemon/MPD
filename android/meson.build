android_package = 'org.musicpd'
android_package_path = join_paths(android_package.split('.'))

android_ndk = get_option('android_ndk')
android_sdk = get_option('android_sdk')
android_abi = get_option('android_abi')

android_sdk_build_tools_version = '29.0.3'
android_sdk_platform = 'android-29'

android_build_tools_dir = join_paths(android_sdk, 'build-tools', android_sdk_build_tools_version)
android_sdk_platform_dir = join_paths(android_sdk, 'platforms', android_sdk_platform)

android_aidl = join_paths(android_build_tools_dir, 'aidl')
android_aapt = join_paths(android_build_tools_dir, 'aapt')
android_dx = join_paths(android_build_tools_dir, 'dx')
android_zipalign = join_paths(android_build_tools_dir, 'zipalign')

javac = find_program('javac')
jarsigner = find_program('jarsigner')
rsvg_convert = find_program('rsvg-convert')
convert = find_program('convert')
zip = find_program('zip')

common_cppflags += '-I' + join_paths(meson.current_build_dir(), 'include')

#
# AIDL
#

IMainCallback_java = custom_target(
  'IMainCallback.java',
  output: 'IMainCallback.java',
  input: join_paths(meson.current_source_dir(), 'src', 'IMainCallback.aidl'),
  command: [
    join_paths(meson.current_source_dir(), 'run-aidl.sh'),
    android_aidl,
    '@INPUT@',
    '@OUTPUT@',
    join_paths(meson.current_build_dir(), 'src'),
    android_package_path,
  ],
)

IMain_java = custom_target(
  'IMain.java',
  output: 'IMain.java',
  input: join_paths(meson.current_source_dir(), 'src', 'IMain.aidl'),
  depends: IMainCallback_java,
  command: [
    join_paths(meson.current_source_dir(), 'run-aidl.sh'),
    android_aidl,
    '@INPUT@',
    '@OUTPUT@',
    join_paths(meson.current_build_dir(), 'src'),
    android_package_path,
  ],
)

#
# Resources
#

android_icon = custom_target(
  'Android icon',
  output: 'icon.png',
  input: '../mpd.svg',
  command: [
    rsvg_convert, '--width=48', '--height=48', '@INPUT@', '-o', '@OUTPUT@',
  ],
)

android_notification_icon = custom_target(
  'Android notification icon',
  output: 'notification_icon.png',
  input: android_icon,
  command: [
    convert, '@INPUT@', '-colorspace', 'Gray', '-gamma', '2.2', '@OUTPUT@',
  ],
)

resources_apk = custom_target(
  'resources.apk',
  output: ['resources.apk', 'R.java'],
  input: [
    'res/layout/custom_notification_gb.xml',
    'res/layout/log_item.xml',
    'res/layout/settings.xml',
    'res/values/strings.xml',
    android_icon,
    android_notification_icon,
  ],
  command: [
    join_paths(meson.current_source_dir(), 'make-resources-apk.sh'),
    android_aapt,
    join_paths(android_sdk_platform_dir, 'android.jar'),
    android_package,
    android_package_path,
    '@OUTPUT0@',
  ],
)

#
# Compile Java
#

classes_jar = custom_target(
  'classes.jar',
  output: 'classes.jar',
  input: [
    'src/Bridge.java',
    'src/Loader.java',
    'src/Main.java',
    'src/Receiver.java',
    'src/Settings.java',
    IMain_java,
    IMainCallback_java,
    resources_apk[1],
  ],
  command: [
    join_paths(meson.current_source_dir(), 'run-javac.sh'),
    javac,
    join_paths(android_sdk_platform_dir, 'android.jar'),
    android_package_path,
    zip,
    '@OUTPUT@',
    '@INPUT@',
  ],
)

classes_dex = custom_target(
  'classes.dex',
  output: 'classes.dex',
  input: classes_jar,
  command: [
    android_dx,
    '--dex', '--output', '@OUTPUT@',
    '@INPUT@',
  ],
)
