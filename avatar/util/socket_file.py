from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from builtins import object
class SocketFile(object):
    def __init__(self, sock):
        self._sock = sock
        
    def read(self, num_bytes):
        return self._sock.recv(num_bytes)
        
    def write(self, data):
        if isinstance(data, str):
            raise Exception()
        self._sock.send(data)
        
    def fileno(self):
        return self._sock.fileno()
    
    def flush(self):
        pass
    
#     def readline(self):
#         data = self.read(1)
#         buffer = []
#         while data[0] != 0xA and data[0] != 0xD:
#             buffer.append(data[0])
#             data = self.read(1)
#             
#         buffer.append(data[0])
#         return bytes(buffer)