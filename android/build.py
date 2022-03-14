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
    print("SDK not found in", sdk_path, file=sys.stderr)
    sys.exit(1)

if not os.path.isdir(ndk_path):
    print("NDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

android_abis = {
    'armeabi-v7a': {
        'arch': 'arm-linux-androideabi',
        'ndk_arch': 'arm',
        'llvm_triple': 'armv7-linux-androideabi',
        'cflags': '-fpic -mfpu=neon -mfloat-abi=softfp',
    },

    'arm64-v8a': {
        'arch': 'aarch64-linux-android',
        'ndk_arch': 'arm64',
        'llvm_triple': 'aarch64-linux-android',
        'cflags': '-fpic',
    },

    'x86': {
        'arch': 'i686-linux-android',
        'ndk_arch': 'x86',
        'llvm_triple': 'i686-linux-android',
        'cflags': '-fPIC -march=i686 -mtune=intel -mssse3 -mfpmath=sse -m32',
    },

    'x86_64': {
        'arch': 'x86_64-linux-android',
        'ndk_arch': 'x86_64',
        'llvm_triple': 'x86_64-linux-android',
        'cflags': '-fPIC -m64',
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

        install_prefix = os.path.join(arch_path, 'root')

        self.arch = arch
        self.actual_arch = arch
        self.install_prefix = install_prefix

        llvm_path = os.path.join(ndk_path, 'toolchains', 'llvm', 'prebuilt', build_arch)
        llvm_triple = abi_info['llvm_triple'] + android_api_level

        common_flags = '-Os -g'
        common_flags += ' ' + abi_info['cflags']

        llvm_bin = os.path.join(llvm_path, 'bin')
        self.cc = os.path.join(llvm_bin, 'clang')
        self.cxx = os.path.join(llvm_bin, 'clang++')
        common_flags += ' -target ' + llvm_triple

        common_flags += ' -fvisibility=hidden -fdata-sections -ffunction-sections'

        self.ar = os.path.join(llvm_bin, 'llvm-ar')
        self.ranlib = os.path.join(llvm_bin, 'llvm-ranlib')
        self.nm = os.path.join(llvm_bin, 'llvm-nm')
        self.strip = os.path.join(llvm_bin, 'llvm-strip')

        self.cflags = common_flags
        self.cxxflags = common_flags
        self.cppflags = ' -isystem ' + os.path.join(install_prefix, 'include')
        self.ldflags = ' -L' + os.path.join(install_prefix, 'lib') + \
            ' -Wl,--exclude-libs=ALL' + \
            ' ' + common_flags
        self.ldflags = common_flags
        self.libs = ''

        self.is_arm = ndk_arch == 'arm'
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_aarch64 = ndk_arch == 'arm64'
        self.is_windows = False

        libstdcxx_flags = ''
        libstdcxx_cxxflags = ''
        libstdcxx_ldflags = ''
        libstdcxx_libs = '-static-libstdc++'

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
        os.makedirs(bin_dir, exist_ok=True)
        self.pkg_config = shutil.copy(os.path.join(mpd_path, 'build', 'pkg-config.sh'),
                                      os.path.join(bin_dir, 'pkg-config'))
        self.env['PKG_CONFIG'] = self.pkg_config

# a list of third-party libraries to be used by MPD on Android
from build.libs import *
thirdparty_libs = [
    libmpdclient,
    libogg,
    opus,
    flac,
    libid3tag,
    libmodplug,
    wildmidi,
    gme,
    ffmpeg,
    openssl,
    curl,
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
