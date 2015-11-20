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

    def build(self, toolchain):
        src = self.unpack(toolchain)

        # install the headers manually; don't build any library
        # (because right now, we only use header-only libraries)
        includedir = os.path.join(toolchain.install_prefix, 'include')
        for dirpath, dirnames, filenames in os.walk(os.path.join(src, 'boost')):
            relpath = dirpath[len(src)+1:]
            destdir = os.path.join(includedir, relpath)
            try:
                os.mkdir(destdir)
            except:
                pass
            for name in filenames:
                if name[-4:] == '.hpp':
                    shutil.copyfile(os.path.join(dirpath, name),
                                    os.path.join(destdir, name))
