#!/usr/bin/env python3

import os, os.path
import sys, shutil, subprocess
import urllib.request
import hashlib
import re

configure_args = sys.argv[1:]

host_arch = 'i686-w64-mingw32'

if len(configure_args) > 0 and configure_args[0] == '--64':
    configure_args = configure_args[1:]
    host_arch = 'x86_64-w64-mingw32'

# the path to the MPD sources
mpd_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))

# output directories
lib_path = os.path.abspath('lib')

shared_path = lib_path
if 'MPD_SHARED_LIB' in os.environ:
    shared_path = os.environ['MPD_SHARED_LIB']
tarball_path = os.path.join(shared_path, 'download')
src_path = os.path.join(shared_path, 'src')

arch_path = os.path.join(lib_path, host_arch)
build_path = os.path.join(arch_path, 'build')
root_path = os.path.join(arch_path, 'root')

class CrossGccToolchain:
    def __init__(self, toolchain_path, arch, install_prefix):
        self.arch = arch
        self.install_prefix = install_prefix

        toolchain_bin = os.path.join(toolchain_path, 'bin')
        self.cc = os.path.join(toolchain_bin, arch + '-gcc')
        self.cxx = os.path.join(toolchain_bin, arch + '-g++')
        self.ar = os.path.join(toolchain_bin, arch + '-ar')
        self.nm = os.path.join(toolchain_bin, arch + '-nm')
        self.strip = os.path.join(toolchain_bin, arch + '-strip')

        common_flags = ''
        self.cflags = '-O2 -g ' + common_flags
        self.cxxflags = '-O2 -g ' + common_flags
        self.cppflags = '-isystem ' + os.path.join(install_prefix, 'include')
        self.ldflags = '-L' + os.path.join(install_prefix, 'lib')
        self.libs = ''

        self.env = dict(os.environ)

        # redirect pkg-config to use our root directory instead of the
        # default one on the build host
        self.env['PKG_CONFIG_LIBDIR'] = os.path.join(install_prefix, 'lib/pkgconfig')

def file_md5(path):
    """Calculate the MD5 checksum of a file and return it in hexadecimal notation."""

    with open(path, 'rb') as f:
        m = hashlib.md5()
        while True:
            data = f.read(65536)
            if len(data) == 0:
                # end of file
                return m.hexdigest()
            m.update(data)

def download_tarball(url, md5):
    """Download a tarball, verify its MD5 checksum and return the local path."""

    global tarball_path
    os.makedirs(tarball_path, exist_ok=True)
    path = os.path.join(tarball_path, os.path.basename(url))

    try:
        calculated_md5 = file_md5(path)
        if md5 == calculated_md5: return path
        os.unlink(path)
    except FileNotFoundError:
        pass

    tmp_path = path + '.tmp'

    print("download", url)
    urllib.request.urlretrieve(url, tmp_path)
    calculated_md5 = file_md5(tmp_path)
    if calculated_md5 != md5:
        os.unlink(tmp_path)
        raise "MD5 mismatch"

    os.rename(tmp_path, path)
    return path

class Project:
    def __init__(self, url, md5, installed, name=None, version=None,
                 base=None):
        if base is None:
            basename = os.path.basename(url)
            m = re.match(r'^(.+)\.(tar(\.(gz|bz2|xz|lzma))?|zip)$', basename)
            if not m: raise
            self.base = m.group(1)
        else:
            self.base = base

        if name is None or version is None:
            m = re.match(r'^([-\w]+)-(\d[\d.]*[a-z]?)$', self.base)
            if name is None: name = m.group(1)
            if version is None: version = m.group(2)

        self.name = name
        self.version = version

        self.url = url
        self.md5 = md5
        self.installed = installed

    def download(self):
        return download_tarball(self.url, self.md5)

    def is_installed(self, toolchain):
        tarball = self.download()
        installed = os.path.join(toolchain.install_prefix, self.installed)
        tarball_mtime = os.path.getmtime(tarball)
        try:
            return os.path.getmtime(installed) >= tarball_mtime
        except FileNotFoundError:
            return False

    def unpack(self, out_of_tree=True):
        global src_path, build_path
        tarball = self.download()
        if out_of_tree:
            parent_path = src_path
        else:
            parent_path = build_path
        path = os.path.join(parent_path, self.base)
        try:
            shutil.rmtree(path)
        except FileNotFoundError:
            pass
        os.makedirs(parent_path, exist_ok=True)
        subprocess.check_call(['/bin/tar', 'xfC', tarball, parent_path])
        return path

    def make_build_path(self):
        path = os.path.join(build_path, self.base)
        try:
            shutil.rmtree(path)
        except FileNotFoundError:
            pass
        os.makedirs(path, exist_ok=True)
        return path

class AutotoolsProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 autogen=False,
                 cppflags='',
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.autogen = autogen
        self.cppflags = cppflags

    def build(self, toolchain):
        src = self.unpack()
        if self.autogen:
            subprocess.check_call(['/usr/bin/aclocal'], cwd=src)
            subprocess.check_call(['/usr/bin/automake', '--add-missing', '--force-missing', '--foreign'], cwd=src)
            subprocess.check_call(['/usr/bin/autoconf'], cwd=src)
            subprocess.check_call(['/usr/bin/libtoolize', '--force'], cwd=src)

        build = self.make_build_path()

        configure = [
            os.path.join(src, 'configure'),
            'CC=' + toolchain.cc,
            'CXX=' + toolchain.cxx,
            'CFLAGS=' + toolchain.cflags,
            'CXXFLAGS=' + toolchain.cxxflags,
            'CPPFLAGS=' + toolchain.cppflags + ' ' + self.cppflags,
            'LDFLAGS=' + toolchain.ldflags,
            'LIBS=' + toolchain.libs,
            'AR=' + toolchain.ar,
            'STRIP=' + toolchain.strip,
            '--host=' + toolchain.arch,
            '--prefix=' + toolchain.install_prefix,
            '--enable-silent-rules',
        ] + self.configure_args

        subprocess.check_call(configure, cwd=build, env=toolchain.env)
        subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'],
                              cwd=build, env=toolchain.env)
        subprocess.check_call(['/usr/bin/make', '--quiet', 'install'],
                              cwd=build, env=toolchain.env)

class ZlibProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)

    def build(self, toolchain):
        src = self.unpack(out_of_tree=False)

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

class FfmpegProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 cppflags='',
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.cppflags = cppflags

    def build(self, toolchain):
        src = self.unpack()
        build = self.make_build_path()

        configure = [
            os.path.join(src, 'configure'),
            '--cc=' + toolchain.cc,
            '--cxx=' + toolchain.cxx,
            '--nm=' + toolchain.nm,
            '--extra-cflags=' + toolchain.cflags + ' ' + toolchain.cppflags + ' ' + self.cppflags,
            '--extra-cxxflags=' + toolchain.cxxflags + ' ' + toolchain.cppflags + ' ' + self.cppflags,
            '--extra-ldflags=' + toolchain.ldflags,
            '--extra-libs=' + toolchain.libs,
            '--ar=' + toolchain.ar,
            '--enable-cross-compile',
            '--arch=x86',
            '--target-os=mingw32',
            '--cross-prefix=' + toolchain.arch + '-',
            '--prefix=' + toolchain.install_prefix,
        ] + self.configure_args

        subprocess.check_call(configure, cwd=build, env=toolchain.env)
        subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'],
                              cwd=build, env=toolchain.env)
        subprocess.check_call(['/usr/bin/make', '--quiet', 'install'],
                              cwd=build, env=toolchain.env)

class BoostProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        m = re.match(r'.*/boost_(\d+)_(\d+)_(\d+)\.tar\.bz2$', url)
        version = "%s.%s.%s" % (m.group(1), m.group(2), m.group(3))
        Project.__init__(self, url, md5, installed,
                         name='boost', version=version,
                         **kwargs)

    def build(self, toolchain):
        src = self.unpack()

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

