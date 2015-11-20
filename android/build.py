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
lib_path = os.path.abspath('lib')

shared_path = lib_path
if 'MPD_SHARED_LIB' in os.environ:
    shared_path = os.environ['MPD_SHARED_LIB']
tarball_path = os.path.join(shared_path, 'download')
src_path = os.path.join(shared_path, 'src')

arch_path = os.path.join(lib_path, arch)
build_path = os.path.join(arch_path, 'build')

# build host configuration
build_arch = 'linux-x86_64'

# set up the NDK toolchain

class AndroidNdkToolchain:
    def __init__(self, tarball_path, src_path, build_path,
                 use_cxx, use_clang):
        self.tarball_path = tarball_path
        self.src_path = src_path
        self.build_path = build_path

        self.ndk_arch = 'arm'
        android_abi = 'armeabi-v7a'
        ndk_platform = 'android-14'

        # select the NDK compiler
        gcc_version = '4.9'
        llvm_version = '3.6'

        ndk_platform_path = os.path.join(ndk_path, 'platforms', ndk_platform)
        sysroot = os.path.join(ndk_platform_path, 'arch-' + self.ndk_arch)

        install_prefix = os.path.join(arch_path, 'root')

        self.arch = arch
        self.install_prefix = install_prefix
        self.sysroot = sysroot

        toolchain_path = os.path.join(ndk_path, 'toolchains', arch + '-' + gcc_version, 'prebuilt', build_arch)
        llvm_path = os.path.join(ndk_path, 'toolchains', 'llvm-' + llvm_version, 'prebuilt', build_arch)
        llvm_triple = 'armv7-none-linux-androideabi'

        common_flags = '-march=armv7-a -mfloat-abi=softfp'

        toolchain_bin = os.path.join(toolchain_path, 'bin')
        if use_clang:
            llvm_bin = os.path.join(llvm_path, 'bin')
            self.cc = os.path.join(llvm_bin, 'clang')
            self.cxx = os.path.join(llvm_bin, 'clang++')
            common_flags += ' -target ' + llvm_triple + ' -integrated-as -gcc-toolchain ' + toolchain_path
        else:
            self.cc = os.path.join(toolchain_bin, arch + '-gcc')
            self.cxx = os.path.join(toolchain_bin, arch + '-g++')

        self.ar = os.path.join(toolchain_bin, arch + '-ar')
        self.nm = os.path.join(toolchain_bin, arch + '-nm')
        self.strip = os.path.join(toolchain_bin, arch + '-strip')

        self.cflags = '-Os -g ' + common_flags
        self.cxxflags = '-Os -g ' + common_flags
        self.cppflags = '--sysroot=' + self.sysroot + ' -isystem ' + os.path.join(install_prefix, 'include')
        self.ldflags = '--sysroot=' + self.sysroot + ' -L' + os.path.join(install_prefix, 'lib')
        self.libs = ''

        self.is_arm = self.ndk_arch == 'arm'
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_windows = False

        libstdcxx_path = os.path.join(ndk_path, 'sources/cxx-stl/gnu-libstdc++', gcc_version)
        libstdcxx_cppflags = '-isystem ' + os.path.join(libstdcxx_path, 'include') + ' -isystem ' + os.path.join(libstdcxx_path, 'libs', android_abi, 'include')
        if use_clang:
            libstdcxx_cppflags += ' -D__STRICT_ANSI__'
        libstdcxx_ldadd = os.path.join(libstdcxx_path, 'libs', android_abi, 'libgnustl_static.a')

        if use_cxx:
            self.libs += ' ' + libstdcxx_ldadd
            self.cppflags += ' ' + libstdcxx_cppflags

        self.env = dict(os.environ)

        # redirect pkg-config to use our root directory instead of the
        # default one on the build host
        self.env['PKG_CONFIG_LIBDIR'] = os.path.join(install_prefix, 'lib/pkgconfig')

from build.project import Project
from build.autotools import AutotoolsProject
from build.ffmpeg import FfmpegProject
from build.boost import BoostProject

