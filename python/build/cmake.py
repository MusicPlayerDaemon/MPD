import os
import subprocess

from build.project import Project

def __write_cmake_compiler(f, language, compiler):
    s = compiler.split(' ', 1)
    if len(s) == 2:
        print(f'set(CMAKE_{language}_COMPILER_LAUNCHER {s[0]})', file=f)
        compiler = s[1]
    print(f'set(CMAKE_{language}_COMPILER {compiler})', file=f)

def __write_cmake_toolchain_file(f, toolchain):
    if '-darwin' in toolchain.actual_arch:
        cmake_system_name = 'Darwin'
    elif toolchain.is_windows:
        cmake_system_name = 'Windows'
    else:
        cmake_system_name = 'Linux'

    f.write(f"""
set(CMAKE_SYSTEM_NAME {cmake_system_name})
set(CMAKE_SYSTEM_PROCESSOR {toolchain.actual_arch.split('-', 1)[0]})

set(CMAKE_C_COMPILER_TARGET {toolchain.actual_arch})
set(CMAKE_CXX_COMPILER_TARGET {toolchain.actual_arch})

set(CMAKE_C_FLAGS "{toolchain.cflags} {toolchain.cppflags}")
set(CMAKE_CXX_FLAGS "{toolchain.cxxflags} {toolchain.cppflags}")
""")
    __write_cmake_compiler(f, 'C', toolchain.cc)
    __write_cmake_compiler(f, 'CXX', toolchain.cxx)

def configure(toolchain, src, build, args=()):
    cross_args = []

    if toolchain.is_windows:
        cross_args.append('-DCMAKE_RC_COMPILER=' + toolchain.windres)

    # Several targets need a sysroot to prevent pkg-config from
    # looking for libraries on the build host (TODO: fix this
    # properly); but we must not do that on Android because the NDK
    # has a sysroot already
    if '-android' not in toolchain.actual_arch and '-darwin' not in toolchain.actual_arch:
        cross_args.append('-DCMAKE_SYSROOT=' + toolchain.install_prefix)

    os.makedirs(build, exist_ok=True)
    cmake_toolchain_file = os.path.join(build, 'cmake_toolchain_file')
    with open(cmake_toolchain_file, 'w') as f:
        __write_cmake_toolchain_file(f, toolchain)

    configure = [
        'cmake',
        src,

        '-DCMAKE_TOOLCHAIN_FILE=' + cmake_toolchain_file,

        '-DCMAKE_INSTALL_PREFIX=' + toolchain.install_prefix,
        '-DCMAKE_BUILD_TYPE=release',

        '-GNinja',
    ] + cross_args + args

    subprocess.check_call(configure, env=toolchain.env, cwd=build)

class CmakeProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 windows_configure_args=[],
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.windows_configure_args = windows_configure_args

    def configure(self, toolchain):
        src = self.unpack(toolchain)
        build = self.make_build_path(toolchain)
        configure_args = self.configure_args
        if toolchain.is_windows:
            configure_args = configure_args + self.windows_configure_args
        configure(toolchain, src, build, configure_args)
        return build

    def _build(self, toolchain):
        build = self.configure(toolchain)
        subprocess.check_call(['ninja', 'install'],
                              cwd=build, env=toolchain.env)
