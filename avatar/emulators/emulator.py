from avatar.debuggable import Debuggable
from avatar.system import EVENT_REQUEST_WRITE_MEMORY_VALUE,\
    EVENT_REQUEST_READ_MEMORY_VALUE

class Emulator(Debuggable):
    def __init__(self, system):
        super(Emulator, self).__init__()
        self._system = system
        self._read_handler = None
        self._write_handler = None
        
    def set_read_request_handler(self, handler):
        self._read_handler = handler
        
    def set_write_request_handler(self, handler):
        self._write_handler = handler    

    def _notify_read_request_handler(self, params):
        self._system.post_event({"source": "emulator", 
                                 "tags": [EVENT_REQUEST_READ_MEMORY_VALUE],
                                 "properties": params})
        assert(self._read_handler) #Read handler must be set at this point
        
        return self._read_handler(params)
            
    def _notify_write_request_handler(self, params):
        self._system.post_event({"source": "emulator", 
                                 "tags": [EVENT_REQUEST_WRITE_MEMORY_VALUE],
                                 "properties": params})
        assert(self._write_handler) #Write handler must be set at this point
        
        return self._write_handler(params)       

