from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from avatar.emulators.s2e.s2e_emulator import S2EEmulator

def init_s2e_emulator(system):
    system.set_emulator(S2EEmulator(system))