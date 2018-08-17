import os.path, subprocess, sys

from build.project import Project

class MesonProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args

    def _make_cross_file(self, toolchain):
        if toolchain.is_windows:
            system = 'windows'
        else:
            system = 'linux'

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
            if 'x86_64' in toolchain.arch:
                cpu = 'x86_64'
            else:
                cpu = 'i686'

        # TODO: support more CPUs
        endian = 'little'

        # TODO: write pkg-config wrapper

        path = os.path.join(toolchain.build_path, 'meson.cross')
        os.makedirs(toolchain.build_path, exist_ok=True)
        with open(path, 'w') as f:
            f.write("""
[binaries]
c = '%s'
cpp = '%s'
ar = '%s'
strip = '%s'

[properties]
root = '%s'

c_args = %s
c_link_args = %s

cpp_args = %s
cpp_link_args = %s

# Keep Meson from executing Android-x86 test binariees
needs_exe_wrapper = true

[host_machine]
system = '%s'
cpu_family = '%s'
cpu = '%s'
endian = '%s'
            """ % (toolchain.cc, toolchain.cxx, toolchain.ar, toolchain.strip,
                   toolchain.install_prefix,
                   repr((toolchain.cppflags + ' ' + toolchain.cflags).split()),
                   repr(toolchain.ldflags.split()),
                   repr((toolchain.cppflags + ' ' + toolchain.cxxflags).split()),
                   repr(toolchain.ldflags.split()),
                   system, cpu_family, cpu, endian))
        return path

    def configure(self, toolchain):
        src = self.unpack(toolchain)
        cross_file = self._make_cross_file(toolchain)
        build = self.make_build_path(toolchain)
        configure = [
            'meson',
            src, build,

            '--prefix', toolchain.install_prefix,

            # this is necessary because Meson uses Debian's build machine
            # MultiArch path (e.g. "lib/x86_64-linux-gnu") for cross
            # builds, which is obviously wrong
            '--libdir', 'lib',

            '--buildtype', 'plain',

            '--default-library=static',

            '--cross-file', cross_file,
        ] + self.configure_args

        subprocess.check_call(configure, env=toolchain.env)
        return build

    def build(self, toolchain):
        build = self.configure(toolchain)
        subprocess.check_call(['ninja', 'install'],
                              cwd=build, env=toolchain.env)
