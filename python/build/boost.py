import os, shutil
import re

from build.project import Project

class BoostProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        m = re.match(r'.*/boost_(\d+)_(\d+)_(\d+)\.tar\.bz2$', url)
        version = "%s.%s.%s" % (m.group(1), m.group(2), m.group(3))
        Project.__init__(self, url, md5, installed,
                         name='boost', version=version,
                         **kwargs)

    def _build(self, toolchain):
        src = self.unpack(toolchain)

        # install the headers manually; don't build any library
        # (because right now, we only use header-only libraries)
        includedir = os.path.join(toolchain.install_prefix, 'include')
        dest = os.path.join(includedir, 'boost')
        shutil.rmtree(dest, ignore_errors=True)
        shutil.copytree(os.path.join(src, 'boost'), dest)

        # touch the boost/version.hpp file to ensure it's newer than
        # the downloaded Boost tarball, to avoid reinstalling Boost on
        # every run
        os.utime(os.path.join(toolchain.install_prefix, self.installed))
