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
subprocess.check_call(['ninja', 'install'])

android_abi = sys.argv[3]

print("""
-------------------------------------
## To build the android app:
# cd ../../android
# ./gradlew assemble{}Debug
## or, for a universal apk (includes both arm64-v8a and x86_64)
# ./gradlew assembleUniversalDebug
-------------------------------------
""".format(android_abi.capitalize()))
