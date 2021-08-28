import os.path, subprocess

from build.project import Project

class ZlibProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)

    def _build(self, toolchain):
        src = self.unpack(toolchain, out_of_tree=False)

        subprocess.check_call(['/usr/bin/make', '--quiet',
            '-f', 'win32/Makefile.gcc',
            'PREFIX=' + toolchain.arch + '-',
            '-j12',
            'install',
            'INCLUDE_PATH='+ os.path.join(toolchain.install_prefix, 'include'),
            'LIBRARY_PATH=' + os.path.join(toolchain.install_prefix, 'lib'),
            'BINARY_PATH=' + os.path.join(toolchain.install_prefix, 'bin'),
            ],
            cwd=src, env=toolchain.env)
