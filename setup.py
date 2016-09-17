#!/usr/bin/env python3
from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import open
from future import standard_library
standard_library.install_aliases()
from distutils.core import setup

setup(
    name='avatar',
    version='0.1.0dev1',
    author='Jonas Zaddach',
    author_email='zaddach@eurecom.fr',
    packages=[  'avatar',
                'avatar/bintools',
                'avatar/bintools.gdb',
                'avatar/targets',
                'avatar/plugins',
                'avatar/util',
                'avatar/interfaces',
                'avatar/interfaces/gdb',
                'avatar/interfaces/avatar_stub',
                'avatar/emulators',
                'avatar/emulators/s2e'],
    url='http://www.s3.eurecom.fr/tools/avatar/',
    license='Apache License 2.0',
    description='Dynamic firmware analysis',
    long_description=open('README.rst').read()
)
