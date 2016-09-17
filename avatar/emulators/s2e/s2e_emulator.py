from __future__ import print_function
from __future__ import unicode_literals
from __future__ import division
from __future__ import absolute_import
from builtins import super
from builtins import str
from builtins import int
from future import standard_library
standard_library.install_aliases()
from avatar.emulators.s2e.configuration import S2EConfiguration
import logging
import subprocess
import os
from avatar.interfaces.s2e_remote_memory import S2ERemoteMemoryInterface
from avatar.emulators.emulator import Emulator
import time
from avatar.util.processes import find_processes
import signal
import threading
from avatar.bintools.gdb.gdb_debugger import GdbDebugger
from avatar.bintools.gdb.mi_parser import Async
from avatar.system import EVENT_RUNNING, EVENT_STOPPED, EVENT_BREAKPOINT, EVENT_END_STEPPING
from avatar.debuggable import Breakpoint
from queue import Queue
log = logging.getLogger(__name__)


class S2EBreakpoint(Breakpoint):
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
        self._system.get_emulator()._gdb_interface.delete_breakpoint(self._bkpt_num)
        
    def _event_receiver(self, evt):
        if EVENT_BREAKPOINT in evt["tags"] and \
                evt["source"] == "emulator" and \
                evt["properties"]["bkpt_number"] == self._bkpt_num:
            if self._handler:
                self._handler(self._system, self)
            else:
                self._queue.put(evt)
        

