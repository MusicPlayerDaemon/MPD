import os, shutil
import re

from build.download import download_and_verify
from build.tar import untar

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

    def download(self, toolchain):
        return download_and_verify(self.url, self.md5, toolchain.tarball_path)

    def is_installed(self, toolchain):
        tarball = self.download(toolchain)
        installed = os.path.join(toolchain.install_prefix, self.installed)
        tarball_mtime = os.path.getmtime(tarball)
        try:
            return os.path.getmtime(installed) >= tarball_mtime
        except FileNotFoundError:
            return False

    def unpack(self, toolchain, out_of_tree=True):
        if out_of_tree:
            parent_path = toolchain.src_path
        else:
            parent_path = toolchain.build_path
        return untar(self.download(toolchain), parent_path, self.base)

    def make_build_path(self, toolchain):
        path = os.path.join(toolchain.build_path, self.base)
        try:
            shutil.rmtree(path)
        except FileNotFoundError:
            pass
        os.makedirs(path, exist_ok=True)
        return path
