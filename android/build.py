#!/usr/bin/env -S python3 -u

import os, os.path
import shutil
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

# the path to the MPD sources
mpd_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))
sys.path[0] = os.path.join(mpd_path, 'python')

# output directories
from build.dirs import lib_path, tarball_path, src_path
from build.toolchain import AndroidNdkToolchain

# a list of third-party libraries to be used by MPD on Android
from build.libs import *
thirdparty_libs = [
    libmodplug,
    wildmidi,
    gme,
    ffmpeg,
    libnfs,
]

# build the third-party libraries
for x in thirdparty_libs:
    toolchain = AndroidNdkToolchain(mpd_path, lib_path,
                                    tarball_path, src_path,
                                    ndk_path, android_abi,
                                    use_cxx=x.use_cxx)
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD
toolchain = AndroidNdkToolchain(mpd_path, lib_path,
                                tarball_path, src_path,
                                ndk_path, android_abi,
                                use_cxx=True)

configure_args += [
    '-Dandroid_sdk=' + sdk_path,
    '-Dandroid_ndk=' + ndk_path,
    '-Dandroid_abi=' + android_abi,
    '-Dandroid_strip=' + toolchain.strip,
    '-Dopenssl:asm=disabled'
]

from build.meson import configure as run_meson
run_meson(toolchain, mpd_path, '.', configure_args)

ninja = shutil.which("ninja")
subprocess.check_call([ninja], env=toolchain.env)
