class ProtocolError(Exception):
    def __init__(self, description):
        self.description = description
        
class ProtocolUnknownOpcodeError(ProtocolError):
    def __init__(self, opcode):
        self.opcode = opcode
        
    def __repr__(self):
        return "Error: Unknown opcode 0x%02x" % self.opcode

class ProtocolOutOfDataError(ProtocolError):
    def __init__(self, err):
        self.index_error = err
        
    def __repr__(self):
        return "Error: Invalid data index access (%s)" % (str(self.index_error))  