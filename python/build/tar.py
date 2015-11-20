import os, shutil, subprocess

def untar(tarball_path, parent_path, base):
    path = os.path.join(parent_path, base)
    try:
        shutil.rmtree(path)
    except FileNotFoundError:
        pass
    os.makedirs(parent_path, exist_ok=True)
    subprocess.check_call(['/bin/tar', 'xfC', tarball_path, parent_path])
    return path
