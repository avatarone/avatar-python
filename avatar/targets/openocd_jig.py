'''
@author: Luca BRUNO <lucab@debian.org>
'''
from avatar.targets.jig import Jig
import socket
import logging
import subprocess
import time
import os

log = logging.getLogger(__name__)

class OpenocdJig(Jig):
    def __init__(self, cfg):
        assert "openocd_configuration" in cfg
        assert "config_file" in cfg["openocd_configuration"]
        self._fname = cfg["openocd_configuration"]["config_file"]
        if self._fname[0] != '/':
            self._fname = cfg["configuration_directory"] + '/' + self._fname
        self._gdbaddress = ("127.0.0.1", 3333)
        self._telnetaddress = ("127.0.0.1", 4444)
        self.attach()
        
    def attach(self):
        with open(os.devnull, "w") as fnull:
            self._pid = subprocess.Popen(["openocd", "-f", self._fname], stdout=fnull)
        time.sleep(1) #Openocd initialization takes some time
        
    def detach(self):
        self._pid.kill()
    
    def get_gdb_jigstr(self):
        return "tcp:"+':'.join(str(i) for i in self._gdbaddress)
        
    def get_gdb_jigsock(self):
        return self._gdbaddress

    def get_telnet_jigstr(self):
        return "tcp:"+':'.join(str(i) for i in self._telnetaddress)
        
    def get_telnet_jigsock(self):
        return self._telnetaddress
    
    def __del__(self):
        self.detach()
    
    @classmethod
    def from_cfg(cls, cfg):
        return cls(cfg)

    @classmethod
    def from_str(cls, exe):
        cfg=[]
        cfg["openocd_configuration"]["config_file"] = exe
        return cls(cfg)
    
