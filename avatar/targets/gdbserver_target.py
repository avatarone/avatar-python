'''
Created on Jun 24, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from avatar.targets.target import Target
import logging
from avatar.bintools.gdb.gdb_debugger import GdbDebugger
from avatar.system import EVENT_RUNNING, EVENT_STOPPED, EVENT_BREAKPOINT, EVENT_SIGABRT
from avatar.bintools.gdb.mi_parser import Async
from avatar.debuggable import Breakpoint
from queue import Queue

log = logging.getLogger(__name__)

class GdbBreakpoint(Breakpoint):
    def __init__(self, system, bkpt_num):
        super().__init__()
        self._system = system
        self._bkpt_num = bkpt_num
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
        self._system.get_target()._gdb_interface.delete_breakpoint(self._bkpt_num)
        
    def _event_receiver(self, evt):
        if EVENT_BREAKPOINT in evt["tags"] and \
                evt["source"] == "target" and \
                evt["properties"]["bkpt_number"] == self._bkpt_num:
            if self._handler:
                self._handler(self._system, self)
            else:
                self._queue.put(evt)
        elif EVENT_SIGABRT in evt["tags"]:
                self._queue.put(evt)

class GdbserverTarget(Target):
    def __init__(self, system, verbose=False):
        self._system = system
        self._verbose = verbose
        
    def init(self):
        conf = self._system.get_configuration()
        assert("avatar_configuration" in conf)
        assert("target_gdb_address" in conf["avatar_configuration"])
        assert(conf["avatar_configuration"]["target_gdb_address"].startswith("tcp:"))
        sockaddr_str = conf["avatar_configuration"]["target_gdb_address"][4:]
        if "target_gdb_path" in conf["avatar_configuration"]:
                self.gdb_exec= conf["avatar_configuration"]["target_gdb_path"]
        else:
            self.gdb_exec = "/home/zaddach/projects/hdd-svn/gdb/gdb/gdb"
            log.warn("target_gdb_path not defined in avatar configuration, using hardcoded GDB path: %s", self.gdb_exec)
        if "target_gdb_additional_arguments" in conf["avatar_configuration"]:
            self.additional_args = conf["avatar_configuration"]["target_gdb_additional_arguments"]
        else:
            self.additional_args = []
        self._sockaddress = (sockaddr_str[:sockaddr_str.rfind(":")],
                             int(sockaddr_str[sockaddr_str.rfind(":") + 1:]))
        
    def start(self):
        #TODO: Handle timeout
        if self._verbose: log.info("Trying to connect to target gdb server at %s:%d", self._sockaddress[0], self._sockaddress[1])
        self._gdb_interface = GdbDebugger(gdb_executable = self.gdb_exec, cwd = ".", additional_args = self.additional_args )
        self._gdb_interface.set_async_message_handler(self.handle_gdb_async_message)
        self._gdb_interface.connect(("tcp", self._sockaddress[0], "%d" % self._sockaddress[1]))
        
    def write_typed_memory(self, address, size, data):
        self._gdb_interface.write_memory(address, size, data)

    def read_typed_memory(self, address, size):
        return self._gdb_interface.read_memory(address, size)

    def set_register(self, reg, val):
        self._gdb_interface.set_register(reg, val)

    def get_register(self, reg):
        return self._gdb_interface.get_register(reg)

    def execute_gdb_command(self, cmd):
        return self._gdb_interface.execute_gdb_command(cmd)
    def get_checksum(self, addr, size):
        return self._gdb_interface.get_checksum(addr, size)
        
    def stop(self):
        pass
    
    def set_breakpoint(self, address, **properties):
        if "thumb" in properties:
            del properties["thumb"]
        bkpt = self._gdb_interface.insert_breakpoint(address, *properties)
        return GdbBreakpoint(self._system, int(bkpt["bkpt"]["number"]))
        
    
    def cont(self):
        self._gdb_interface.cont()
        
    def handle_gdb_async_message(self, msg):
        print("Received async message: '%s'" % str(msg))
        if msg.type == Async.EXEC:
            if msg.klass == "running":
                self._post_event({"tags": [EVENT_RUNNING], "channel": "gdb"})
            elif msg.klass == "stopped":
                if "reason" in msg.results and msg.results["reason"] == "breakpoint-hit":
                    self._post_event({"tags": [EVENT_STOPPED, EVENT_BREAKPOINT],
                                     "properties": {
                                        "address": int(msg.results["frame"]["addr"], 16),
                                        "bkpt_number": int(msg.results["bkptno"])},
                                     "channel": "gdb"})
                elif "reason" in msg.results and msg.results["reason"] == "signal-received":
                    # this is data abort
                    try:
                        addr = int(msg.results["frame"]["addr"], 16)
                    except:
                        addr = 0xDEADDEAD
                    self._post_event({"tags": [EVENT_STOPPED, EVENT_SIGABRT],
                        "properties": {
                            "address": addr,
                            },
                        "channel": "gdb"})
    def _post_event(self, evt):
        evt["source"] = "target"
        self._system.post_event(evt)
    

    @classmethod
    def from_str(cls, sockaddr_str):
        assert(sockaddr_str.startswith("tcp:"))
        sockaddr = (sockaddr_str[:sockaddr_str.rfind(":")],
                    int(sockaddr_str[sockaddr_str.rfind(":") + 1:]))
        return cls(sockaddr)

def init_gdbserver_target(system):
    system.set_target(GdbserverTarget(system))
    
