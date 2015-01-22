#!/usr/bin/env python3

import os, os.path
import sys, shutil, subprocess
import urllib.request
import hashlib
import re

if len(sys.argv) < 3:
    print("Usage: build.py SDK_PATH NDK_PATH [configure_args...]", file=sys.stderr)
    sys.exit(1)

sdk_path = sys.argv[1]
ndk_path = sys.argv[2]
configure_args = sys.argv[3:]

if not os.path.isfile(os.path.join(sdk_path, 'tools', 'android')):
    print("SDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

if not os.path.isdir(ndk_path):
    print("NDK not found in", ndk_path, file=sys.stderr)
    sys.exit(1)

# the path to the MPD sources
mpd_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))

# output directories
lib_path = os.path.abspath('lib')
tarball_path = lib_path
src_path = os.path.join(lib_path, 'src')
build_path = os.path.join(lib_path, 'build')
root_path = os.path.join(lib_path, 'root')

# build host configuration
build_arch = 'linux-x86_64'

# redirect pkg-config to use our root directory instead of the default
# one on the build host
os.environ['PKG_CONFIG_LIBDIR'] = os.path.join(root_path, 'lib/pkgconfig')

# select the NDK compiler
gcc_version = '4.9'
llvm_version = '3.5'

# select the NDK target
ndk_arch = 'arm'
host_arch = 'arm-linux-androideabi'
android_abi = 'armeabi-v7a'
ndk_platform = 'android-14'

# set up the NDK toolchain

gcc_toolchain = os.path.join(ndk_path, 'toolchains', host_arch + '-' + gcc_version, 'prebuilt', build_arch)
llvm_toolchain = os.path.join(ndk_path, 'toolchains', 'llvm-' + llvm_version, 'prebuilt', build_arch)
ndk_platform_path = os.path.join(ndk_path, 'platforms', ndk_platform)
target_root = os.path.join(ndk_platform_path, 'arch-' + ndk_arch)

llvm_triple = 'armv7-none-linux-androideabi'

def select_toolchain(use_cxx, use_clang):
    global cc, cxx, ar, strip, cflags, cxxflags, cppflags, ldflags, libs

    target_arch = '-march=armv7-a -mfloat-abi=softfp'
    if use_clang:
        cc = os.path.join(llvm_toolchain, 'bin/clang')
        cxx = os.path.join(llvm_toolchain, 'bin/clang++')
        target_arch += ' -target ' + llvm_triple + ' -integrated-as -gcc-toolchain ' + gcc_toolchain
    else:
        cc = os.path.join(gcc_toolchain, 'bin', host_arch + '-gcc')
        cxx = os.path.join(gcc_toolchain, 'bin', host_arch + '-g++')
    ar = os.path.join(gcc_toolchain, 'bin', host_arch + '-ar')
    strip = os.path.join(gcc_toolchain, 'bin', host_arch + '-strip')

    libstdcxx_path = os.path.join(ndk_path, 'sources/cxx-stl/gnu-libstdc++', gcc_version)
    libstdcxx_cppflags = '-isystem ' + os.path.join(libstdcxx_path, 'include') + ' -isystem ' + os.path.join(libstdcxx_path, 'libs', android_abi, 'include')
    if use_clang:
        libstdcxx_cppflags += ' -D__STRICT_ANSI__'
    libstdcxx_ldadd = os.path.join(libstdcxx_path, 'libs', android_abi, 'libgnustl_static.a')

    cflags = '-Os -g ' + target_arch
    cxxflags = '-Os -g ' + target_arch
    cppflags = '--sysroot=' + target_root + ' -I' + root_path + '/include'
    ldflags = '--sysroot=' + target_root + ' -L' + root_path + '/lib'
    libs = ''

    if use_cxx:
        libs += ' ' + libstdcxx_ldadd
        cppflags += ' ' + libstdcxx_cppflags

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
                 base=None,
                 use_cxx=False, use_clang=False):
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

        self.use_cxx = use_cxx
        self.use_clang = use_clang

    def download(self):
        return download_tarball(self.url, self.md5)

    def is_installed(self):
        global root_path
        tarball = self.download()
        installed = os.path.join(root_path, self.installed)
        tarball_mtime = os.path.getmtime(tarball)
        try:
            return os.path.getmtime(installed) >= tarball_mtime
        except FileNotFoundError:
            return False

    def unpack(self):
        global src_path
        tarball = self.download()
        path = os.path.join(src_path, self.base)
        try:
            shutil.rmtree(path)
        except FileNotFoundError:
            pass
        os.makedirs(src_path, exist_ok=True)
        subprocess.check_call(['/bin/tar', 'xfC', tarball, src_path])
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

    def build(self):
        src = self.unpack()
        if self.autogen:
            subprocess.check_call(['/usr/bin/aclocal'], cwd=src)
            subprocess.check_call(['/usr/bin/automake', '--add-missing', '--force-missing', '--foreign'], cwd=src)
            subprocess.check_call(['/usr/bin/autoconf'], cwd=src)
            subprocess.check_call(['/usr/bin/libtoolize', '--force'], cwd=src)

        build = self.make_build_path()

        select_toolchain(use_cxx=self.use_cxx, use_clang=self.use_clang)
        configure = [
            os.path.join(src, 'configure'),
            'CC=' + cc,
            'CXX=' + cxx,
            'CFLAGS=' + cflags,
            'CXXFLAGS=' + cxxflags,
            'CPPFLAGS=' + cppflags + ' ' + self.cppflags,
            'LDFLAGS=' + ldflags,
            'LIBS=' + libs,
            'AR=' + ar,
            'STRIP=' + strip,
            '--host=' + host_arch,
            '--prefix=' + root_path,
            '--with-sysroot=' + target_root,
            '--enable-silent-rules',
        ] + self.configure_args

        subprocess.check_call(configure, cwd=build)
        subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], cwd=build)
        subprocess.check_call(['/usr/bin/make', '--quiet', 'install'], cwd=build)

