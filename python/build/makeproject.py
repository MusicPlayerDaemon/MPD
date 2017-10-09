import subprocess, multiprocessing

from build.project import Project

class MakeProject(Project):
    def __init__(self, url, md5, installed,
                 install_target='install',
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.install_target = install_target

    def get_simultaneous_jobs(self):
        try:
            # use twice as many simultaneous jobs as we have CPU cores
            return multiprocessing.cpu_count() * 2
        except NotImplementedError:
            # default to 12, if multiprocessing.cpu_count() is not implemented
            return 12

    def get_make_args(self, toolchain):
        return ['--quiet', '-j' + str(self.get_simultaneous_jobs())]

    def get_make_install_args(self, toolchain):
        return ['--quiet', self.install_target]

    def make(self, toolchain, wd, args):
        subprocess.check_call(['make'] + args,
                              cwd=wd, env=toolchain.env)

    def build_make(self, toolchain, wd, install=True):
        self.make(toolchain, wd, self.get_make_args(toolchain))
        if install:
            self.make(toolchain, wd, self.get_make_install_args(toolchain))
