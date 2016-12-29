import hashlib

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