# a list of third-party libraries to be used by MPD on Android
thirdparty_libs = [
    AutotoolsProject(
        'http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.xz',
        '5c3a34309d8b98640827e5d0991a4015',
        'lib/libogg.a',
        ['--disable-shared', '--enable-static'],
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.xz',
        '28cb28097c07a735d6af56e598e1c90f',
        'lib/libvorbis.a',
        ['--disable-shared', '--enable-static'],
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz',
        'c5a8cf7c0b066759542bc4ca46817ac6',
        'lib/libopus.a',
        ['--disable-shared', '--enable-static'],
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/flac/flac-1.3.1.tar.xz',
        'b9922c9a0378c88d3e901b234f852698',
        'lib/libFLAC.a',
        [
            '--disable-shared', '--enable-static',
            '--disable-xmms-plugin', '--disable-cpplibs',
        ],
    ),

    ZlibProject(
        'http://zlib.net/zlib-1.2.8.tar.xz',
        '28f1205d8dd2001f26fec1e8c2cebe37',
        'lib/libz.a',
    ),

    AutotoolsProject(
        'ftp://ftp.mars.org/pub/mpeg/libid3tag-0.15.1b.tar.gz',
        'e5808ad997ba32c498803822078748c3',
        'lib/libid3tag.a',
        ['--disable-shared', '--enable-static'],
        autogen=True,
    ),

    FfmpegProject(
        'http://ffmpeg.org/releases/ffmpeg-2.8.2.tar.xz',
        '5041ffe661392b0685d2248114791fde',
        'lib/libavcodec.a',
        [
            '--disable-shared', '--enable-static',
            '--enable-gpl',
            '--enable-small',
            '--disable-pthreads',
            '--disable-programs',
            '--disable-doc',
            '--disable-avdevice',
            '--disable-swresample',
            '--disable-swscale',
            '--disable-postproc',
            '--disable-avfilter',
            '--disable-network',
            '--disable-encoders',
            '--disable-protocols',
            '--disable-outdevs',
            '--disable-filters',
        ],
    ),

    AutotoolsProject(
        'http://curl.haxx.se/download/curl-7.45.0.tar.lzma',
        'c9a0a77f71fdc6b0f925bc3e79eb77f6',
        'lib/libcurl.a',
        [
            '--disable-shared', '--enable-static',
            '--disable-debug',
            '--enable-http',
            '--enable-ipv6',
            '--disable-ftp', '--disable-file',
            '--disable-ldap', '--disable-ldaps',
            '--disable-rtsp', '--disable-proxy', '--disable-dict', '--disable-telnet',
            '--disable-tftp', '--disable-pop3', '--disable-imap', '--disable-smtp',
            '--disable-gopher',
            '--disable-manual',
            '--disable-threaded-resolver', '--disable-verbose', '--disable-sspi',
            '--disable-crypto-auth', '--disable-ntlm-wb', '--disable-tls-srp', '--disable-cookies',
            '--without-ssl', '--without-gnutls', '--without-nss', '--without-libssh2',
        ],
    ),

    BoostProject(
        'http://netcologne.dl.sourceforge.net/project/boost/boost/1.59.0/boost_1_59_0.tar.bz2',
        '6aa9a5c6a4ca1016edd0ed1178e3cb87',
        'include/boost/version.hpp',
    ),
]

# build the third-party libraries
toolchain = CrossGccToolchain('/usr', host_arch, root_path)

for x in thirdparty_libs:
    if not x.is_installed(toolchain):
        x.build(toolchain)

# configure and build MPD

configure = [
    os.path.join(mpd_path, 'configure'),
    'CC=' + toolchain.cc,
    'CXX=' + toolchain.cxx,
    'CFLAGS=' + toolchain.cflags,
    'CXXFLAGS=' + toolchain.cxxflags,
    'CPPFLAGS=' + toolchain.cppflags,
    'LDFLAGS=' + toolchain.ldflags + ' -static',
    'LIBS=' + toolchain.libs,
    'AR=' + toolchain.ar,
    'STRIP=' + toolchain.strip,
    '--host=' + toolchain.arch,
    '--prefix=' + toolchain.install_prefix,

    '--enable-silent-rules',

    '--disable-icu',

] + configure_args

subprocess.check_call(configure, env=toolchain.env)
subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], env=toolchain.env)
