import os.path, subprocess, sys

from build.makeproject import MakeProject

class AutotoolsProject(MakeProject):
    def __init__(self, url, md5, installed, configure_args=[],
                 autogen=False,
                 autoreconf=False,
                 cppflags='',
                 ldflags='',
                 libs='',
                 subdirs=None,
                 **kwargs):
        MakeProject.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.autogen = autogen
        self.autoreconf = autoreconf
        self.cppflags = cppflags
        self.ldflags = ldflags
        self.libs = libs
        self.subdirs = subdirs

    def configure(self, toolchain):
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

        configure = [
            os.path.join(src, 'configure'),
            'CC=' + toolchain.cc,
            'CXX=' + toolchain.cxx,
            'CFLAGS=' + toolchain.cflags,
            'CXXFLAGS=' + toolchain.cxxflags,
            'CPPFLAGS=' + toolchain.cppflags + ' ' + self.cppflags,
            'LDFLAGS=' + toolchain.ldflags + ' ' + self.ldflags,
            'LIBS=' + toolchain.libs + ' ' + self.libs,
            'AR=' + toolchain.ar,
            'ARFLAGS=' + toolchain.arflags,
            'RANLIB=' + toolchain.ranlib,
            'STRIP=' + toolchain.strip,
            '--host=' + toolchain.arch,
            '--prefix=' + toolchain.install_prefix,
            '--disable-silent-rules',
        ] + self.configure_args

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

    def _build(self, toolchain):
        build = self.configure(toolchain)
        if self.subdirs is not None:
            for subdir in self.subdirs:
                self.build_make(toolchain, os.path.join(build, subdir))
        else:
            self.build_make(toolchain, build)
