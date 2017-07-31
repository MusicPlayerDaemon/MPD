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
    'http://downloads.xiph.org/releases/opus/opus-1.1.4.tar.gz',
    '9122b6b380081dd2665189f97bfd777f04f92dc3ab6698eea1dbb27ad59d8692',
    'lib/libopus.a',
    ['--disable-shared', '--enable-static'],
)

flac = AutotoolsProject(
    'http://downloads.xiph.org/releases/flac/flac-1.3.2.tar.xz',
    '91cfc3ed61dc40f47f050a109b08610667d73477af6ef36dcad31c31a4a8d53f',
    'lib/libFLAC.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-xmms-plugin', '--disable-cpplibs',
    ],
)

zlib = ZlibProject(
    'http://zlib.net/zlib-1.2.11.tar.xz',
    '4ff941449631ace0d4d203e3483be9dbc9da454084111f97ea0a2114e19bf066',
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

liblame = AutotoolsProject(
    'http://downloads.sourceforge.net/project/lame/lame/3.99/lame-3.99.5.tar.gz',
    '24346b4158e4af3bd9f2e194bb23eb473c75fb7377011523353196b19b9a23ff',
    'lib/libmp3lame.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-gtktest', '--disable-analyzer-hooks',
        '--disable-decoder', '--disable-frontend',
    ],
)

ffmpeg = FfmpegProject(
    'http://ffmpeg.org/releases/ffmpeg-3.3.2.tar.xz',
    '1998de1ab32616cbf2ff86efc3f1f26e76805ec5dc51e24c041c79edd8262785',
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
    'http://curl.haxx.se/download/curl-7.54.1.tar.lzma',
    '2b7af34d4900887e0b4e0a9f545b9511ff774d07151ae4976485060d3e1bdb6e',
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
    'http://downloads.sourceforge.net/project/boost/boost/1.64.0/boost_1_64_0.tar.bz2',
    '7bcc5caace97baa948931d712ea5f37038dbb1c5d89b43ad4def4ed7cb683332',
    'include/boost/version.hpp',
)