class S2EEmulator(Emulator):
    def __init__(self, system):
        super().__init__(system)
        self._configuration = S2EConfiguration(self._system.get_configuration())
        
    def init(self):
        self._configuration.write_configuration_files(self._system.get_configuration()["output_directory"])
        self._cmdline = self._configuration.get_command_line()
        
    def start(self):
        s2e_processes = find_processes(self._cmdline[0])
        if s2e_processes:
            log.warn("There are still S2E instances running, NOT killing them ...")
            log.warn("Results might be corrupted if output files are not different")
        log.info("Executing S2E process: %s", " ".join(["'%s'" % x for x in self._cmdline]))
        self._s2e_thread = threading.Thread(target = self.run_s2e_process)
        self._is_s2e_running = threading.Event()
        self._s2e_thread.start()
        #TODO: Would be nicer to put this somewhere in a function called is_running
        #so that other stuff can start in parallel and in the end the system waits for everything
        #to be running
        self._is_s2e_running.wait()
        
    def stop(self):
        if hasattr(self, "_s2e_process"):
            self._s2e_process.kill()
            
    def exit(self):
        if hasattr(self, "_remote_memory_interface"):
            self._remote_memory_interface.stop()
            
        print("Exiting")
        
    def run_s2e_process(self):
        try:
            log.info("Starting S2E process: %s", " ".join(["'%s'" % x for x in self._cmdline]))
        
            self._s2e_process = subprocess.Popen(
                        self._cmdline, 
                        cwd = self._configuration.get_output_directory(), 
                        stdout = subprocess.PIPE,
                        stderr = subprocess.PIPE)
            self._s2e_stdout_tee_process = subprocess.Popen(
                    ["tee", os.path.normpath(os.path.join(self._configuration.get_output_directory(),  "s2e_stdout.log"))], 
                    stdin = self._s2e_process.stdout, 
                    cwd = self._configuration.get_output_directory())
            self._s2e_stderr_tee_process = subprocess.Popen(
                    ["tee", os.path.normpath(os.path.join(self._configuration.get_output_directory(),  "s2e_stderr.log"))], 
                    stdin = self._s2e_process.stderr, 
                    cwd = self._configuration.get_output_directory())

            remote_target = False
            if "RemoteMemory" in self._configuration._s2e_configuration["plugins"]:
                remote_target= True

            if remote_target:
                self._remote_memory_interface = S2ERemoteMemoryInterface(self._configuration.get_remote_memory_listen_address())
                self._remote_memory_interface.set_read_handler(self._notify_read_request_handler)
                self._remote_memory_interface.set_write_handler(self._notify_write_request_handler)
                self._remote_memory_interface.set_set_cpu_state_handler(self._notify_set_cpu_state_handler)
                self._remote_memory_interface.set_get_cpu_state_handler(self._notify_get_cpu_state_handler)
                self._remote_memory_interface.set_continue_handler(self._notify_continue_handler)
                self._remote_memory_interface.set_get_checksum_handler(self._system.get_target().get_checksum)
                time.sleep(2) #Wait a bit for the S2E process to start
                self._remote_memory_interface.start()

            try:
                gdb_path = self._configuration._s2e_configuration["emulator_gdb_path"]
            except KeyError:
                gdb_path = "arm-none-eabi-gdb"
                log.warn("Using default gdb executable path: %s" % gdb_path)
                
            
            try:
                gdb_additional_args = self._configuration._s2e_configuration["emulator_gdb_additional_arguments"]
            except KeyError:
                gdb_additional_args = []

            self._gdb_interface = GdbDebugger(gdb_executable = gdb_path, cwd = ".", additional_args = gdb_additional_args)
            self._gdb_interface.set_async_message_handler(self.handle_gdb_async_message)
            count = 10
            while count != 0:
                try:
                    log.debug("Trying to connect to emulator.")
                    self._gdb_interface.connect(("tcp", "127.0.0.1", "%d" % self._configuration.get_s2e_gdb_port()))
                    break
                except:
                    count -= 1
                    if count > 0:
                        log.warning("Failed to connect to emulator, retrying.")
                    time.sleep(3)
            if count == 0:
                raise Exception("Failed to connect to emulator. Giving up!")
            log.info("Successfully connected to emulator.")
            self._is_s2e_running.set()
            self._s2e_process.wait()
        except KeyboardInterrupt:
            pass
            
        self.exit()

    def write_typed_memory(self, address, size, data):
        self._gdb_interface.write_memory(address, size, data)

    def read_typed_memory(self, address, size):
        return self._gdb_interface.read_memory(address, size)

    def set_register(self, reg, val):
        self._gdb_interface.set_register(reg, val)

    def get_register_from_nr(self, reg_nr):
        return self._gdb_interface.get_register_from_nr(reg_nr)

    def get_register(self, reg):
        return self._gdb_interface.get_register(reg)

    def set_breakpoint(self, address, **properties):
        if "thumb" in properties:
            del properties["thumb"]
        bkpt = self._gdb_interface.insert_breakpoint(address, *properties)
        return S2EBreakpoint(self._system, int(bkpt["bkpt"]["number"]))
    def execute_gdb_command(self, cmd):
        return self._gdb_interface.execute_gdb_command(cmd)
        
    
    def cont(self):
        self._gdb_interface.cont()
        
    def stepi(self):
        self._gdb_interface.stepi()

    def send_signal(self, signalnr):
        self._gdb_interface.send_signal(signalnr)

    def handle_gdb_async_message(self, msg):
        print("Received async message: '%s'" % str(msg))
        if msg.type == Async.EXEC:
            if msg.klass == "running":
                self.post_event({"tags": [EVENT_RUNNING], "channel": "gdb"})
            elif msg.klass == "stopped":
                if "reason" in msg.results and msg.results["reason"] == "breakpoint-hit":
                    self.post_event({"tags": [EVENT_STOPPED, EVENT_BREAKPOINT],
                                     "properties": {
                                        "address": int(msg.results["frame"]["addr"], 16),
                                        "bkpt_number": int(msg.results["bkptno"])},
                                     "channel": "gdb"})
                elif "reason" in msg.results and msg.results["reason"] == "end-stepping-range":
                    self.post_event({"tags": [EVENT_STOPPED, EVENT_END_STEPPING],
                                     "properties": {
                                        "address": int(msg.results["frame"]["addr"], 16)
                                        },
                                     "channel": "gdb"})
                # elif "signal-name" in msg.results and msg.results["signal-name"] == "SIGINT":
                #     self.post_event({"tags": [EVENT_STOPPED, EVENT_END_STEPPING],
                #                      "properties": {
                #                         "address": int(msg.results["frame"]["addr"], 16)
                #                         },
                #                      "channel": "gdb"})
                
    def post_event(self, evt):
        evt["source"] = "emulator"
        self._system.post_event(evt)
        

def init_s2e_emulator(system):
    system.set_emulator(S2EEmulator(system))
    
    
