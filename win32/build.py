#!/usr/bin/env python3

import os, os.path
import sys, subprocess
import shutil

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
        self.actual_arch = arch
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
        self.windres = os.path.join(toolchain_bin, arch + '-windres')

        common_flags = '-O2 -g'

        if not x64:
            # enable SSE support which is required for LAME
            common_flags += ' -march=pentium3'

        self.cflags = common_flags
        self.cxxflags = common_flags
        self.cppflags = '-isystem ' + os.path.join(install_prefix, 'include') + \
                        ' -DWINVER=0x0600 -D_WIN32_WINNT=0x0600'
        self.ldflags = '-L' + os.path.join(install_prefix, 'lib') + \
                       ' -static-libstdc++ -static-libgcc'
        self.libs = ''

        # Explicitly disable _FORTIFY_SOURCE because it is broken with
        # mingw.  This prevents some libraries such as libFLAC to
        # enable it.
        self.cppflags += ' -D_FORTIFY_SOURCE=0'

        self.is_arm = arch.startswith('arm')
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_aarch64 = arch == 'aarch64'
        self.is_windows = 'mingw32' in arch

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
    zlib,
    libid3tag,
    liblame,
    libmodplug,
    libopenmpt,
    wildmidi,
    gme,
    ffmpeg,
    curl,
    libnfs,
    jack,
    boost,
]

# build the third-party libraries
toolchain = CrossGccToolchain('/usr', host_arch,
                              tarball_path, src_path, build_path, root_path)

for x in thirdparty_libs:
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD

from build.meson import configure as run_meson
run_meson(toolchain, mpd_path, '.', configure_args)
subprocess.check_call(['/usr/bin/ninja'], env=toolchain.env)
