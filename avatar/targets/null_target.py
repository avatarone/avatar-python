'''
Created on Jun 24, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from avatar.targets.target import Target
import logging
from bintools.gdb.gdb_debugger import GdbDebugger
from avatar.system import EVENT_RUNNING, EVENT_STOPPED, EVENT_BREAKPOINT
from bintools.gdb.mi_parser import Async
from avatar.debuggable import Breakpoint
from queue import Queue

log = logging.getLogger(__name__)

class NullTarget(Target):
    def __init__(self, system, verbose=False):
        pass
        
    def init(self):
        pass
        
    def start(self):
        pass
        
    def write_typed_memory(self, address, size, data):
        pass

    def read_typed_memory(self, address, size):
        pass

    def set_register(self, reg, val):
        pass

    def get_register(self, reg):
        pass
        
    def stop(self):
        pass
    
    def set_breakpoint(self, address, **properties):
        pass
        
    
    def cont(self):
        pass

def init_null_target(system):
    system.set_target(NullTarget(system))