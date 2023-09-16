import subprocess
from typing import Union

def run_quilt(toolchain, cwd: str, patches_path: str, *args: str) -> None:
    env = dict(toolchain.env)
    env['QUILT_PATCHES'] = patches_path
    subprocess.check_call(['quilt'] + list(args), cwd=cwd, env=env)

def push_all(toolchain, src_path: str, patches_path: str) -> None:
    run_quilt(toolchain, src_path, patches_path, 'push', '-a')
