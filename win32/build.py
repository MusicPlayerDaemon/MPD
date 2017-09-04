#!/usr/bin/env python3

import os, os.path
import sys, subprocess

configure_args = sys.argv[1:]

x64 = True

while len(configure_args) > 0:
    arg = configure_args[0]
    if arg == '--64':
        x64 = True
    elif arg == '--32':
        x64 = False
    else:
        break
    configure_args.pop(0)

if x64:
    host_arch = 'x86_64-w64-mingw32'
else:
    host_arch = 'i686-w64-mingw32'

# the path to the MPD sources
mpd_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))
sys.path[0] = os.path.join(mpd_path, 'python')

# output directories
from build.dirs import lib_path, tarball_path, src_path

arch_path = os.path.join(lib_path, host_arch)
build_path = os.path.join(arch_path, 'build')
root_path = os.path.join(arch_path, 'root')

class CrossGccToolchain:
    def __init__(self, toolchain_path, arch,
                 tarball_path, src_path, build_path, install_prefix):
        self.arch = arch
        self.tarball_path = tarball_path
        self.src_path = src_path
        self.build_path = build_path
        self.install_prefix = install_prefix

        toolchain_bin = os.path.join(toolchain_path, 'bin')
        self.cc = os.path.join(toolchain_bin, arch + '-gcc')
        self.cxx = os.path.join(toolchain_bin, arch + '-g++')
        self.ar = os.path.join(toolchain_bin, arch + '-ar')
        self.ranlib = os.path.join(toolchain_bin, arch + '-ranlib')
        self.nm = os.path.join(toolchain_bin, arch + '-nm')
        self.strip = os.path.join(toolchain_bin, arch + '-strip')

        common_flags = ''

        if not x64:
            # enable SSE support which is required for LAME
            common_flags += ' -march=pentium3'

        self.cflags = '-O2 -g ' + common_flags
        self.cxxflags = '-O2 -g ' + common_flags
        self.cppflags = '-isystem ' + os.path.join(install_prefix, 'include')
        self.ldflags = '-L' + os.path.join(install_prefix, 'lib')
        self.libs = ''

        self.is_arm = arch.startswith('arm')
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_windows = 'mingw32' in arch

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
    zlib,
    libid3tag,
    liblame,
    ffmpeg,
    curl,
    boost,
]

# build the third-party libraries
toolchain = CrossGccToolchain('/usr', host_arch,
                              tarball_path, src_path, build_path, root_path)

for x in thirdparty_libs:
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD

configure = [
    os.path.join(mpd_path, 'configure'),
    'CC=' + toolchain.cc,
    'CXX=' + toolchain.cxx,
    'CFLAGS=' + toolchain.cflags,
    'CXXFLAGS=' + toolchain.cxxflags,
    'CPPFLAGS=' + toolchain.cppflags,
    'LDFLAGS=' + toolchain.ldflags + ' -static',
    'LIBS=' + toolchain.libs,
    'AR=' + toolchain.ar,
    'RANLIB=' + toolchain.ranlib,
    'STRIP=' + toolchain.strip,
    '--host=' + toolchain.arch,
    '--prefix=' + toolchain.install_prefix,

    '--enable-silent-rules',

    '--disable-icu',

] + configure_args

from build.cmdline import concatenate_cmdline_variables
configure = concatenate_cmdline_variables(configure,
    set(('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS')))

subprocess.check_call(configure, env=toolchain.env)
subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], env=toolchain.env)
