'''
Created on May 2, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import super
from builtins import str
from future import standard_library
standard_library.install_aliases()
class AvatarException(Exception):
    def __init__(self, reason = None):
        self._reason = reason
        
    def __str__(self):
        if self._reason:
            return "Avatar exception: %s" % self._reason
        else:
            return "Avatar exception"
        
class AvatarUnimplementedException(AvatarException):
    def __init(self):
        super().__init__("Unimplemented")
        
class AvatarNotSupportedException(AvatarException):
    def __init__(self):
        super().__init__("Not supported")
        
class AvatarConnectException(AvatarException):
    def __init__(self, msg):
        super().__init__("Connect error: " + msg)
    
class AvatarRemoteError(AvatarException):
    def __init__(self, code):
        super().__init__("Stub error")
        self._code = code
        
    def __str__(self):
        return super.__str__() + " (error code 0x%02x)" % self._code
   
class AvatarProtocolUnknownOpcodeException(AvatarException):
    def __init__(self, opcode):
        self.opcode = opcode
        
    def __repr__(self):
        return "Error: Unknown opcode 0x%02x" % self.opcode

class AvatarProtocolOutOfDataException(AvatarException):
    def __init__(self, err):
        self.index_error = err
        
    def __repr__(self):
        return "Error: Invalid data index access (%s)" % (str(self.index_error))   