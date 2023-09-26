import os, shutil
import re
from typing import cast, BinaryIO, Optional, Sequence, Union

from build.download import download_basename, download_and_verify
from build.tar import untar
from build.quilt import push_all
from .toolchain import AnyToolchain

class Project:
    def __init__(self, url: Union[str, Sequence[str]], md5: str, installed: str,
                 name: Optional[str]=None, version: Optional[str]=None,
                 base: Optional[str]=None,
                 patches: Optional[str]=None,
                 edits=None,
                 use_cxx: bool=False):
        if base is None:
            basename = download_basename(url)
            m = re.match(r'^(.+)\.(tar(\.(gz|bz2|xz|lzma))?|zip)$', basename)
            if not m: raise RuntimeError('Could not identify tarball name: ' + basename)
            self.base = m.group(1)
        else:
            self.base = base

        if name is None or version is None:
            m = re.match(r'^([-\w]+)-(\d[\d.]*[a-z]?[\d.]*(?:-(?:alpha|beta)\d+)?)(\+.*)?$', self.base)
            if not m: raise RuntimeError('Could not identify tarball name: ' + self.base)
            if name is None: name = m.group(1)
            if version is None: version = m.group(2)

        self.name = name
        self.version = version

        self.url = url
        self.md5 = md5
        self.installed = installed

        if patches is not None:
            srcdir = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
            patches = os.path.join(srcdir, patches)
        self.patches = patches
        self.edits = edits
        self.use_cxx = use_cxx

    def download(self, toolchain: AnyToolchain) -> str:
        return download_and_verify(self.url, self.md5, toolchain.tarball_path)

    def is_installed(self, toolchain: AnyToolchain) -> bool:
        tarball = self.download(toolchain)
        installed = os.path.join(toolchain.install_prefix, self.installed)
        tarball_mtime = os.path.getmtime(tarball)
        try:
            return os.path.getmtime(installed) >= tarball_mtime
        except FileNotFoundError:
            return False

    def unpack(self, toolchain: AnyToolchain, out_of_tree: bool=True) -> str:
        if out_of_tree:
            parent_path = toolchain.src_path
        else:
            parent_path = toolchain.build_path
        path = untar(self.download(toolchain), parent_path, self.base,
                     lazy=out_of_tree and self.patches is None)
        if self.patches is not None:
            push_all(toolchain, path, self.patches)

        if self.edits is not None:
            for filename, function in self.edits.items():
                with open(os.path.join(path, filename), 'r+t') as f:
                    old_data = f.read()
                    new_data = function(old_data)
                    f.seek(0)
                    f.truncate(0)
                    f.write(new_data)

        return path

    def make_build_path(self, toolchain: AnyToolchain, lazy: bool=False) -> str:
        path = os.path.join(toolchain.build_path, self.base)
        if lazy and os.path.isdir(path):
            return path
        try:
            shutil.rmtree(path)
        except FileNotFoundError:
            pass
        os.makedirs(path, exist_ok=True)
        return path

    def build(self, toolchain: AnyToolchain) -> None:
        self._build(toolchain)