# a list of third-party libraries to be used by MPD on Android
thirdparty_libs = [
    AutotoolsProject(
        'http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.xz',
        '5c3a34309d8b98640827e5d0991a4015',
        'lib/libogg.a',
        ['--disable-shared', '--enable-static'],
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.xz',
        '28cb28097c07a735d6af56e598e1c90f',
        'lib/libvorbis.a',
        ['--disable-shared', '--enable-static'],
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz',
        'c5a8cf7c0b066759542bc4ca46817ac6',
        'lib/libopus.a',
        ['--disable-shared', '--enable-static'],
        use_clang=True,
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/flac/flac-1.3.1.tar.xz',
        'b9922c9a0378c88d3e901b234f852698',
        'lib/libFLAC.a',
        [
            '--disable-shared', '--enable-static',
            '--disable-xmms-plugin', '--disable-cpplibs',
        ],
        use_clang=True,
    ),

    AutotoolsProject(
        'ftp://ftp.mars.org/pub/mpeg/libid3tag-0.15.1b.tar.gz',
        'e5808ad997ba32c498803822078748c3',
        'lib/libid3tag.a',
        ['--disable-shared', '--enable-static'],
        autogen=True,
    ),

    AutotoolsProject(
        'ftp://ftp.mars.org/pub/mpeg/libmad-0.15.1b.tar.gz',
        '1be543bc30c56fb6bea1d7bf6a64e66c',
        'lib/libmad.a',
        ['--disable-shared', '--enable-static'],
        autogen=True,
    ),

    FfmpegProject(
        'http://ffmpeg.org/releases/ffmpeg-2.8.2.tar.xz',
        '5041ffe661392b0685d2248114791fde',
        'lib/libavcodec.a',
        [
            '--disable-shared', '--enable-static',
            '--enable-gpl',
            '--enable-small',
            '--disable-pthreads',
            '--disable-programs',
            '--disable-doc',
            '--disable-avdevice',
            '--disable-swresample',
            '--disable-swscale',
            '--disable-postproc',
            '--disable-avfilter',
            '--disable-network',
            '--disable-encoders',
            '--disable-protocols',
            '--disable-outdevs',
            '--disable-filters',
        ],
    ),

    AutotoolsProject(
        'http://curl.haxx.se/download/curl-7.45.0.tar.lzma',
        'c9a0a77f71fdc6b0f925bc3e79eb77f6',
        'lib/libcurl.a',
        [
            '--disable-shared', '--enable-static',
            '--disable-debug',
            '--enable-http',
            '--enable-ipv6',
            '--disable-ftp', '--disable-file',
            '--disable-ldap', '--disable-ldaps',
            '--disable-rtsp', '--disable-proxy', '--disable-dict', '--disable-telnet',
            '--disable-tftp', '--disable-pop3', '--disable-imap', '--disable-smtp',
            '--disable-gopher',
            '--disable-manual',
            '--disable-threaded-resolver', '--disable-verbose', '--disable-sspi',
            '--disable-crypto-auth', '--disable-ntlm-wb', '--disable-tls-srp', '--disable-cookies',
            '--without-ssl', '--without-gnutls', '--without-nss', '--without-libssh2',
        ],
        use_clang=True,
    ),

    BoostProject(
        'http://netcologne.dl.sourceforge.net/project/boost/boost/1.59.0/boost_1_59_0.tar.bz2',
        '6aa9a5c6a4ca1016edd0ed1178e3cb87',
        'include/boost/version.hpp',
    ),
]

# build the third-party libraries
for x in thirdparty_libs:
    toolchain = AndroidNdkToolchain(tarball_path, src_path, build_path,
                                    use_cxx=x.use_cxx, use_clang=x.use_clang)
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD
toolchain = AndroidNdkToolchain(tarball_path, src_path, build_path,
                                use_cxx=True, use_clang=True)

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
    'STRIP=' + toolchain.strip,
    '--host=' + toolchain.arch,
    '--prefix=' + toolchain.install_prefix,
    '--with-sysroot=' + toolchain.sysroot,
    '--with-android-sdk=' + sdk_path,

    '--enable-silent-rules',

    '--disable-icu',

] + configure_args

subprocess.check_call(configure, env=toolchain.env)
subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], env=toolchain.env)
