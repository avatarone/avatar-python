from avatar.emulators.s2e.s2e_emulator import S2EEmulator

def init_s2e_emulator(system):
    system.set_emulator(S2EEmulator(system))