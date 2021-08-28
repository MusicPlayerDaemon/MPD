import os.path, subprocess

from build.project import Project

class FfmpegProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 cppflags='',
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.cppflags = cppflags

    def _build(self, toolchain):
        src = self.unpack(toolchain)
        build = self.make_build_path(toolchain)

        if toolchain.is_arm:
            arch = 'arm'
        elif toolchain.is_aarch64:
            arch = 'aarch64'
        else:
            arch = 'x86'

        if toolchain.is_windows:
            target_os = 'mingw32'
        else:
            target_os = 'linux'

        configure = [
            os.path.join(src, 'configure'),
            '--cc=' + toolchain.cc,
            '--cxx=' + toolchain.cxx,
            '--nm=' + toolchain.nm,
            '--extra-cflags=' + toolchain.cflags + ' ' + toolchain.cppflags + ' ' + self.cppflags,
            '--extra-cxxflags=' + toolchain.cxxflags + ' ' + toolchain.cppflags + ' ' + self.cppflags,
            '--extra-ldflags=' + toolchain.ldflags,
            '--extra-libs=' + toolchain.libs,
            '--ar=' + toolchain.ar,
            '--ranlib=' + toolchain.ranlib,
            '--enable-cross-compile',
            '--arch=' + arch,
            '--target-os=' + target_os,
            '--prefix=' + toolchain.install_prefix,
        ] + self.configure_args

        if toolchain.is_armv7:
            configure.append('--cpu=cortex-a8')

        subprocess.check_call(configure, cwd=build, env=toolchain.env)
        subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], cwd=build, env=toolchain.env)
        subprocess.check_call(['/usr/bin/make', '--quiet', 'install'], cwd=build, env=toolchain.env)
