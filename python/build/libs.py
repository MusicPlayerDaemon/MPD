from build.project import Project
from build.zlib import ZlibProject
from build.autotools import AutotoolsProject
from build.ffmpeg import FfmpegProject
from build.boost import BoostProject

libogg = AutotoolsProject(
    'http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.xz',
    '5c3a34309d8b98640827e5d0991a4015',
    'lib/libogg.a',
    [
        '--disable-shared', '--enable-static',
    ],
)

libvorbis = AutotoolsProject(
    'http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.xz',
    '28cb28097c07a735d6af56e598e1c90f',
    'lib/libvorbis.a',
    [
        '--disable-shared', '--enable-static',
    ],
)

opus = AutotoolsProject(
    'https://archive.mozilla.org/pub/opus/opus-1.2.1.tar.gz',
    'cfafd339ccd9c5ef8d6ab15d7e1a412c054bf4cb4ecbbbcc78c12ef2def70732',
    'lib/libopus.a',
    [
        '--disable-shared', '--enable-static',
    ],
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
    [
        '--disable-shared', '--enable-static',

        # without this, libid3tag's configure.ac ignores -O* and -f*
        '--disable-debugging',
    ],
    autogen=True,
)

libmad = AutotoolsProject(
    'ftp://ftp.mars.org/pub/mpeg/libmad-0.15.1b.tar.gz',
    '1be543bc30c56fb6bea1d7bf6a64e66c',
    'lib/libmad.a',
    [
        '--disable-shared', '--enable-static',

        # without this, libmad's configure.ac ignores -O* and -f*
        '--disable-debugging',
    ],
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
    'http://ffmpeg.org/releases/ffmpeg-3.4.1.tar.xz',
    '5a77278a63741efa74e26bf197b9bb09ac6381b9757391b922407210f0f991c0',
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
        '--disable-lzo',
        '--disable-faan',
        '--disable-pixelutils',
        '--disable-network',
        '--disable-encoders',
        '--disable-protocols',
        '--disable-devices',
        '--disable-filters',
        '--disable-v4l2_m2m',

        # clang misinterprets the "B0" in hevc_mvs.c as binary
        # literal, which breaks the build; but we don't need that
        # video codec anyway
        '--disable-decoder=hevc',
    ],
)

curl = AutotoolsProject(
    'http://curl.haxx.se/download/curl-7.57.0.tar.xz',
    'f5f6fd3c72b7b8389969f4fb671ed8532fa9b5bb7a5cae7ca89bc1cea45c7878',
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
    'http://downloads.sourceforge.net/project/boost/boost/1.66.0/boost_1_66_0.tar.bz2',
    '5721818253e6a0989583192f96782c4a98eb6204965316df9f5ad75819225ca9',
    'include/boost/version.hpp',
)
