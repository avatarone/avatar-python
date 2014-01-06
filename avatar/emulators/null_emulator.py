from avatar.emulators.s2e.configuration import S2EConfiguration
import logging
import subprocess
import os
from avatar.emulators.emulator import Emulator
from queue import Queue
log = logging.getLogger(__name__)


class NullEmulator(Emulator):
    def __init__(self, system):
        super().__init__(system)
        
    def init(self):
        pass
        
    def start(self):
        pass
        
    def stop(self):
        pass
            
    def exit(self):
        pass
        
    def write_typed_memory(self, address, size, data):
        pass

    def set_register(self, reg, val):
        pass

    def get_register(self, reg):
        return None
        
    def set_breakpoint(self, address, **properties):
        pass
    
    def cont(self):
        pass
        
def init_null_emulator(system):
    system.set_emulator(NullEmulator(system))
    
    
