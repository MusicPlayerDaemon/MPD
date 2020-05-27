import subprocess

from build.project import Project

def configure(toolchain, src, build, args=()):
    cross_args = []

    if toolchain.is_windows:
        cross_args.append('-DCMAKE_SYSTEM_NAME=Windows')
        cross_args.append('-DCMAKE_RC_COMPILER=' + toolchain.windres)

    configure = [
        'cmake',
        src,

        '-DCMAKE_INSTALL_PREFIX=' + toolchain.install_prefix,
        '-DCMAKE_BUILD_TYPE=release',

        '-DCMAKE_C_COMPILER=' + toolchain.cc,
        '-DCMAKE_CXX_COMPILER=' + toolchain.cxx,

        '-DCMAKE_C_FLAGS=' + toolchain.cflags + ' ' + toolchain.cppflags,
        '-DCMAKE_CXX_FLAGS=' + toolchain.cxxflags + ' ' + toolchain.cppflags,

        '-GNinja',
    ] + cross_args + args

    subprocess.check_call(configure, env=toolchain.env, cwd=build)

class CmakeProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args

    def configure(self, toolchain):
        src = self.unpack(toolchain)
        build = self.make_build_path(toolchain)
        configure(toolchain, src, build, self.configure_args)
        return build

    def build(self, toolchain):
        build = self.configure(toolchain)
        subprocess.check_call(['ninja', 'install'],
                              cwd=build, env=toolchain.env)
