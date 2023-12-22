import os.path
import platform
import shutil
from typing import Union

def with_ccache(command: str) -> str:
    ccache = shutil.which('ccache')
    if ccache is None:
        return command
    else:
        return f'{ccache} {command}'

android_abis = {
    'armeabi-v7a': {
        'arch': 'armv7a-linux-androideabi',
        'ndk_arch': 'arm',
        'cflags': '-fpic -mfpu=neon -mfloat-abi=softfp',
    },

    'arm64-v8a': {
        'arch': 'aarch64-linux-android',
        'ndk_arch': 'arm64',
        'cflags': '-fpic',
    },

    'x86': {
        'arch': 'i686-linux-android',
        'ndk_arch': 'x86',
        'cflags': '-fPIC -march=i686 -mtune=intel -mssse3 -mfpmath=sse -m32',
    },

    'x86_64': {
        'arch': 'x86_64-linux-android',
        'ndk_arch': 'x86_64',
        'cflags': '-fPIC -m64',
    },
}

# https://developer.android.com/ndk/guides/other_build_systems
def build_arch() :
    platforms = {
        'Linux': 'linux-x86_64',
        'Windows': 'windows-x86_64',
        'Darwin': 'darwin-x86_64' # Despite the x86_64 tag in the Darwin name, those are fat binaries that include M1 support.
    }

    return platforms.get(platform.system())


class AndroidNdkToolchain:
    def __init__(self, top_path: str, lib_path: str,
                 tarball_path: str, src_path: str,
                 ndk_path: str, android_abi: str,
                 use_cxx):
        # select the NDK target
        abi_info = android_abis[android_abi]
        host_triplet = abi_info['arch']

        arch_path = os.path.join(lib_path, host_triplet)

        self.tarball_path = tarball_path
        self.src_path = src_path
        self.build_path = os.path.join(arch_path, 'build')

        ndk_arch = abi_info['ndk_arch']
        android_api_level = '24'

        install_prefix = os.path.join(arch_path, 'root')

        self.host_triplet = host_triplet
        self.install_prefix = install_prefix

        llvm_path = os.path.join(ndk_path, 'toolchains', 'llvm', 'prebuilt', build_arch())
        llvm_triple = host_triplet + android_api_level

        common_flags = '-Os -g'
        common_flags += ' ' + abi_info['cflags']

        llvm_bin = os.path.join(llvm_path, 'bin')
        self.cc = with_ccache(os.path.join(llvm_bin, 'clang'))
        self.cxx = with_ccache(os.path.join(llvm_bin, 'clang++'))
        common_flags += ' -target ' + llvm_triple

        common_flags += ' -fvisibility=hidden -fdata-sections -ffunction-sections'

        self.ar = os.path.join(llvm_bin, 'llvm-ar')
        self.arflags = 'rcs'
        self.ranlib = os.path.join(llvm_bin, 'llvm-ranlib')
        self.nm = os.path.join(llvm_bin, 'llvm-nm')
        self.strip = os.path.join(llvm_bin, 'llvm-strip')

        self.cflags = common_flags
        self.cxxflags = common_flags
        self.cppflags = ' -isystem ' + os.path.join(install_prefix, 'include')
        self.ldflags = ' -L' + os.path.join(install_prefix, 'lib') + \
            ' -Wl,--exclude-libs=ALL' + \
            ' ' + common_flags
        self.ldflags = common_flags
        self.libs = ''

        self.is_arm = ndk_arch == 'arm'
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_aarch64 = ndk_arch == 'arm64'
        self.is_windows = False
        self.is_android = True
        self.is_darwin = False

        libstdcxx_flags = ''
        libstdcxx_cxxflags = ''
        libstdcxx_ldflags = ''
        libstdcxx_libs = '-static-libstdc++'

        if self.is_armv7:
            # On 32 bit ARM, clang generates no ".eh_frame" section;
            # instead, the LLVM unwinder library is used for unwinding
            # the stack after a C++ exception was thrown
            libstdcxx_libs += ' -lunwind'

        if use_cxx:
            self.cxxflags += ' ' + libstdcxx_cxxflags
            self.ldflags += ' ' + libstdcxx_ldflags
            self.libs += ' ' + libstdcxx_libs

        self.env = dict(os.environ)

        # redirect pkg-config to use our root directory instead of the
        # default one on the build host
        bin_dir = os.path.join(install_prefix, 'bin')
        os.makedirs(bin_dir, exist_ok=True)
        self.pkg_config = shutil.copy(os.path.join(top_path, 'build', 'pkg-config.sh'),
                                      os.path.join(bin_dir, 'pkg-config'))
        self.env['PKG_CONFIG'] = self.pkg_config

class MingwToolchain:
    def __init__(self, top_path: str,
                 toolchain_path, host_triplet, x64: bool,
                 tarball_path, src_path, build_path, install_prefix):
        self.host_triplet = host_triplet
        self.tarball_path = tarball_path
        self.src_path = src_path
        self.build_path = build_path
        self.install_prefix = install_prefix

        toolchain_bin = os.path.join(toolchain_path, 'bin')
        self.cc = with_ccache(os.path.join(toolchain_bin, host_triplet + '-gcc'))
        self.cxx = with_ccache(os.path.join(toolchain_bin, host_triplet + '-g++'))
        self.ar = os.path.join(toolchain_bin, host_triplet + '-ar')
        self.arflags = 'rcs'
        self.ranlib = os.path.join(toolchain_bin, host_triplet + '-ranlib')
        self.nm = os.path.join(toolchain_bin, host_triplet + '-nm')
        self.strip = os.path.join(toolchain_bin, host_triplet + '-strip')
        self.windres = os.path.join(toolchain_bin, host_triplet + '-windres')

        common_flags = '-O2 -g'

        if not x64:
            # enable SSE support which is required for LAME
            common_flags += ' -march=pentium3'

        self.cflags = common_flags
        self.cxxflags = common_flags
        self.cppflags = '-isystem ' + os.path.join(install_prefix, 'include') + \
                        ' -DWINVER=0x0600 -D_WIN32_WINNT=0x0600'
        self.ldflags = '-L' + os.path.join(install_prefix, 'lib') + \
                       ' -static-libstdc++ -static-libgcc'
        self.libs = ''

        # Explicitly disable _FORTIFY_SOURCE because it is broken with
        # mingw.  This prevents some libraries such as libFLAC to
        # enable it.
        self.cppflags += ' -D_FORTIFY_SOURCE=0'

        self.is_arm = host_triplet.startswith('arm')
        self.is_armv7 = self.is_arm and 'armv7' in self.cflags
        self.is_aarch64 = host_triplet == 'aarch64'
        self.is_windows = 'mingw32' in host_triplet
        self.is_android = False
        self.is_darwin = False

        self.env = dict(os.environ)

        # redirect pkg-config to use our root directory instead of the
        # default one on the build host
        import shutil
        bin_dir = os.path.join(install_prefix, 'bin')
        os.makedirs(bin_dir, exist_ok=True)
        self.pkg_config = shutil.copy(os.path.join(top_path, 'build', 'pkg-config.sh'),
                                      os.path.join(bin_dir, 'pkg-config'))
        self.env['PKG_CONFIG'] = self.pkg_config

AnyToolchain = Union[AndroidNdkToolchain, MingwToolchain]
