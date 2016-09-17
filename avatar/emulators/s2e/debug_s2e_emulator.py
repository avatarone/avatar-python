'''
Created on Jun 26, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import super
from builtins import open
from future import standard_library
standard_library.install_aliases()
from avatar.emulators.s2e.s2e_emulator import S2EEmulator
import logging
import subprocess
import time
import os.path
from avatar.interfaces.s2e_remote_memory import S2ERemoteMemoryInterface


log = logging.getLogger(__name__)


class DebugS2EEmulator(S2EEmulator):
    def __init__(self, system):
        system.get_configuration()["avatar_configuration"] = system.get_configuration()["avatar_configuration"] or {}
        system.get_configuration()["avatar_configuration"]["s2e_debug"] = True
        super().__init__(system)
        
    def run_s2e_process(self):
        try:
            log.info("Starting QEMU with S2E process: %s", " ".join(["'%s'" % x for x in self._cmdline]))
            gdb_file = os.path.join(self._configuration.get_output_directory(), "run.gdb")
            
            f = open(gdb_file, 'w')
            f.write("break main\n")
            f.write("run %s\n" % " ".join(["'%s'" % x for x in self._cmdline[1:]]))
            f.close()
            
            #TODO: get gdb program from config
            self._s2e_process = subprocess.Popen(["gdb", "-x", gdb_file, self._cmdline[0]], 
                                                 cwd = self._configuration.get_output_directory())
 
            self._remote_memory_interface = S2ERemoteMemoryInterface(self._configuration.get_remote_memory_listen_address())
            self._remote_memory_interface.set_read_handler(self._notify_read_request_handler)
            self._remote_memory_interface.set_write_handler(self._notify_write_request_handler)
            time.sleep(20) #Wait a bit for the S2E process to start
            self._remote_memory_interface.start()
            
            self._s2e_process.wait()
        except KeyboardInterrupt:
            pass
            
        self.exit()
        
def init_debug_s2e_emulator(system):
    system.set_emulator(DebugS2EEmulator(system))