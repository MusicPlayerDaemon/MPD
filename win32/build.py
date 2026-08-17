#!/usr/bin/env -S python3 -u

import os.path
import subprocess
import sys

subprocess.check_call(
    [
        os.path.join(os.path.dirname(__file__), 'configure.py'),
    ] + sys.argv[1:],
)
subprocess.check_call(['ninja'])