class FfmpegProject(Project):
    def __init__(self, url, md5, installed, configure_args=[],
                 cppflags='',
                 **kwargs):
        Project.__init__(self, url, md5, installed, **kwargs)
        self.configure_args = configure_args
        self.cppflags = cppflags

    def build(self):
        src = self.unpack()
        build = self.make_build_path()

        select_toolchain(use_cxx=self.use_cxx, use_clang=self.use_clang)
        configure = [
            os.path.join(src, 'configure'),
            '--cc=' + cc,
            '--cxx=' + cxx,
            '--extra-cflags=' + cflags + ' ' + cppflags + ' ' + self.cppflags,
            '--extra-cxxflags=' + cxxflags + ' ' + cppflags + ' ' + self.cppflags,
            '--extra-ldflags=' + ldflags,
            '--extra-libs=' + libs,
            '--ar=' + ar,
            '--enable-cross-compile',
            '--target-os=linux',
            '--arch=' + ndk_arch,
            '--cpu=cortex-a8',
            '--prefix=' + root_path,
        ] + self.configure_args

        subprocess.check_call(configure, cwd=build)
        subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'], cwd=build)
        subprocess.check_call(['/usr/bin/make', '--quiet', 'install'], cwd=build)

class BoostProject(Project):
    def __init__(self, url, md5, installed,
                 **kwargs):
        m = re.match(r'.*/boost_(\d+)_(\d+)_(\d+)\.tar\.bz2$', url)
        version = "%s.%s.%s" % (m.group(1), m.group(2), m.group(3))
        Project.__init__(self, url, md5, installed,
                         name='boost', version=version,
                         **kwargs)

    def build(self):
        src = self.unpack()

        # install the headers manually; don't build any library
        # (because right now, we only use header-only libraries)
        includedir = os.path.join(root_path, 'include')
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
        'http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.4.tar.xz',
        '55f2288055e44754275a17c9a2497391',
        'lib/libvorbis.a',
        ['--disable-shared', '--enable-static'],
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz',
        'c5a8cf7c0b066759542bc4ca46817ac6',
        'lib/libopus.a',
        ['--disable-shared', '--enable-static'],
        use_clang=True,
    ),

    AutotoolsProject(
        'http://downloads.xiph.org/releases/flac/flac-1.3.1.tar.xz',
        'b9922c9a0378c88d3e901b234f852698',
        'lib/libFLAC.a',
        [
            '--disable-shared', '--enable-static',
            '--disable-xmms-plugin', '--disable-cpplibs',
        ],
        use_clang=True,
    ),

    AutotoolsProject(
        'ftp://ftp.mars.org/pub/mpeg/libid3tag-0.15.1b.tar.gz',
        'e5808ad997ba32c498803822078748c3',
        'lib/libid3tag.a',
        ['--disable-shared', '--enable-static'],
        autogen=True,
    ),

    AutotoolsProject(
        'ftp://ftp.mars.org/pub/mpeg/libmad-0.15.1b.tar.gz',
        '1be543bc30c56fb6bea1d7bf6a64e66c',
        'lib/libmad.a',
        ['--disable-shared', '--enable-static'],
        autogen=True,
    ),

    FfmpegProject(
        'http://ffmpeg.org/releases/ffmpeg-2.5.tar.bz2',
        '4346fe710cc6bdd981f6534d2420d1ab',
        'lib/libavcodec.a',
        [
            '--disable-shared', '--enable-static',
            '--enable-gpl',
            '--enable-small',
            '--disable-pthreads',
            '--disable-runtime-cpudetect',
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
        'http://curl.haxx.se/download/curl-7.39.0.tar.lzma',
        'e9aa6dec29920eba8ef706ea5823bad7',
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
        use_clang=True,
    ),

    BoostProject(
        'http://netcologne.dl.sourceforge.net/project/boost/boost/1.55.0/boost_1_55_0.tar.bz2',
        'd6eef4b4cacb2183f2bf265a5a03a354',
        'include/boost/version.hpp',
    ),
]

# build the third-party libraries
for x in thirdparty_libs:
    if not x.is_installed():
        x.build()

# configure and build MPD
select_toolchain(use_cxx=True, use_clang=True)

configure = [
    os.path.join(mpd_path, 'configure'),
    'CC=' + cc,
    'CXX=' + cxx,
    'CFLAGS=' + cflags,
    'CXXFLAGS=' + cxxflags,
    'CPPFLAGS=' + cppflags,
    'LDFLAGS=' + ldflags,
    'LIBS=' + libs,
    'AR=' + ar,
    'STRIP=' + strip,
    '--host=' + host_arch,
    '--prefix=' + root_path,
    '--with-sysroot=' + target_root,
    '--with-android-sdk=' + sdk_path,

    '--enable-silent-rules',

    '--disable-glib',
    '--disable-icu',

    # disabled for now because these features require GLib:
    '--disable-httpd-output',
    '--disable-vorbis-encoder',

] + configure_args

subprocess.check_call(configure)
subprocess.check_call(['/usr/bin/make', '--quiet', '-j12'])
