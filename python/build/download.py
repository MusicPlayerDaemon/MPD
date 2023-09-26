from typing import Sequence, Union
import os
import sys
import urllib.request

from .verify import verify_file_digest

def __to_string_sequence(x: Union[str, Sequence[str]]) -> Sequence[str]:
    if isinstance(x, str):
        return (x,)
    else:
        return x

def __get_any(x: Union[str, Sequence[str]]) -> str:
    if isinstance(x, str):
        return x
    else:
        return x[0]

def __download_one(url: str, path: str) -> None:
    print("download", url)
    urllib.request.urlretrieve(url, path)

def __download(urls: Sequence[str], path: str) -> None:
    for url in urls[:-1]:
        try:
            __download_one(url, path)
            return
        except:
            print("download error:", sys.exc_info()[0])
    __download_one(urls[-1], path)

def __download_and_verify_to(urls: Sequence[str], md5: str, path: str) -> None:
    __download(urls, path)
    if not verify_file_digest(path, md5):
        raise RuntimeError("Digest mismatch")

def download_basename(urls: Union[str, Sequence[str]]) -> str:
    return os.path.basename(__get_any(urls))

def download_and_verify(urls: Union[str, Sequence[str]], md5: str, parent_path: str) -> str:
    """Download a file, verify its MD5 checksum and return the local path."""

    base = download_basename(urls)

    os.makedirs(parent_path, exist_ok=True)
    path = os.path.join(parent_path, base)

    try:
        if verify_file_digest(path, md5): return path
        os.unlink(path)
    except FileNotFoundError:
        pass

    tmp_path = path + '.tmp'

    __download_and_verify_to(__to_string_sequence(urls), md5, tmp_path)
    os.rename(tmp_path, path)
    return path
