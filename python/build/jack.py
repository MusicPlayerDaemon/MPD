import os, shutil
import re

from .project import Project

# This class installs just the public headers and a fake pkg-config
# file which defines the macro "DYNAMIC_JACK".  This tells MPD's JACK
# output plugin to load the libjack64.dll dynamically using
# LoadLibrary().  This kludge avoids the runtime DLL dependency for
# users who don't use JACK, but still allows using the system JACK
# client library.
#
# The problem with JACK is that it uses an extremely fragile shared
# memory protocol to communicate with the daemon.  One needs to use
# daemon and client library from the same build.  That's why we don't
# build libjack statically here; it would probably not be compatible
# with the user's JACK daemon.

class JackProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        m = re.match(r'.*/v([\d.]+)\.tar\.gz$', url)
        self.version = m.group(1)
        Project.__init__(self, url, md5, installed,
                         name='jack2', version=self.version,
                         base='jack2-' + self.version,
                         **kwargs)

    def _build(self, toolchain):
        src = self.unpack(toolchain)

        includes = ['jack.h', 'ringbuffer.h', 'systemdeps.h', 'transport.h', 'types.h', 'weakmacros.h']
        includedir = os.path.join(toolchain.install_prefix, 'include', 'jack')
        os.makedirs(includedir, exist_ok=True)

        for i in includes:
            shutil.copyfile(os.path.join(src, 'common', 'jack', i),
                            os.path.join(includedir, i))

        with open(os.path.join(toolchain.install_prefix, 'lib', 'pkgconfig', 'jack.pc'), 'w') as f:
            print("prefix=" + toolchain.install_prefix, file=f)
            print("", file=f)
            print("Name: jack", file=f)
            print("Description: dummy", file=f)
            print("Version: " + self.version, file=f)
            print("Libs: ", file=f)
            print("Cflags: -DDYNAMIC_JACK", file=f)
