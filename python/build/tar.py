import os, shutil, subprocess

def untar(tarball_path: str, parent_path: str, base: str,
          lazy: bool=False) -> str:
    path = os.path.join(parent_path, base)
    if lazy and os.path.isdir(path):
        return path
    try:
        shutil.rmtree(path)
    except FileNotFoundError:
        pass
    os.makedirs(parent_path, exist_ok=True)
    try:
        subprocess.check_call(['tar', 'xfC', tarball_path, parent_path])
    except FileNotFoundError:
        import tarfile
        tar = tarfile.open(tarball_path)
        tar.extractall(path=parent_path)
        tar.close()
    return path
