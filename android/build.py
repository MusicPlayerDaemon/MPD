#!/usr/bin/env python3

import os, os.path
import sys, subprocess

if len(sys.argv) < 4:
    print("Usage: build.py SDK_PATH NDK_PATH ABI [configure_args...]", file=sys.stderr)
    sys.exit(1)

sdk_path = sys.argv[1]
ndk_path = sys.argv[2]
android_abi = sys.argv[3]
configure_args = sys.argv[4:]

if not os.path.isfile(os.path.join(sdk_path, 'tools', 'android')):
    print("SDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

if not os.path.isdir(ndk_path):
    print("NDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

android_abis = {
    'armeabi-v7a': {
        'arch': 'arm-linux-androideabi',
        'ndk_arch': 'arm',
        'toolchain_arch': 'arm-linux-androideabi',
        'llvm_triple': 'armv7-none-linux-androideabi',
        'cflags': '-march=armv7-a -mfpu=vfp -mfloat-abi=softfp',
    },

    'arm64-v8a': {
        'android_api_level': '21',
        'arch': 'aarch64-linux-android',
        'ndk_arch': 'arm64',
        'toolchain_arch': 'aarch64-linux-android',
        'llvm_triple': 'aarch64-none-linux-android',
        'cflags': '',
    },

    'x86': {
        'arch': 'i686-linux-android',
        'ndk_arch': 'x86',
        'toolchain_arch': 'x86',
        'llvm_triple': 'i686-none-linux-android',
        'cflags': '-march=i686 -mtune=intel -mssse3 -mfpmath=sse -m32',
    },
}

# select the NDK target
abi_info = android_abis[android_abi]
arch = abi_info['arch']

# the path to the MPD sources
mpd_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))
sys.path[0] = os.path.join(mpd_path, 'python')

# output directories
from build.dirs import lib_path, tarball_path, src_path
from build.meson import configure as run_meson

arch_path = os.path.join(lib_path, arch)
build_path = os.path.join(arch_path, 'build')

# build host configuration
build_arch = 'linux-x86_64'

# set up the NDK toolchain

class AndroidNdkToolchain:
    def __init__(self, tarball_path, src_path, build_path,
                 use_cxx):
        self.tarball_path = tarball_path
        self.src_path = src_path
        self.build_path = build_path

        ndk_arch = abi_info['ndk_arch']
        android_api_level = '21'
        ndk_platform = 'android-' + android_api_level

        # select the NDK compiler
        gcc_version = '4.9'

        ndk_platform_path = os.path.join(ndk_path, 'platforms', ndk_platform)
        sysroot = os.path.join(ndk_path, 'sysroot')
        target_root = os.path.join(ndk_platform_path, 'arch-' + ndk_arch)

        install_prefix = os.path.join(arch_path, 'root')

        self.arch = arch
        self.install_prefix = install_prefix
        self.sysroot = sysroot

        toolchain_path = os.path.join(ndk_path, 'toolchains', abi_info['toolchain_arch'] + '-' + gcc_version, 'prebuilt', build_arch)
        llvm_path = os.path.join(ndk_path, 'toolchains', 'llvm', 'prebuilt', build_arch)
        llvm_triple = abi_info['llvm_triple']

        common_flags = '-Os -g'
        common_flags += ' -fPIC'
        common_flags += ' ' + abi_info['cflags']

        toolchain_bin = os.path.join(toolchain_path, 'bin')
        llvm_bin = os.path.join(llvm_path, 'bin')
        self.cc = os.path.join(llvm_bin, 'clang')
        self.cxx = os.path.join(llvm_bin, 'clang++')
        common_flags += ' -target ' + llvm_triple + ' -integrated-as -gcc-toolchain ' + toolchain_path

        common_flags += ' -fvisibility=hidden -fdata-sections -ffunction-sections'

        self.ar = os.path.join(toolchain_bin, arch + '-ar')
        self.ranlib = os.path.join(toolchain_bin, arch + '-ranlib')
        self.nm = os.path.join(toolchain_bin, arch + '-nm')
        self.strip = os.path.join(toolchain_bin, arch + '-strip')

        self.cflags = common_flags
        self.cxxflags = common_flags
        self.cppflags = '--sysroot=' + sysroot + \
            ' -isystem ' + os.path.join(install_prefix, 'include') + \
            ' -isystem ' + os.path.join(sysroot, 'usr', 'include', arch) + \
            ' -D__ANDROID_API__=' + android_api_level
        self.ldflags = '--sysroot=' + sysroot + \
            ' -L' + os.path.join(install_prefix, 'lib') + \
            ' -L' + os.path.join(target_root, 'usr', 'lib') + \
            ' -B' + os.path.join(target_root, 'usr', 'lib') + \
            ' ' + common_flags
        self.libs = ''

        self.is_arm = ndk_arch == 'arm'
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_aarch64 = ndk_arch == 'arm64'
        self.is_windows = False

        libcxx_path = os.path.join(ndk_path, 'sources/cxx-stl/llvm-libc++')
        libcxx_libs_path = os.path.join(libcxx_path, 'libs', android_abi)

        libstdcxx_flags = ''
        libstdcxx_cxxflags = libstdcxx_flags + ' -isystem ' + os.path.join(libcxx_path, 'include') + ' -isystem ' + os.path.join(ndk_path, 'sources/android/support/include')
        libstdcxx_ldflags = libstdcxx_flags + ' -L' + libcxx_libs_path
        libstdcxx_libs = '-lc++_static -lc++abi'

        if self.is_armv7:
            # On 32 bit ARM, clang generates no ".eh_frame" section;
            # instead, the LLVM unwinder library is used for unwinding
            # the stack after a C++ exception was thrown
            libstdcxx_libs += ' -lunwind'

        if use_cxx:
            self.cxxflags += ' ' + libstdcxx_cxxflags
            self.ldflags += ' ' + libstdcxx_ldflags
            self.libs += ' ' + libstdcxx_libs

        self.env = dict(os.environ)

        # redirect pkg-config to use our root directory instead of the
        # default one on the build host
        import shutil
        bin_dir = os.path.join(install_prefix, 'bin')
        try:
            os.makedirs(bin_dir)
        except:
            pass
        self.pkg_config = shutil.copy(os.path.join(mpd_path, 'build', 'pkg-config.sh'),
                                      os.path.join(bin_dir, 'pkg-config'))
        self.env['PKG_CONFIG'] = self.pkg_config

# a list of third-party libraries to be used by MPD on Android
from build.libs import *
thirdparty_libs = [
    libmpdclient,
    libogg,
    libvorbis,
    opus,
    flac,
    libid3tag,
    ffmpeg,
    curl,
    libexpat,
    libnfs,
    boost,
]

# build the third-party libraries
for x in thirdparty_libs:
    toolchain = AndroidNdkToolchain(tarball_path, src_path, build_path,
                                    use_cxx=x.use_cxx)
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD
toolchain = AndroidNdkToolchain(tarball_path, src_path, build_path,
                                use_cxx=True)

configure_args += [
    '-Dandroid_sdk=' + sdk_path,
    '-Dandroid_ndk=' + ndk_path,
    '-Dandroid_abi=' + android_abi,
    '-Dandroid_strip=' + toolchain.strip,
]

from build.meson import configure as run_meson
run_meson(toolchain, mpd_path, '.', configure_args)
subprocess.check_call(['/usr/bin/ninja'], env=toolchain.env)
