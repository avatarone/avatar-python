#!/usr/bin/env python3
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
