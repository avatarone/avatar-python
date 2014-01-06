'''
Created on Jun 24, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from avatar.targets.target import Target
import socket
import logging
from avatar.interfaces.avatar_stub.avatar_protocol import AvatarProtocol
from avatar.system import EVENT_STOPPED, EVENT_BREAKPOINT
from avatar.debuggable import Breakpoint
from queue import Queue

log = logging.getLogger(__name__)

ARM_REGISTER_NAMES = {"r0": 0, 
                      "r1": 1,
                      "r2": 2,
                      "r3": 3,
                      "r4": 4,
                      "r5": 5,
                      "r6": 6,
                      "r7": 7,
                      "r8": 8,
                      "r9": 9,
                      "r10": 10,
                      "r11": 11,
                      "r12": 12,
                      "r13": 13,
                      "sp": 13,
                      "r14": 14,
                      "lr": 14,
                      "r15": 15,
                      "pc": 15,
                      "cpsr": 16}

class AvatarBreakpoint(Breakpoint):
    def __init__(self, system, address):
        super().__init__()
        self._system = system
        self._address = address
        self._queue = Queue()
        system.register_event_listener(self._event_receiver)
        
    def wait(self, timeout = None):
        if self._handler:
            raise Exception("Breakpoint cannot have a handler and be waited on")

        if timeout == 0:
            return self._queue.get(False)
        else:
            return self._queue.get(True, timeout)
    
    def delete(self):
        self._system.unregister_event_listener(self._event_receiver)
        self._system.get_target().clear_breakpoint(self._address)
        
    def _event_receiver(self, evt):
        if EVENT_BREAKPOINT in evt["tags"] and \
                evt["source"] == "target" and \
                evt["properties"]["address"] == self._address:
            if self._handler:
                self._handler(self._system, self)
            else:
                self._queue.put(evt)

class AvatarstubTarget(Target):
    def __init__(self, system):
        self._system = system
        self._breakpoints = {}

    def init(self):
        conf = self._system.get_configuration()
        assert("avatar_configuration" in conf)
        assert("target_gdb_address" in conf["avatar_configuration"])
        assert(conf["avatar_configuration"]["target_gdb_address"].startswith("tcp:"))
        sockaddr_str = conf["avatar_configuration"]["target_gdb_address"][4:]
        sockaddr = (sockaddr_str[:sockaddr_str.rfind(":")],
                int(sockaddr_str[sockaddr_str.rfind(":") + 1:]))
        self._sockaddress = sockaddr
        
    def start(self):
        #TODO: Handle timeout
        log.info("Trying to connect to target avatar server at %s:%d", self._sockaddress[0], self._sockaddress[1])
        self._socket = socket.create_connection(self._sockaddress, 10)
        self._avatar_connection = AvatarProtocol(self._socket, self._handle_asynchronous_message)
        
    def read_typed_memory(self, address, size):
        return self._avatar_connection.read_memory(address, size)
            
    def read_untyped_memory(self, address, length):
        return self._avatar_connection.read_memory_untyped(address, length)
    
    def write_typed_memory(self, address, size, value):
        self._avatar_connection.write_memory(address, size, value)
        
    def write_untyped_memory(self, address, data):
        chunk_address = address
        remaining_size = len(data)
        while remaining_size > 0:
            self._avatar_connection.write_memory_untyped(address, data[address:address + 255])
            chunk_address += 255
            remaining_size -= 255
            
    def get_register(self, name):
        if isinstance(name, str):
            name = ARM_REGISTER_NAMES[name.lower()]
        return self._avatar_connection.get_register(name)
        
    def set_register(self, name, value):
        if isinstance(name, str):
            name = ARM_REGISTER_NAMES[name.lower()]
        self._avatar_connection.set_register(name, value)
        
    def install_codelet(self, address, codelet):
        self.write_untyped_memory(address, codelet)
        
    def execute_codelet(self, address):
        self._avatar_connection.execute_codelet(address)
        
    def set_breakpoint(self, address, **properties):
        if address in self._breakpoints:
            raise Exception("Breakpoint at 0x%x already set, not setting a new one" % address)
        
        bkpt = {"instruction_set": "arm"}
        if "thumb" in properties and properties['thumb']:
            bkpt["instruction_set"] = "thumb"
            
        self._breakpoints[address] = bkpt
        
        return AvatarBreakpoint(self._system, address)
        
    def clear_breakpoint(self, address):
        del self._breakpoints[address]
        
    def cont(self):
        pc = self.get_register("pc")
        for (bkpt_address, bkpt_data) in self._breakpoints.items():
            if bkpt_address == pc:
                continue
            
            if bkpt_data["instruction_set"] == "arm":
                self.write_typed_memory(bkpt_address, 4, 0xe1200071)
            elif bkpt_data["instruction_set"] == "thumb":
                self.write_typed_memory(bkpt_address, 2, 0xbe01)
        self._avatar_connection.cont()
    
    def _handle_asynchronous_message(self, msg):
        print("Received event")
        if msg.name == "AVATAR_RPC_DTH_STATE":
            #TODO: Look at state and act accordingly (Breakpoint, page fault, etc)
            self._system.post_event({"tags": [EVENT_STOPPED, EVENT_BREAKPOINT], 
                                     "channel": "avatar", 
                                     "source": "target",
                                     "properties": {
                                            "address": self.get_register("pc")}})
            
        
    

def init_avatarstub_target(system):
    system.set_target(AvatarstubTarget(system))
    
