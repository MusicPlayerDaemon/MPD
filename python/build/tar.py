import os, shutil, subprocess

def untar(tarball_path, parent_path, base):
    path = os.path.join(parent_path, base)
    try:
        shutil.rmtree(path)
    except FileNotFoundError:
        pass
    os.makedirs(parent_path, exist_ok=True)
    try:
        subprocess.check_call(['/bin/tar', 'xfC', tarball_path, parent_path])
    except FileNotFoundError:
        import tarfile
        tar = tarfile.open(tarball_path)
        tar.extractall(path=parent_path)
        tar.close()
    return path
