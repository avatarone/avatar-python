from avatar.debuggable import Debuggable
from avatar.interfaces.gdb.protocol_lowlevel import GdbLowlevelProtocol
import socket
import struct

import logging
log = logging.getLogger(__name__)

class InvalidChecksumException(Exception):
    pass     

class GdbServer:
    def __init__(self, debuggable, sock, system, verbose=False):
        self._verbose=verbose
        self._debuggable = debuggable
        self._socket = sock
        self._low_level_protocol = GdbLowlevelProtocol(sock, self._message_received, verbose=self._verbose)
        self._low_level_protocol.start()
        self._system=system
        self._system.register_event_listener(self.handle_event)
        self._breakpoints = {}

    def handle_event(self, evt):
        # TODO this is very very verbose ...
        if self._verbose:
            log.info("gdb_server handle_event recieved : %s", evt)
        if 'EVENT_BREAKPOINT' in evt['tags'] or 'EVENT_END_STEPPING' in evt['tags']  :
            self._low_level_protocol.send_message("S05")

    def _message_received(self, msg):
        if self._verbose: print("Received message: %s" % msg)
        opcode = msg[0]
        if opcode == 'm':
            address = int(msg[1:].split(',')[0], 16)
            size = int(msg[1:].split(',')[1])
            if size in [1, 2, 4]:
                value = self._debuggable.read_typed_memory(address, size)
                self._low_level_protocol.send_message("".join(["%02x" % x for x in struct.pack("<%s" % {1: "B", 2: "H", 4: "L"}[size], value)]))
            else:
                value = self._debuggable.read_untyped_memory(address, size)
                self._low_level_protocol.send_message("".join(["%02x" % x for x in value]))
        elif opcode == 'M':
            address_size = msg[1:].split(":")[0]
            address = address_size.split(",")[0]
            size = address_size.split(",")[1]
            data = bytes([int(x, 16) for x in zip(*[iter(msg[1:].split(":")[1])] * 2)])
            
            assert(len(data) == size)
            
            if size in [1, 2, 4]:
                (value, ) = struct.unpack("<%s" % {1: "B", 2: "H", 4: "L"}[size], data)
                self._debuggable.write_typed_memory(address, size, value)
            else:
                self._debuggable.write_untyped_memory(address, data)
            #TODO: On error reply with error message
            self._low_level_protocol.send_message("OK")
        elif opcode == 'g':
            #TODO: ARM specific
            registers = ["r0", "r1", "r2", "r3", "r4", "r5", 
                "r6", "r7", "r8", "r9", "r10", 
                "r11", "r12", "sp", "lr", "pc", "cpsr"]
            register_values = [self._debuggable.get_register(x) for x in registers]
            data = "".join(["".join(["%02x" % y for y in struct.pack("<L", x)]) for x in register_values])
            self._low_level_protocol.send_message(data)
        elif opcode == 'G':
            registers = ["r0", "r1", "r2", "r3", "r4", "r5", 
                "r6", "r7", "r8", "r9", "r10", 
                "r11", "r12", "sp", "lr", "pc", "cpsr"]
            register_data = [int("".join(x), 16) for x in zip(*[iter(msg[1:])] * 2)]
            register_values = [struct.unpack("<L", bytes(x)) for x in zip(*[iter(register_data)] * 4)]
            for (reg, val) in zip(registers, register_values):
                self._debuggable.set_register(reg, val)
            self._low_level_protocol.send_message("OK")
        elif opcode == 'p':
            regnr = int(msg[1:], 16)
            value = self._debuggable.get_register_from_nr(regnr)
            self._low_level_protocol.send_message("".join(["%02x" % x for x in struct.pack("<L", value)]))
        elif opcode == 'P':
            regnr = int(msg[1:].split("=")[0], 16)
            value_wrong_endianness = int(msg[1:].split("=")[1], 16)
            value = struct.unpack("<L", struct.pack(">L", value_wrong_endianness))[0]
            self._debuggable.set_register(regnr, value) 
            self._low_level_protocol.send_message("OK")
        elif opcode == 'c':
            self._debuggable.cont()
        elif opcode == 'z':
            tipe = ord(msg[1]) - ord('0')
            address = int(msg[1:].split(",")[1], 16)
            kind = msg[1:].split(",")[2]
            if tipe in [0, 1]:
                self._breakpoints[address].delete()
                del self._breakpoints[address]
 #               self._debuggable.clear_breakpoint(address, )
            else:
                assert(False) #Unimplemented break/watchpoint type
            self._low_level_protocol.send_message("OK")
        elif opcode == 'Z':
            tipe = ord(msg[1]) - ord('0')
            address = int(msg[1:].split(",")[1], 16)
            kind = msg[1:].split(",")[2]
            if tipe in [0, 1]:
                bkpt = self._debuggable.set_breakpoint(address)
                self._breakpoints[address] = bkpt
            else:
                assert(False) #Unimplemented break/watchpoint type
            self._low_level_protocol.send_message("OK")
        elif opcode == '?':
            #TODO: Return real state instead of dummy
            self._low_level_protocol.send_message("S05")
        elif opcode == 's':
            self._debuggable.stepi()
        elif opcode in ['v','q','H']:
            log.info("gdb opcode '%s' not supported, but should be ok"% opcode)
            self._low_level_protocol.send_message("")
        else :
            log.error("gdb_server.py: unknown gdb opcode '%s' (not implemented?)"% opcode)
            self._low_level_protocol.send_message("")
            
class TestDebuggable(Debuggable):
    def read_typed_memory(self, address, size):
        print("Called read_typed_memory(0x%08x, %d)" % (address, size))
        return 0xcafebabe & (0xFFFFFFFF >> (8 * (4 - size)))
    
    def write_typed_memory(self, address, size, value):
        print("Called write_typed_memory(0x%08x, %d, 0x%08x)" % (address, size, value))
    
    def read_untyped_memory(self, address, length):
        print("Called read_untyped_memory(0x%08x, %d)" % (address, length))
        return bytes([0xfe] * length)
        
    def write_untyped_memory(self, address, data):
        print("Called write_untyped_memory(0x%08x, %s)" % (address, ".".join(["%02x" % x for x in data])))
        
    def get_register(self, register):
        print("Called get_register(%s)" % str(register))
        return 0xdeadbeef
    
    def set_register(self, register, value):
        print("Called set_register(%s, 0x%08x)" % (str(register), value))
        
    def set_breakpoint(self, address, **properties):
        print("Called set_breakpoint(0x%08x)" % address)

    def clear_breakpoint(self, address, **properties):
        print("Called clear_breakpoint(0x%08x)" % address)
    
    def cont(self):
        print("Called cont")

if __name__ == "__main__":
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 5555))
    sock.listen(1)
    (s, _) = sock.accept()
    print("GDB connected")
    gdb = GdbServer(TestDebuggable(), s)
       
           
