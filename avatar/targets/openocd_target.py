from avatar.targets.target import Target
import socket
import logging
import telnetlib

log = logging.getLogger(__name__)

#This names are OpenOCD specific
ARM_REGISTERS = ["r0", "r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11",
                  "r12","sp_usr","lr_usr","pc","cpsr","r8_fiq","r9_fiq",
                  "r10_fiq","r11_fiq","r12_fiq","sp_fiq","lr_fiq","spsr_fiq",
                  "sp_svc","lr_svc","spsr_svc","sp_abt","lr_abt","spsr_abt",
                  "sp_irq","lr_irq","spsr_irq","sp_und","lr_und","spsr_und"]

# Decorator for methods requiring target Stop&Start
def paused(fn):
    # TODO: variadic decorator
    def wrapped(self, opt=None):
        self.halt()
        if opt:
          fn(self, opt)
        else:
          fn(self)
        self.cont()
    return wrapped

# Decorator for methods requiring target Hard Stop
def halted(fn):
    # TODO: variadic decorator
    def wrapped(self, opt=None):
        self.halt()
        if opt:
          return fn(self, opt)
        else:
          return fn(self)
    return wrapped


class OpenocdTarget(Target):
    def __init__(self, sockaddr):
        self._sockaddress = sockaddr
        self._prompt = telnetlib.Telnet()
        self.start()
        
    def start(self):
        """ Start the telnet session to OpenOCD """
        log.info("Trying to connect to target openocd server at %s:%d", self._sockaddress[0], self._sockaddress[1])
        self._prompt.open(self._sockaddress[0], self._sockaddress[1])
        self.get_output()
            
    def stop(self):
        """ Stop the telnet session to OpenOCD """
        self.send_cmd("exit")
        self._prompt.close()
        del self._prompt
    
    def wait(self):
        self.get_output()
    
    def get_output(self, to=0):
        """ Get the output of last command """
        if to == 0:
            out = str(self._prompt.read_until(b"> "))
        else:
            out = str(self._prompt.read_until(b"> ", to))
        out = out.split("> ")[0]
        return out
    
    def halt(self):
        """ Stop the target """
        self.raw_cmd("halt", False)

    def cont(self):
        """ Resume the target """
        self.raw_cmd("resume", False)

    @paused
    def put_bp(self, addr : "str 0xNNNNNNNN"):
        """ Set a breakpoint """
		# XXX: hardcoded: thumb, hw breakpoint
        self.raw_cmd("bp %s 2 hw" % addr)
    
    @paused
    def remove_bp(self, addr : "str 0xNNNNNNNN"):
        """ Remove a breakpoint """
        self.remove_raw_bp(addr)

    def remove_raw_bp(self, addr : "str 0xNNNNNNNN"):
        """ Remove a breakpoint """
        self.raw_cmd("rbp %s" % addr)
    
    def raw_cmd(self, cmd, is_log=True):
        """ Send a raw command to OpenOCD """
        self._prompt.write(str(cmd+'\n').encode("ascii"))
        out=self.get_output()
        if is_log:
            log.info(out)
        return out
    
    @paused
    def get_register(self, regname : "str") -> "str 0xNNNNNNNN":
        """ Get the value of single registers """
        return self.get_raw_register(regname)
    
    def get_raw_register(self, regname : "str")-> "str 0xNNNNNNNN":
        """ Register reader, undecorated """
        assert(regname in ARM_REGISTERS)
        value=self.raw_cmd("reg " + regname, False).split(": ")[-1]
        # XXX
        return value.split("\\")[0]

    @halted
    def dump_all_registers(self)-> "dict{str: str 0xNNNNNNNN}":
        """ Loop over all available registers and dump their content """
        out = {}
        # Flush session input
        self.get_output(2)
        for i in ARM_REGISTERS:
            val = self.get_raw_register(i)
            try:
                out[i] = int(val, 0) # thanks json
            except Exception as ex:
                log.exception("%s ignored, read value was «%s»" % (i, val))
        return out
            
    def initstate(self, cfg : "dict of str"):
        """ Change S2E configurable machine initial setup"""
        assert("machine_configuration" in cfg)
        self.get_output(2)
        st = self.dump_all_registers()        
        cfg["machine_configuration"]["init_state"] = [st]
        # Override entry address
        if "pc" in st:
            cfg["machine_configuration"]["entry_address"] = st["pc"]            
        return cfg
    
#    def read_typed_memory(self, address, size):
        
            
#    def read_untyped_memory(self, address, length):
    
#    def write_typed_memory(self, address, size, value):
        
#    def write_untyped_memory(self, address, data):

    
    @classmethod
    def from_str(cls, sockaddr_str: "str, proto:addr:port"):
        """ Static factory """
        assert(sockaddr_str.startswith("tcp:"))
        sockaddr = (sockaddr_str[:sockaddr_str.rfind(":")],
                    int(sockaddr_str[sockaddr_str.rfind(":") + 1:]))
        return cls(sockaddr)
  
