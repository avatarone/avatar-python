'''
Created on May 3, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import range
from builtins import bytes
from future import standard_library
standard_library.install_aliases()
from builtins import object

class Breakpoint(object):
    """This is an interface for breakpoints that are created by Debuggable.set_breakpoint"""
    def __init__(self):
        self._handler = None

    def wait(self):
        """Wait until this breakpoint is hit"""
        assert(False) #Not implemented
        
    def delete(self):
        """Delete this breakpoint"""
        assert(False) #Not implemented

    def set_handler(self, handler):
        """Set the handler function that is called when the breakpoint is hit"""
        self._handler = handler
        

class Debuggable(object):
    """This is an interface for all objects that support a minimal set of debugging operations"""
    def read_typed_memory(self, address, size):
        """Read a memory word with size <b>size</b> from memory address
           <b>address</b>"""
        assert(False) #No implementation
    
    def write_typed_memory(self, address, size, value):
        """Write a memory word <b>val</b> with size <b>size</b> to memory address
           <b>address</b>"""
        assert(False) #No implementation
    
    def read_untyped_memory(self, address, length):
        """Read a range of untyped (byte) memory"""
        data = []
        for i in range(0, length):
            data.append(self.read_typed_memory(address + i, 1))

        return bytes(data)
    
    def write_untyped_memory(self, address, data):
        """Write untyped (byte) data"""
        for byte in data:
            self.write_typed_memory(address, 1, byte)
            address += 1
        
    def get_register(self, register):
        """Return the value of a register"""
        assert(False) #No implementation
    
    def set_register(self, register, value):
        """Set the value of a register"""
        assert(False) #No implementation
        
    def set_breakpoint(self, address, **properties):
        """
            Set a code breakpoint.
            Properties can be:
                - temporary (boolean): If True, breakpoint is deleted upon hit
                - thumb (boolean): If True and architecture is arm, a thumb
                    breakpoint will be used.
        """
        assert(False) #Not implemented

    def remove_breakpoint(self, address):
        """Remove a breakpoint"""
        assert(False) #Not implemented
            
    def dump_registers(self):
        """Dump the value of general registers"""
        assert(False) #No implementation

    def dump_all_registers(self):
        """Dump the value of all registers"""
        assert(False) #No implementation
    
    def cont(self):
        """Continue execution"""
        assert(False) #No implementation
        
    def halt(self):
        """Continue execution"""
        assert(False) #No implementation
        
    #TODO: Merge with set_breakpoint
    def put_bp(self, addr):
        """Put a breakpoint"""
        assert(False) #No implementation
        
    #TODO: Merge with remove_bp
    def remove_bp(self, addr):
        """Remove a breakpoint"""
        assert(False) #No implementation
