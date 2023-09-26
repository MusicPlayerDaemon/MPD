import os
import subprocess
import platform
from typing import Optional, Sequence, Union

from build.project import Project
from .toolchain import AnyToolchain

def __no_ccache(cmd: str) -> str:
    if cmd.startswith('ccache '):
        cmd = cmd[7:]
    return cmd

def make_cross_file(toolchain: AnyToolchain) -> str:
    if toolchain.is_windows:
        system = 'windows'
        windres = "windres = '%s'" % toolchain.windres
    else:
        system = 'linux'
        windres = ''

    if toolchain.is_arm:
        cpu_family = 'arm'
        if toolchain.is_armv7:
            cpu = 'armv7'
        else:
            cpu = 'armv6'
    elif toolchain.is_aarch64:
        cpu_family = 'aarch64'
        cpu = 'arm64-v8a'
    else:
        cpu_family = 'x86'
        if 'x86_64' in toolchain.host_triplet:
            cpu = 'x86_64'
        else:
            cpu = 'i686'

    # TODO: support more CPUs
    endian = 'little'

    # TODO: write pkg-config wrapper

    path = os.path.join(toolchain.build_path, 'meson.cross')
    os.makedirs(toolchain.build_path, exist_ok=True)
    with open(path, 'w') as f:
        f.write(f"""
[binaries]
c = '{__no_ccache(toolchain.cc)}'
cpp = '{__no_ccache(toolchain.cxx)}'
ar = '{toolchain.ar}'
strip = '{toolchain.strip}'
pkgconfig = '{toolchain.pkg_config}'
""")

        if toolchain.is_windows and platform.system() != 'Windows':
            f.write(f"windres = '{toolchain.windres}'\n")

            # Run unit tests with WINE when cross-building for Windows
            print("exe_wrapper = 'wine'", file=f)

        f.write(f"""
[properties]
root = '{toolchain.install_prefix}'
""")

        if toolchain.is_android:
            f.write("""
# Keep Meson from executing Android-x86 test binariees
needs_exe_wrapper = true
""")

        f.write(f"""
[built-in options]
c_args = {repr((toolchain.cppflags + ' ' + toolchain.cflags).split())}
c_link_args = {repr(toolchain.ldflags.split() + toolchain.libs.split())}

cpp_args = {repr((toolchain.cppflags + ' ' + toolchain.cxxflags).split())}
cpp_link_args = {repr(toolchain.ldflags.split() + toolchain.libs.split())}
""")

        f.write(f"""
[host_machine]
system = '{system}'
cpu_family = '{cpu_family}'
cpu = '{cpu}'
endian = '{endian}'
""")
    return path

def configure(toolchain: AnyToolchain, src: str, build: str, args: list[str]=[]) -> None:
    configure = [
        'meson', 'setup',
        build, src,

        '--prefix', toolchain.install_prefix,

        '--buildtype', 'plain',

        '--default-library=static',
    ] + args

    if toolchain.host_triplet is not None:
        # cross-compiling: write a cross-file
        cross_file = make_cross_file(toolchain)
        configure.append(f'--cross-file={cross_file}')

    env = toolchain.env.copy()

    subprocess.check_call(configure, env=env)

class MesonProject(Project):
    def __init__(self, url: Union[str, Sequence[str]], md5: str, installed: str,
                 configure_args: list[str]=[],
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args

    def configure(self, toolchain: AnyToolchain) -> str:
        src = self.unpack(toolchain)
        build = self.make_build_path(toolchain)
        configure(toolchain, src, build, self.configure_args)
        return build

    def _build(self, toolchain: AnyToolchain) -> None:
        build = self.configure(toolchain)
        subprocess.check_call(['ninja', '-v', 'install'],
                              cwd=build, env=toolchain.env)
