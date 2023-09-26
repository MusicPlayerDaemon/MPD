import os.path, subprocess, sys
from typing import Collection, Iterable, Optional, Sequence, Union
from collections.abc import Mapping

from build.makeproject import MakeProject
from .toolchain import AnyToolchain

class AutotoolsProject(MakeProject):
    def __init__(self, url: Union[str, Sequence[str]], md5: str, installed: str,
                 configure_args: Iterable[str]=[],
                 autogen: bool=False,
                 autoreconf: bool=False,
                 per_arch_cflags: Optional[Mapping[str, str]]=None,
                 cppflags: str='',
                 ldflags: str='',
                 libs: str='',
                 subdirs: Optional[Collection[str]]=None,
                 **kwargs):
        MakeProject.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.autogen = autogen
        self.autoreconf = autoreconf
        self.per_arch_cflags = per_arch_cflags
        self.cppflags = cppflags
        self.ldflags = ldflags
        self.libs = libs
        self.subdirs = subdirs

    def configure(self, toolchain: AnyToolchain) -> str:
        src = self.unpack(toolchain)
        if self.autogen:
            if sys.platform == 'darwin':
                subprocess.check_call(['glibtoolize', '--force'], cwd=src)
            else:
                subprocess.check_call(['libtoolize', '--force'], cwd=src)
            subprocess.check_call(['aclocal'], cwd=src)
            subprocess.check_call(['automake', '--add-missing', '--force-missing', '--foreign'], cwd=src)
            subprocess.check_call(['autoconf'], cwd=src)
        if self.autoreconf:
            subprocess.check_call(['autoreconf', '-vif'], cwd=src)

        build = self.make_build_path(toolchain)

        arch_cflags = ''
        if self.per_arch_cflags is not None and toolchain.host_triplet is not None:
            arch_cflags = self.per_arch_cflags.get(toolchain.host_triplet, '')

        configure = [
            os.path.join(src, 'configure'),
            'CC=' + toolchain.cc,
            'CXX=' + toolchain.cxx,
            'CFLAGS=' + toolchain.cflags + ' ' + arch_cflags,
            'CXXFLAGS=' + toolchain.cxxflags + ' ' + arch_cflags,
            'CPPFLAGS=' + toolchain.cppflags + ' ' + self.cppflags,
            'LDFLAGS=' + toolchain.ldflags + ' ' + self.ldflags,
            'LIBS=' + toolchain.libs + ' ' + self.libs,
            'AR=' + toolchain.ar,
            'ARFLAGS=' + toolchain.arflags,
            'RANLIB=' + toolchain.ranlib,
            'STRIP=' + toolchain.strip,
            '--prefix=' + toolchain.install_prefix,
            '--disable-silent-rules',
        ]

        if toolchain.host_triplet is not None:
            configure.append('--host=' + toolchain.host_triplet)

        configure.extend(self.configure_args)

        try:
            print(configure)
            subprocess.check_call(configure, cwd=build, env=toolchain.env)
        except subprocess.CalledProcessError:
            # dump config.log after a failed configure run
            try:
                with open(os.path.join(build, 'config.log')) as f:
                    sys.stdout.write(f.read())
            except:
                pass
            # re-raise the exception
            raise

        return build

    def _build(self, toolchain: AnyToolchain) -> None:
        build = self.configure(toolchain)
        if self.subdirs is not None:
            for subdir in self.subdirs:
                self.build_make(toolchain, os.path.join(build, subdir))
        else:
            self.build_make(toolchain, build)
