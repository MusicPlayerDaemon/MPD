from build.project import Project
from build.zlib import ZlibProject
from build.autotools import AutotoolsProject
from build.ffmpeg import FfmpegProject
from build.boost import BoostProject

libogg = AutotoolsProject(
    'http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.xz',
    '5c3a34309d8b98640827e5d0991a4015',
    'lib/libogg.a',
    ['--disable-shared', '--enable-static'],
)

libvorbis = AutotoolsProject(
    'http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.xz',
    '28cb28097c07a735d6af56e598e1c90f',
    'lib/libvorbis.a',
    ['--disable-shared', '--enable-static'],
)

opus = AutotoolsProject(
    'http://downloads.xiph.org/releases/opus/opus-1.1.2.tar.gz',
    '1f08a661bc72930187893a07f3741a91',
    'lib/libopus.a',
    ['--disable-shared', '--enable-static'],
)

flac = AutotoolsProject(
    'http://downloads.xiph.org/releases/flac/flac-1.3.1.tar.xz',
    'b9922c9a0378c88d3e901b234f852698',
    'lib/libFLAC.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-xmms-plugin', '--disable-cpplibs',
    ],
)

zlib = ZlibProject(
    'http://zlib.net/zlib-1.2.8.tar.xz',
    '28f1205d8dd2001f26fec1e8c2cebe37',
    'lib/libz.a',
)

libid3tag = AutotoolsProject(
    'ftp://ftp.mars.org/pub/mpeg/libid3tag-0.15.1b.tar.gz',
    'e5808ad997ba32c498803822078748c3',
    'lib/libid3tag.a',
    ['--disable-shared', '--enable-static'],
    autogen=True,
)

libmad = AutotoolsProject(
    'ftp://ftp.mars.org/pub/mpeg/libmad-0.15.1b.tar.gz',
    '1be543bc30c56fb6bea1d7bf6a64e66c',
    'lib/libmad.a',
    ['--disable-shared', '--enable-static'],
    autogen=True,
)

ffmpeg = FfmpegProject(
    'http://ffmpeg.org/releases/ffmpeg-3.0.2.tar.xz',
    'beb5c69c671aba1386e7156fc2af1ab6',
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
)

curl = AutotoolsProject(
    'http://curl.haxx.se/download/curl-7.48.0.tar.lzma',
    'de75665721e3adb0a1d7e03b90c3d7a2',
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
)

boost = BoostProject(
    'http://downloads.sourceforge.net/boost/boost_1_61_0.tar.bz2',
    '6095876341956f65f9d35939ccea1a9f',
    'include/boost/version.hpp',
)
