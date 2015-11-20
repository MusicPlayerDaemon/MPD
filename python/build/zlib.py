import subprocess

from build.project import Project

class ZlibProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)

    def build(self, toolchain):
        src = self.unpack(toolchain, out_of_tree=False)

        subprocess.check_call(['/usr/bin/make', '--quiet',
            '-f', 'win32/Makefile.gcc',
            'PREFIX=' + toolchain.arch + '-',
            '-j12',
            'install',
            'DESTDIR=' + toolchain.install_prefix + '/',
            'INCLUDE_PATH=include',
            'LIBRARY_PATH=lib',
            'BINARY_PATH=bin', 'SHARED_MODE=1'],
            cwd=src, env=toolchain.env)
