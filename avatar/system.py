#from configuration.manager import ConfigurationManager
#from interfaces.s2e_remote_memory import S2ERemoteMemoryInterface
#from interfaces.avatar_protocol import AvatarProtocol
#from util.socket_file import SocketFile
#from exceptions import OSError
#import signal
#import subprocess
#import os
#import atexit
#import sys
#import socket
#import threading
#import logging

from threading import Event, Thread
import logging
import sys
import os
from avatar.util.ostools import mkdir_p
from avatar.call_proxy import EmulatorTargetCallProxy
from queue import Empty, Queue



CONSOLE_LOG_FORMAT = "%(levelname)s - %(message)s"
FILE_LOG_FORMAT = '%(asctime)s - %(levelname)s - %(message)s'
#log = logging.getLogger(__module__ + "." + __name__)
log = logging.getLogger(__name__)

#Event tags
EVENT_STOPPED = "EVENT_STOPPED"
EVENT_BREAKPOINT = "EVENT_BREAKPOINT"
EVENT_END_STEPPING = "EVENT_END_STEPPING"
EVENT_RUNNING = "EVENT_RUNNING"
EVENT_REQUEST_READ_MEMORY_VALUE = "EVENT_REQUEST_READ_MEMORY_VALUE"
EVENT_REQUEST_WRITE_MEMORY_VALUE = "EVENT_REQUEST_WRITE_MEMORY_VALUE"
EVENT_RESPONSE_READ_MEMORY_VALUE = "EVENT_RESPONSE_READ_MEMORY_VALUE"
EVENT_RESPONSE_WRITE_MEMORY_VALUE = "EVENT_RESPONSE_WRITE_MEMORY_VALUE"
EVENT_SIGABRT = "EVENT_SIGABRT"

class EventWaiter():
    def __init__(self, system):
        self._queue = Queue()
        self._system = system
        system.register_event_listener(self._queue_event)
        
    def _queue_event(self, evt):
        self._queue.put(evt)
        
    def wait_event(self):
        return self._queue.get()
    
    def __del__(self):
        self._system.unregister_event_listener(self._queue_event)

class System():
    def __init__(self, configuration, create_emulator, create_target):
        self._configuration = configuration
        self._plugins = []
        self._terminating = Event()
        self._call_proxy = EmulatorTargetCallProxy()
        self._emulator = None
        self._target = None
        self._listeners = []
        self._events = Queue()
        
        
        #Setup logging to console
#         logging.basicConfig(level = logging.DEBUG, format = CONSOLE_LOG_FORMAT)
#        console_log_handler = logging.StreamHandler()
#        console_log_handler.setLevel(logging.DEBUG)
#        console_log_handler.setFormatter(logging.Formatter(CONSOLE_LOG_FORMAT))
#        logging.getLogger("").addHandler(console_log_handler)
 #       logging.getLogger("").setLevel(logging.DEBUG)
        
        #Check if output directory exists, and create it if not
        output_directory = configuration["output_directory"]
        if os.path.exists(output_directory) and not os.path.isdir(output_directory):
            log.error("Output destination exists, but is not a directory")
            sys.exit(1)
            
        if not os.path.exists(output_directory):
            log.info("Output directory did not exist, trying to create it")
            mkdir_p(output_directory)
            
        if os.listdir(output_directory):
            log.warn("Output directory is not empty, will overwrite files")
            
        #Now the output directory should exist, divert a logging stream there
        file_log_handler = logging.FileHandler(filename = os.path.join(output_directory, "avatar.log"), mode = 'w')
        file_log_handler.setLevel(logging.DEBUG)
        file_log_handler.setFormatter(logging.Formatter(FILE_LOG_FORMAT))
        logging.getLogger("").addHandler(file_log_handler)
 #       console_log_handler.setLevel(logging.INFO)
        
        self._event_thread = Thread(target = self._process_events)
        self._event_thread.start()
        
        create_emulator(self)
        create_target(self)
        
        
    def get_configuration(self):
        return self._configuration
        
    def init(self):
        self._emulator.init()
        self._target.init()
        
    def start(self):
        assert(self._emulator) #Start emulator hook needs to be set!
        assert(self._target) #Start target hook needs to be set!
        
        self._emulator.set_read_request_handler(self._call_proxy.handle_emulator_read_request)
        self._emulator.set_write_request_handler(self._call_proxy.handle_emulator_write_request)
        self._emulator.set_set_cpu_state_request_handler(self._call_proxy.handle_emulator_set_cpu_state_request)
        self._emulator.set_get_cpu_state_request_handler(self._call_proxy.handle_emulator_get_cpu_state_request)
        self._emulator.set_continue_request_handler(self._call_proxy.handle_emulator_continue_request)
        self._emulator.set_get_checksum_request_handler(self._call_proxy.handle_emulator_get_checksum_request)
        self._call_proxy.set_target(self._target)
        
        self._target.start()
        self._emulator.start()
        
        
    def stop(self):
        if not self._terminating.is_set():
            self._terminating.set()
            self._emulator.stop()
            self._target.stop()
            self._call_proxy.stop_monitors()
        
    def set_emulator(self, emulator):
        """This method is supposed to be called by the emulator init function
           when it sets the actual emulator object. The _emulator variable should
           not be set directly, as some decoration might happen."""
        self._emulator = emulator
        
    def set_target(self, target):
        """This method is supposed to be called by the target init function
           when it sets the actual target object. The _target variable should
           not be set directly, as some decoration might happen."""
        self._target = target
        
    def get_emulator(self):
        return self._emulator
    
    def get_target(self):
        return self._target
        
    def add_monitor(self, monitor):
        self._call_proxy.add_monitor(monitor)
        
    def post_event(self, evt):
        if not "properties" in evt:
            evt["properties"] = {}
        self._events.put(evt)
        
    def register_event_listener(self, listener):
        self._listeners.append(listener)
        
    def unregister_event_listener(self, listener):
        self._listeners.remove(listener)
        
    def _process_events(self):
        while not self._terminating.is_set():
            try:
                evt = self._events.get(1)
                log.debug("Processing event: '%s'", str(evt))
                for listener in self._listeners:
                    try:
                        listener(evt)
                    except Exception:
                        log.exception("Exception while handling event")
            except Empty:
                pass
            except Exception as ex:
                log.exception("Some more serious exception handled while processing events. Investigate.")
                raise ex
        
        
