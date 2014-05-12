'''
Created on Jul 1, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from avatar.bintools.gdb.mi import GDB, Debugger
import logging
from avatar.bintools.gdb.mi_parser import Stream

log = logging.getLogger(__name__)

class GdbDebugger(Debugger):
    def __init__(self, gdb_executable = "gdb", cwd = "."):
        self._async_message_handler = None
        self._stream_handler = None
        self._register_names = None
        self._gdb = GDB(self, executable = gdb_executable, cwd = cwd)
  
    def insert_breakpoint(self, 
                     line, 
                     hardware = False, 
                     temporary = False, 
                     regex = False, 
                     condition = None, 
                     ignore_count = 0, 
                     thread = 0):
        cmd = ["-break-insert"]
        if temporary:
            cmd.append("-t")
        if hardware:
            cmd.append("-h")
        if regex:
            assert((not temporary) and (not condition) and (not ignore_count))
            cmd.append("-r")
        if condition:
            cmd.append("-c")
            cmd.append(str(condition))
        if ignore_count:
            cmd.append("-i")
            cmd.append("%d" % ignore_count)
        if thread:
            cmd.append("-p")
            cmd.append("%d" % thread)
            
        if isinstance(line, int):
            cmd.append("*0x%x" % line)
        else:
            cmd.append(str(line))
            
        return self._gdb.sync_cmd(cmd, "done")

    def execute_gdb_command(self, cmd):
        if not isinstance(cmd, list):
            cmd = [cmd]
        return self._gdb.sync_cmd(cmd, "done")

    def write_memory(self, address, size, val):
        str_size = {1: "char", 2: "short", 4: "long"}[size]
        self._gdb.sync_cmd(["-gdb-set",  "*((%s *) 0x%x)=0x%x" % (str_size, address, val)], "done")
    
    def read_memory(self, address, size):
        str_size = {1: "char", 2: "short", 4: "long"}[size]
        result = self._gdb.sync_cmd(["-data-read-memory", "0x%x" % address, "x", "%d" % size, "1", "1"], "done")
        return int(result["memory"][0]["data"][0], 16)

    def get_checksum(self, address, size):
        result = self._gdb.sync_cmd(["-gdb-show", "remote", "checksum", "%x" % address, "%x" % size], "done")
        print("And the result is: " + repr(result))
        return int(result["value"], 10)

    def map_register_name(self, reg):
        if not self._register_names:
            #Fetch register names ...
            result = self._gdb.sync_cmd(["-data-list-register-names"], "done")
            self._register_names = dict(filter(lambda x: x[0] != "", zip(result["register-names"], range(0, 10000))))

        return self._register_names[reg]

    def get_register(self, reg):
        return self.get_register_from_nr(self.map_register_name(reg))

    def get_register_from_nr(self, reg_num):
        try :
            result = self._gdb.sync_cmd(["-data-list-register-values", "x", "%d" % reg_num], "done") 
            ret=int(result["register-values"][0]["value"], 16)
        except:
            log.error("register nr %s was requested but (probably) does not exist: returning 0",reg_num)
            ret=0
        return ret

    def set_register(self, reg, value):
        #reg_num = self.map_register_name(reg)
        self._gdb.sync_cmd(["-gdb-set", "$%s=0x%x" % (reg, value)], "done") 

    def delete_breakpoint(self, bkpt):
        self._gdb.sync_cmd(["-break-delete", "%d" % bkpt], "done")
        
    def stepi(self):
        self._gdb.sync_cmd(["-exec-step-instruction"], "running")

    def cont(self):
        self._gdb.sync_cmd(["-exec-continue"], "running")
        
    def send_signal(self, signalnr):
        self._gdb.sync_cmd(["-exec-interrupt"],"done")
        
    def connect(self, proto_addr_port):
        log.debug("Connecting to remote gdb: %s", ":".join(proto_addr_port))
        self._gdb.sync_cmd(["-target-select", "remote", ":".join(proto_addr_port)], "connected")
        
    def handle_async(self, msg):
        if self._async_message_handler:
            self._async_message_handler(msg)
            
    def handle_stream_msg(self, msg):
        if self._stream_handler:
            self._stream_handler(msg)
        else:
            if msg.type == Stream.CONSOLE:
                log.info(msg.string)
            elif msg.type == Stream.TARGET:
                log.info('> %s' % msg.string)
            elif msg.type == Stream.ERROR_LOG:
                log.error(msg.string)
        
    def set_async_message_handler(self, handler):
        self._async_message_handler = handler
        
    def set_output_stream_handler(self, handler):
        self._stream_handler = handler
