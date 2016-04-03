import os.path

lib_path = os.path.abspath('lib')

shared_path = lib_path
if 'MPD_SHARED_LIB' in os.environ:
    shared_path = os.environ['MPD_SHARED_LIB']
tarball_path = os.path.join(shared_path, 'download')
src_path = os.path.join(shared_path, 'src')
