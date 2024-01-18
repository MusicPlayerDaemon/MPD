android_package = 'org.musicpd'
android_package_path = join_paths(android_package.split('.'))

android_ndk = get_option('android_ndk')
android_sdk = get_option('android_sdk')
android_abi = get_option('android_abi')

android_sdk_build_tools_version = '34.0.0'
android_sdk_platform = 'android-34'

android_build_tools_dir = join_paths(android_sdk, 'build-tools', android_sdk_build_tools_version)
android_sdk_platform_dir = join_paths(android_sdk, 'platforms', android_sdk_platform)


android_gradlew = join_paths(meson.current_source_dir(), 'gradlew')

subdir('include')
