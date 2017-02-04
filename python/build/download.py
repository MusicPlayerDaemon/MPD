from build.verify import verify_file_digest
import os
import urllib.request

def download_and_verify(url, md5, parent_path):
    """Download a file, verify its MD5 checksum and return the local path."""

    os.makedirs(parent_path, exist_ok=True)
    path = os.path.join(parent_path, os.path.basename(url))

    try:
        if verify_file_digest(path, md5): return path
        os.unlink(path)
    except FileNotFoundError:
        pass

    tmp_path = path + '.tmp'

    print("download", url)
    urllib.request.urlretrieve(url, tmp_path)
    if not verify_file_digest(tmp_path, md5):
        os.unlink(tmp_path)
        raise RuntimeError("Digest mismatch")

    os.rename(tmp_path, path)
    return path
