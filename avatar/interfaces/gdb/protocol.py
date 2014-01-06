'''
Created on May 2, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
#!/usr/bin/env python

import socket
import collections
import queue
import threading
from select import select
from avatar.interfaces.gdb.errors import ProtocolError
import functools
import logging


log = logging.getLogger(__name__)

ARM_REGISTERS = {"r0": 0, "r1": 1, "r2": 2, "r3": 3, "r4": 4, "r5": 5, "r6": 6, "r7": 7, "r8": 8, "r9": 9, "r10": 10, "r11": 11,
                "r12": 12, "sp": 13, "lr": 14, "pc": 15, "cpsr": -1}
                
                
ARM_REGISTERS_IN_g_RESPONSE = {0 : "r0", 1: "r1", 2: "r2", 3: "r3", 4: "r4", 5: "r5", 6: "r6", 7: "r7", 8: "r8", 9: "r9", 10: "r10",
                11: "r11", 12: "r12", 13: "sp", 14: "lr", 15: "pc", -1: "cpsr"}

# This is openOCD specific. TODO: later, uniform them with gdb-stub
BANKED_REGISTERS = {"sp_usr": 26, "lr_usr": 27, "sp_sys": 26, "lr_sys": 27,
                    "r8_fiq": 28, "r9_fiq": 29, "r10_fiq": 30, "r11_fiq": 31,
                    "r12_fiq": 32, "sp_fiq": 33, "lr_fiq": 34, "sp_irq": 35,
                    "lr_irq": 36, "sp_svc": 37, "lr_svc": 38, "sp_abt": 39,
                    "lr_abt": 40, "sp_und": 41, "lr_und": 42, "cpsr_usr": 43,
                    "spsr_fiq": 44, "spsr_irq": 45, "spsr_svc": 46,
                    "spsr_abt": 47, "spsr_und": 48}
                
class Protocol(object):
    def contains_message_end(self, buffer):
        pass
    
    def parse_message(self, buffer):
        pass
    
    def serialize_message(self, msg):
        pass
    
    def is_asynchronous_message(self, msg):
        pass
    
    
class MessagePasser(threading.Thread):
    def __init__(self, comm_file, protocol):
        super(MessagePasser, self).__init__()
        self.comm_file = comm_file
        self.stop = threading.Event()
        self.protocol = protocol
        
        self.start()
        
    def run(self):
        buffer = []
        while not self.stop.is_set():
            (rd, _, _) = select.select([self.comm_file], [], [], 0.5)
            if rd:
                buffer.append(self.comm_file.read())
                while self.contains_message_end(buffer):
                    (buffer, parsed_message) = self.parse_message(buffer)
                    if self.is_asynchronous_message(parsed_message):
                        self.asynchronous_messages.push(parsed_message)
                    else:
                        self.synchronous_messages.push(parsed_message)
                        
    def send_synchronous_message(self, msg):
        self.comm_file.write(self.protocol.serialize_message(msg))
        return self.synchronous_messages.pop()
        
    def send_asynchronous_message(self, msg):
        self.comm_file.write(self.protocol.serialize_message(msg))
        
    def get_asynchronous_message(self, timeout):
        return self.asynchronous_messages.pop(timeout)        

def arm_breakpoint(val):
    return 0xe1200070 | ((val << 4) & 0xFFF00) | (val & 0xF)
    
def thumb_breakpoint(val):
    return 0xbe00 | (val & 0xFF)
    
def data_to_long(data):
    return ord(data[0]) | (ord(data[1]) << 8) | (ord(data[2]) << 16) | (ord(data[3]) << 24)

class GdbError(ProtocolError):
    def __init__(self, msg):
        self.msg = msg
        
    def __str__(self):
        return self.msg

class GdbMessageFormatError(GdbError):
    def __init__(self, msg):
        super(GdbMessageFormatError, self).__init__(msg)
        
class GdbChecksumError(GdbMessageFormatError):
    def __init__(self):
        super(GdbChecksumError, self).__init__("checksum mismatch")
        
class GdbRemoteError(GdbError):
    def __init__(self, errno):
        super(GdbRemoteError, self).__init__("Remote error: %d" % errno)
        
class GdbUnexpectedResponseError(GdbMessageFormatError):
    def __init__(self, resp):
        super(GdbUnexpectedResponseError, self).__init__("Unexpected response: '%s'" % resp)
        
class GdbProtocol(object):
    def __init__(self, sock, console_print_callback = None, unknown_message_callback = None):
        """Initialize the GDB clone.
           streams_file is a file object which communicates with the remote GDB.
        """
        self._socket = sock
        self.breakpoints = {}
        self._registers = None
        self._configuration = {
            "software_single_step": False
        }
        self.input_buffer = collections.deque()
        self.console_print_callback = console_print_callback
        self.unkown_message_callback = unknown_message_callback
        self.responses = queue.Queue()
        self.receive_thread_object = threading.Thread(target = self.receive_thread)
        self.stop_receive_thread = False
        
        self.send_message("?")
        #Drain receive buffers
        while select([self._socket], [], [], 0) != ([], [], []):
            self._socket.recv(1)
        #self.send_message("?")
        self.receive_thread_object.start()
        self.wait_signal(10)
        
    
    def receive_response(self, timeout = None):
        try:
            return self.responses.get(True, timeout)
        except queue.Empty:
            return None 
                
    def send_message(self, data):
        checksum = functools.reduce(lambda x, y: (x + y) % 256, map(ord, data))
        message = "$%s#%02x" % (data, checksum)
#        print "Sending message: '%s'" % message
        self._socket.send(message.encode(encoding = 'ascii'))
#        self._socket.write(message)
                
    def receive_thread(self):
        data = ""
        log.info("Receive thread started")
        
        while not self.stop_receive_thread:
            try:
                #TODO: inefficient
                data += self._socket.recv(1).decode(encoding = 'ascii')
                
                #This is not an eternal loop, escape done by breaks below
                while True:
                    #Find a substring where '$' is followed by '#' and two additional characters
                    start_idx = data.find('$')
                    if start_idx == -1:
                        break
                    end_idx = data.find('#', start_idx)
                    
                    if end_idx == -1 or (end_idx + 2) >= len(data):
                        break
                    
                    try:    
                        resp = self.parse_message(data[start_idx:end_idx + 3])
                        data = data[end_idx]
                    
                        self._socket.send('+'.encode(encoding = 'ascii'))
                    
                        if resp:
                            #if resp[0] == 'S':
                                #TODO: Currently this message is just ignored
                            #    print("Received interrupting signal '%s' - ignoring" % resp)
                            #else:
    #                            #TODO: check if this is an IRQ message
    #                           print("Queueing response '%s'" % resp)
                            self.responses.put(resp)
                    except GdbMessageFormatError:
                        self.streams_file.write('-')
                        self.streams_file.flush()
            except socket.timeout:
                pass
                
                
    def parse_message(self, raw_data):
#        print "Parsing message '%s'" % raw_data
        #Verify checksum
        parts = raw_data.split('#')
        msg_data = parts[0][1:] #Strip the leading '$'
        received_checksum = int(parts[1], 16)
        calculated_checksum = functools.reduce(lambda x, y: (x + y) % 256, map(ord, msg_data), 0)
        
#        print "Message data '%s', received checksum 0x%02x, calculated checksum 0x%02x" % (msg_data, received_checksum, calculated_checksum)
        
        if received_checksum != calculated_checksum:
            print("Checksum error in message")
            raise GdbChecksumError()
            
        return msg_data    
            
    def read_memory(self, address, length):
        self.send_message("m%x,%x" % (address, length))
        resp = self.receive_response()
        if len(resp) == 3 and resp[0] == 'E':
            raise GdbRemoteError(int(resp[1:], 16))
        elif len(resp) != 2 * length:
            raise GdbUnexpectedResponseError(resp)
        else:
            #Convert hex representation of memory to data
            resp = list(map(lambda x: int(x, 16), map("".join, zip(*[iter(resp)] * 2))))
        return resp
        
    def write_memory(self, address: "int", value: "list(char)"):
        self.send_message("M%x,%x:%s" % (address, len(value), "".join(map(lambda x: "%02x" % ord(x), value))))
        resp = self.receive_response()
        if len(resp) == 3 and resp[0] == 'E':
            raise GdbRemoteError(int(resp[1:], 16))
        elif resp == "OK":
            return
        else:
            raise GdbUnexpectedResponseError(resp)
            
    def add_breakpoint(self, address, properties = {}):
        if (not "type" in properties) or (properties["type"] == "hardware"):
            #Hardware breakpoint
            log.debug("Adding GDB hardware breakpoint at 0x%08x" % address)
            pass
        elif "type" in properties and properties["type"] == "software":
            #Software breakpoint
            #Need this only for arch = arm, but this is the only supported currently
            thumb = "thumb" in properties and properties["thumb"]
            log.debug("Adding GDB %s software breakpoint at 0x%08x" % (thumb and "thumb" or "arm", address))
            pass

        self.breakpoints[address] = properties
        
    def remove_breakpoint(self, address: "int"):
        if address in self.breakpoints:
            del self.breakpoints[address]
        else:
            log.warn("Trying to delete nonexisting breakpoint at address 0x0%x" % address)
            
    def write_long(self, address, val):
        self.write_memory(address, [chr(val & 0xff), chr((val >> 8) & 0xff), chr((val >> 16) & 0xff), chr((val >> 24) & 0xff)])
        
    def write_short(self, address, val):
        self.write_memory(address, [chr(val & 0xff), chr((val >> 8) & 0xff)])
            
    def cont(self):
        self.inject_breakpoints()     
        self._registers = None          
        self.send_message("c")
        
    def step(self):
        self.inject_breakpoints()
        if self._configuration["software_single_step"]:
            assert(False) #Not implemented
        else:
            self._registers = None
            self.send_message("s")
        
    def inject_breakpoints(self):
        for bkpt in self.breakpoints:
            if self.breakpoints[bkpt][0] == "arm":
                self.breakpoints[bkpt][2] = self.read_memory(bkpt, 4)
                self.write_long(bkpt, arm_breakpoint(self.breakpoints[bkpt][1]))
            elif self.breakpoints[bkpt][0] == "thumb":
                self.breakpoints[bkpt][2] = self.read_memory(bkpt, 2)
                self.write_short(bkpt, thumb_breakpoint(self.breakpoints[bkpt][1]))
            else:
                log.warn("Unknown instruction set '%s'" % self.breakpoints[bkpt][0])

        
    def wait_signal(self, timeout = None):
        msg = self.receive_response(timeout)
        
        if msg is None or not msg[0] in ['S', 'T']:
            log.warn("GDB: Unexpected message instead of signal: '%s'" % msg)
        else:
            return int(msg[1:3], 10)
            
    def get_register(self, register):
        if not self._registers:
            self.get_registers()
            
        return self._registers[ARM_REGISTERS[register]]   
                
#        self.send_message("p%02d" % ARM_REGISTERS[register])
#        resp = self.receive_response()     
#        return data_to_long(resp)
        
    def set_register(self, register, value):
        if not self._registers:
            self.get_registers()
            
        self._registers[ARM_REGISTERS[register]] = value
        self.set_registers_internal()
#        self.send_message("P%02d=%08x" % (ARM_REGISTERS[register], value))   

    def get_single_register(self, register):
        reg_index = None
        if register in BANKED_REGISTERS.keys():
            reg_index = BANKED_REGISTERS[register]
        if register in ARM_REGISTERS.keys():
            reg_index = ARM_REGISTERS[register]
        if reg_index is not None:
            # wrt. absolute indexing, cpsr is at 25
            if reg_index == -1: 
                reg_index = 25
            self.send_message("p%02x" % reg_index)
            resp = self.receive_response()
            #print "%s -> %s %x" % (register, resp, data_to_long(resp))
            return data_to_long(resp)
        
    def get_registers(self):
        if not self._registers:
            self.send_message("g")
            resp = self.receive_response()
            #convert response to integers (little endian)
            le_converter = lambda x: int("".join(reversed(map("".join, zip(*[iter(x)] * 2)))), 16)
            try:
                register_values = map(le_converter, zip(*[iter(resp)] * 8))[:16]
                #cpsr is always at the end
                register_values.append(le_converter(resp[-8:]))
            except:
                raise GdbUnexpectedResponseError(resp)            
            self._registers = register_values
        
        registers = {}
        for reg_idx in ARM_REGISTERS_IN_g_RESPONSE.keys():
            registers[ARM_REGISTERS_IN_g_RESPONSE[reg_idx]] = self._registers[reg_idx]
            
        return registers
        
    def set_registers(self, regs):
        if not self._registers:
            self.get_registers()
            
        for reg_name in regs:
            self._registers[ARM_REGISTERS[reg_name]] = regs[reg_name]
            
        self.set_registers_internal()
        
    def set_registers_internal(self):
        msg = "G"
        for reg in self._registers:
            msg += "".join(map(lambda x: "%02x" % x, [reg & 0xff, (reg >> 8) & 0xff, (reg >> 16) & 0xff, (reg >> 24) & 0xff]))
             
        self.send_message(msg)
        
    
            
            
        
        
    
            
        
        
if __name__ == "__main__":
    logging.basicConfig(level = logging.DEBUG)
    sock = socket.create_connection(("127.0.0.1", 4444))
    log.info("Socket connected")
    gdb = GdbProtocol(sock.makefile())
    gdb.write_memory(0x1000, "\xca\xfe\xba\xbe")
    val = gdb.read_memory(0x1000, 4)
    
    log.info("Received Memory: %s" % (" ".join(map(lambda x: "%02X" % ord(x), val))))
