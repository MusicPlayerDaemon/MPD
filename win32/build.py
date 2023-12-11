#!/usr/bin/env -S python3 -u

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
from build.toolchain import MingwToolchain

arch_path = os.path.join(lib_path, host_arch)
build_path = os.path.join(arch_path, 'build')
root_path = os.path.join(arch_path, 'root')

# a list of third-party libraries to be used by MPD on Android
from build.libs import *
thirdparty_libs = [
    libmpdclient,
    zlib,
    libid3tag,
    liblame,
    libmodplug,
    libopenmpt,
    wildmidi,
    gme,
    ffmpeg,
    libnfs,
    libsamplerate,
]

# build the third-party libraries
toolchain = MingwToolchain(mpd_path,
                           '/usr', host_arch, x64,
                           tarball_path, src_path, build_path, root_path)

for x in thirdparty_libs:
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD

from build.meson import configure as run_meson
run_meson(toolchain, mpd_path, '.', configure_args)
subprocess.check_call(['/usr/bin/ninja'], env=toolchain.env)
