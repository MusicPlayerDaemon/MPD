#!/usr/bin/env python3

import os, os.path
import sys, subprocess

if len(sys.argv) < 3:
    print("Usage: build.py SDK_PATH NDK_PATH [configure_args...]", file=sys.stderr)
    sys.exit(1)

sdk_path = sys.argv[1]
ndk_path = sys.argv[2]
configure_args = sys.argv[3:]

if not os.path.isfile(os.path.join(sdk_path, 'tools', 'android')):
    print("SDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

if not os.path.isdir(ndk_path):
    print("NDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

# select the NDK target
arch = 'arm-linux-androideabi'

# the path to the MPD sources
mpd_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))
sys.path[0] = os.path.join(mpd_path, 'python')

# output directories
from build.dirs import lib_path, tarball_path, src_path

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

        self.ndk_arch = 'arm'
        android_abi = 'armeabi-v7a'
        ndk_platform = 'android-14'

        # select the NDK compiler
        gcc_version = '4.9'

        ndk_platform_path = os.path.join(ndk_path, 'platforms', ndk_platform)
        sysroot = os.path.join(ndk_path, 'sysroot')
        target_root = os.path.join(ndk_platform_path, 'arch-' + self.ndk_arch)

        install_prefix = os.path.join(arch_path, 'root')

        self.arch = arch
        self.install_prefix = install_prefix
        self.sysroot = sysroot

        toolchain_path = os.path.join(ndk_path, 'toolchains', arch + '-' + gcc_version, 'prebuilt', build_arch)
        llvm_path = os.path.join(ndk_path, 'toolchains', 'llvm', 'prebuilt', build_arch)
        llvm_triple = 'armv7-none-linux-androideabi'

        common_flags = '-Os -g'
        common_flags += ' -fPIC'
        common_flags += ' -march=armv7-a -mfpu=vfp -mfloat-abi=softfp'

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
            ' -D__ANDROID_API__=14'
        self.ldflags = '--sysroot=' + sysroot + \
            ' -L' + os.path.join(install_prefix, 'lib') + \
            ' -L' + os.path.join(target_root, 'usr', 'lib') + \
            ' -B' + os.path.join(target_root, 'usr', 'lib') + \
            ' ' + common_flags
        self.libs = ''

        self.is_arm = self.ndk_arch == 'arm'
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_windows = False

        libcxx_path = os.path.join(ndk_path, 'sources/cxx-stl/llvm-libc++')
        libcxx_libs_path = os.path.join(libcxx_path, 'libs', android_abi)

        libstdcxx_flags = '-stdlib=libc++'
        libstdcxx_cxxflags = libstdcxx_flags + ' -isystem ' + os.path.join(libcxx_path, 'include') + ' -isystem ' + os.path.join(ndk_path, 'sources/android/support/include')
        libstdcxx_ldflags = libstdcxx_flags + ' -static-libstdc++ -L' + libcxx_libs_path

        if use_cxx:
            self.cxxflags += ' ' + libstdcxx_cxxflags
            self.ldflags += ' ' + libstdcxx_ldflags

        self.env = dict(os.environ)

        # redirect pkg-config to use our root directory instead of the
        # default one on the build host
        self.env['PKG_CONFIG_LIBDIR'] = os.path.join(install_prefix, 'lib/pkgconfig')

# a list of third-party libraries to be used by MPD on Android
from build.libs import *
thirdparty_libs = [
    libogg,
    libvorbis,
    opus,
    flac,
    libid3tag,
    ffmpeg,
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

configure = [
    os.path.join(mpd_path, 'configure'),
    'CC=' + toolchain.cc,
    'CXX=' + toolchain.cxx,
    'CFLAGS=' + toolchain.cflags,
    'CXXFLAGS=' + toolchain.cxxflags,
    'CPPFLAGS=' + toolchain.cppflags,
    'LDFLAGS=' + toolchain.ldflags,
    'LIBS=' + toolchain.libs,
    'AR=' + toolchain.ar,
    'RANLIB=' + toolchain.ranlib,
    'STRIP=' + toolchain.strip,
    '--host=' + toolchain.arch,
    '--prefix=' + toolchain.install_prefix,
    '--with-sysroot=' + toolchain.sysroot,
    '--with-android-sdk=' + sdk_path,

    '--enable-silent-rules',

    '--disable-icu',

] + configure_args

from build.cmdline import concatenate_cmdline_variables
configure = concatenate_cmdline_variables(configure,
    set(('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS')))

subprocess.check_call(configure, env=toolchain.env)
subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], env=toolchain.env)
