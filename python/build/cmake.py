import os
import re
import subprocess
from typing import cast, Optional, Sequence, TextIO, Union
from collections.abc import Mapping

from build.project import Project
from .toolchain import AnyToolchain

def __write_cmake_compiler(f: TextIO, language: str, compiler: str) -> None:
    s = compiler.split(' ', 1)
    if len(s) == 2:
        print(f'set(CMAKE_{language}_COMPILER_LAUNCHER {s[0]})', file=f)
        compiler = s[1]
    print(f'set(CMAKE_{language}_COMPILER {compiler})', file=f)

def __write_cmake_toolchain_file(f: TextIO, toolchain: AnyToolchain) -> None:
    if toolchain.is_darwin:
        cmake_system_name = 'Darwin'
    elif toolchain.is_windows:
        cmake_system_name = 'Windows'
    else:
        cmake_system_name = 'Linux'

    f.write(f"""
set(CMAKE_SYSTEM_NAME {cmake_system_name})
set(CMAKE_SYSTEM_PROCESSOR {toolchain.host_triplet.split('-', 1)[0]})

set(CMAKE_C_COMPILER_TARGET {toolchain.host_triplet})
set(CMAKE_CXX_COMPILER_TARGET {toolchain.host_triplet})

set(CMAKE_C_FLAGS_INIT "{toolchain.cflags} {toolchain.cppflags}")
set(CMAKE_CXX_FLAGS_INIT "{toolchain.cxxflags} {toolchain.cppflags}")
""")
    __write_cmake_compiler(f, 'C', toolchain.cc)
    __write_cmake_compiler(f, 'CXX', toolchain.cxx)

    if cmake_system_name == 'Windows':
            # search libraries and headers only in the sysroot, not on
            # the build host
            f.write(f"""
set(CMAKE_FIND_ROOT_PATH "{toolchain.install_prefix}")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
""")

def configure(toolchain: AnyToolchain, src: str, build: str, args: list[str]=[], env: Optional[Mapping[str, str]]=None) -> None:
    cross_args: list[str] = []

    if toolchain.is_windows:
        cross_args.append('-DCMAKE_RC_COMPILER=' + cast(str, toolchain.windres))

    configure = [
        'cmake',
        src,

        '-DCMAKE_INSTALL_PREFIX=' + toolchain.install_prefix,
        '-DCMAKE_BUILD_TYPE=release',

        '-GNinja',
    ] + cross_args + args

    if toolchain.host_triplet is not None:
        # cross-compiling: write a toolchain file
        os.makedirs(build, exist_ok=True)

        # Several targets need a sysroot to prevent pkg-config from
        # looking for libraries on the build host (TODO: fix this
        # properly); but we must not do that on Android because the NDK
        # has a sysroot already
        if not toolchain.is_android and not toolchain.is_darwin:
            cross_args.append('-DCMAKE_SYSROOT=' + toolchain.install_prefix)

        cmake_toolchain_file = os.path.join(build, 'cmake_toolchain_file')
        with open(cmake_toolchain_file, 'w') as f:
            __write_cmake_toolchain_file(f, toolchain)

        configure.append('-DCMAKE_TOOLCHAIN_FILE=' + cmake_toolchain_file)

    if env is None:
        env = toolchain.env
    else:
        env = {**toolchain.env, **env}

    print(configure)
    subprocess.check_call(configure, env=env, cwd=build)

class CmakeProject(Project):
    def __init__(self, url: Union[str, Sequence[str]], md5: str, installed: str,
                 configure_args: list[str]=[],
                 windows_configure_args: list[str]=[],
                 env: Optional[Mapping[str, str]]=None,
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.windows_configure_args = windows_configure_args
        self.env = env

    def configure(self, toolchain: AnyToolchain) -> str:
        src = self.unpack(toolchain)
        build = self.make_build_path(toolchain)
        configure_args = self.configure_args
        if toolchain.is_windows:
            configure_args = configure_args + self.windows_configure_args
        configure(toolchain, src, build, configure_args, self.env)
        return build

    def _build(self, toolchain: AnyToolchain) -> None:
        build = self.configure(toolchain)
        subprocess.check_call(['ninja', '-v', 'install'],
                              cwd=build, env=toolchain.env)
