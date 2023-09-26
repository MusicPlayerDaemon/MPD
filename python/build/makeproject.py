import subprocess, multiprocessing
from typing import Optional, Sequence, Union

from build.project import Project
from .toolchain import AnyToolchain

class MakeProject(Project):
    def __init__(self, url: Union[str, Sequence[str]], md5: str, installed: str,
                 install_target: str='install',
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.install_target = install_target

    def get_simultaneous_jobs(self) -> int:
        try:
            # use twice as many simultaneous jobs as we have CPU cores
            return multiprocessing.cpu_count() * 2
        except NotImplementedError:
            # default to 12, if multiprocessing.cpu_count() is not implemented
            return 12

    def get_make_args(self, toolchain: AnyToolchain) -> list[str]:
        return ['--quiet', '-j' + str(self.get_simultaneous_jobs())]

    def get_make_install_args(self, toolchain: AnyToolchain) -> list[str]:
        return ['--quiet', self.install_target]

    def make(self, toolchain: AnyToolchain, wd: str, args: list[str]) -> None:
        subprocess.check_call(['make'] + args,
                              cwd=wd, env=toolchain.env)

    def build_make(self, toolchain: AnyToolchain, wd: str, install: bool=True) -> None:
        self.make(toolchain, wd, self.get_make_args(toolchain))
        if install:
            self.make(toolchain, wd, self.get_make_install_args(toolchain))
