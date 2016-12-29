from build.verify import file_md5
import os
import urllib.request

def download_and_verify(url, md5, parent_path):
    """Download a file, verify its MD5 checksum and return the local path."""

    os.makedirs(parent_path, exist_ok=True)
    path = os.path.join(parent_path, os.path.basename(url))

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
        raise RuntimeError("MD5 mismatch")

    os.rename(tmp_path, path)
    return path
