from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import dict
from builtins import str
from builtins import bytes
from builtins import map
from builtins import zip
from future import standard_library
standard_library.install_aliases()
from builtins import object
import struct
from functools import reduce
from avatar.interfaces.avatar_stub.avatar_exceptions import AvatarProtocolUnknownOpcodeException
from avatar.interfaces.avatar_stub.avatar_exceptions import AvatarProtocolOutOfDataException

#Local imports

def to_uint32(data):
    return struct.unpack("<L", data[0:4])[0]

def to_uint16(data):
    return struct.unpack("<H", data[0:2])[0]

def to_uint8(data):
    return data[0]

def from_uint32(val):
    return struct.pack("<L", val)

def from_uint16(val):
    return struct.pack("<H", val)

def from_uint8(val):
    return bytes([val & 0xFF])

def from_size(val, size):
    if size == 1:
        return from_uint8(val)
    elif size == 2:
        return from_uint16(val)
    else:
        return from_uint32(val)

MESSAGE_DECLARATIONS = {
    0x01: {"name": "AVATAR_RPC_HTD_READ_MEMORY",
           "class_name": "AvatarReadMemoryMessage",
           "unpack": lambda data: { 
                "address": to_uint32(data[0:4]), 
                "size": to_uint8(data[4:5])},
           "pack": lambda obj: from_uint32(obj.address) + from_uint8(obj.size)},
    0x02: {"name": "AVATAR_RPC_HTD_WRITE_MEMORY",
           "class_name": "AvatarWriteMemoryMessage",
           "unpack": lambda data: { 
                "address": to_uint32(data[0:4]), 
                "size": to_uint8(data[4:5]), 
                "value": to_uint32(data[5:9])},
           "pack": lambda obj: from_uint32(obj.address) + \
                from_uint8(obj.size) + from_size(obj.value, obj.size)},
    0x03: {"name": "AVATAR_RPC_HTD_GET_REGISTER",
           "unpack": lambda data: { "register": to_uint8(data[0:1])},
           "pack": lambda obj: from_uint8(obj.register)},      
    0x04: {"name": "AVATAR_RPC_HTD_SET_REGISTER",
           "unpack": lambda data: { "register": to_uint8(data[0:1]), 
                                   "value": to_uint32(data[1:5])},
           "pack": lambda obj: from_uint8(obj.register) + from_uint32(obj.value)}, 
    0x05: {"name": "AVATAR_RPC_HTD_READ_UNTYPED_MEMORY",
           "class_name": "AvatarReadUntypedMemoryMessage",
           "unpack": lambda data: { 
                "address": to_uint32(data[0:4]), 
                "size": to_uint8(data[4:5])},
           "pack": lambda obj: from_uint32(obj.address) + from_uint8(obj.size)},
    0x06: {"name": "AVATAR_RPC_HTD_WRITE_UNTYPED_MEMORY",
           "class_name": "AvatarWriteUntypedMemoryMessage",
           "unpack": lambda data: { 
                "address": to_uint32(data[0:4]), 
                "size": to_uint8(data[4:5]), 
                "data": data[5:]},
           "pack": lambda obj: from_uint32(obj.address) + \
                from_uint8(len(obj.data)) + bytes(obj.data)},    
    0x07: {"name": "AVATAR_RPC_HTD_CODELET_EXECUTE",
           "class_name": "AvatarExecuteCodeletMessage",
           "unpack": lambda data: { 
                "address": to_uint32(data[0:4])},
           "pack": lambda obj: from_uint32(obj.address)},                                                      
    0x10: {"name": "AVATAR_RPC_HTD_INSERT_PAGE",
           "unpack": lambda data: { "page_address": to_uint32(data[0:4]), 
                                   "data": (data[5:5 + 64])},
           "pack": lambda obj: from_uint32(obj.page_address) + obj.data},
    0x11: {"name": "AVATAR_RPC_HTD_EXTRACT_PAGE",
           "unpack": lambda data: { "page_address": to_uint32(data[0:4])},
           "pack": lambda obj: from_uint32(obj.page_address)},
    0x12: {"name": "AVATAR_RPC_HTD_UNMAP_PAGE",
           "unpack": lambda data: { "page_address": to_uint32(data[0:4])},
           "pack": lambda obj: from_uint32(obj.page_address)},
    0x13: {"name": "AVATAR_RPC_HTD_SET_MEMORY_MAP",
           "unpack": lambda data: { "entries": map(lambda x: {
                    "start": to_uint32(x[0:4]), 
                    "end": to_uint32(x[4:8]),
                    "flags": to_uint32(x[8:12])}, 
                               map("".join, zip(*[iter(data[1:1 + 12 * ord(data[0])])] * 12)))},
           "pack": lambda obj: from_uint8(len(obj.entries)) + \
                    reduce(lambda r, x: r + from_uint32(x["start"]) + \
                    from_uint32(x["end"]) + from_uint32(x["flags"]), obj.entries, bytes())},
    0x14: {"name": "AVATAR_RPC_HTD_GET_DIRTY_PAGES",
           "unpack": lambda data: {},
           "pack": lambda obj: bytes()},
    0x20: {"name": "AVATAR_RPC_HTD_SET_EXCEPTION_CONFIG",
           "unpack": lambda data: { 
                    "config": to_uint32(data[0:4]), 
                    "irq_squelch" : to_uint16(data[4:6]), 
                    "fiq_squelch": to_uint16(data[6:8]),
                    "exceptions_vectors": [to_uint32("".join(x)) for x in 
                                           zip(*[iter(data[8:8 + 32])] * 4)]},
           "pack": lambda obj: to_uint32(obj.config) + \
                to_uint16(obj.irq_squelch) + \
                to_uint16(obj.fiq_squelch) + \
                reduce(lambda r, x: r + x, [to_uint32(x) for x in obj.exception_vectors], bytes())},
    0x21: {"name": "AVATAR_RPC_HTD_CLEAR_EXCEPTION",
           "unpack": lambda data: {"exception": to_uint8(data[0:4])},
           "pack": lambda obj: from_uint8(obj.exception)},
#     0x30: {"name": "AVATAR_RPC_HTD_ADD_EMULATED_INSTRUCTION",
#            "unpack": lambda data: {
#                 "identifier": to_uint32(data[0:4]), 
#                 "data": data[5: 5 + ord(data[4])]},
#            "pack": lambda obj: from_uint32(obj.identifier) + \
#                 from_uint8(len(obj.data)) + obj.data},
#     0x31: {"name": "AVATAR_RPC_HTD_CLEAR_EMULATED_INSTRUCTIONS",
#            "unpack": lambda data: {},
#            "pack": lambda obj: ""},
    0x40: {"name": "AVATAR_RPC_HTD_RESUME_VM",
           "unpack": lambda data: {},
           "pack": lambda obj: bytes()},
    0x41: {"name": "AVATAR_RPC_HTD_QUERY_STATE",
           "unpack": lambda data: {},
           "pack": lambda obj: bytes()},
    0x42: {"name": "AVATAR_RPC_HTD_CONTINUE_FROM_PAGEFAULT",
           "unpack": lambda data: {},
           "pack": lambda obj: ""},                   
    0xA0: {"name": "AVATAR_RPC_DTH_INFO_EXCEPTION",
           "unpack": lambda data: {"exception": to_uint8(data[0:1])},
           "pack": lambda obj: from_uint8(obj.exception)},
    0xA4: {"name": "AVATAR_RPC_DTH_PAGEFAULT",
           "unpack": lambda data: {"page_address": to_uint32(data[0:4])},
           "pack": lambda obj: from_uint32(obj.page_address)},                    
    0x81: {"name": "AVATAR_RPC_DTH_STATE",
           "unpack": lambda data: {"state": to_uint8(data[0:1])},
           "pack": lambda obj: from_uint8(obj.state)},
    0x82: {"name": "AVATAR_RPC_DTH_REPLY_STATE",
           "unpack": lambda data: {"state": to_uint8(data[0:1])},
           "pack": lambda obj: from_uint8(obj.state)},                    
    0xB1: {"name": "AVATAR_RPC_DTH_REPLY_OK",
           "unpack": lambda data: {},
           "pack": lambda obj: ""},
    0xB2: {"name": "AVATAR_RPC_DTH_REPLY_ERROR",
           "unpack": lambda data: {"error": to_uint8(data[0:1])},
           "pack": lambda obj: from_uint8(obj.error)},
    0x90: {"name": "AVATAR_RPC_DTH_REPLY_READ_MEMORY",
           "unpack": lambda data: {"value": to_uint32(data[1: 1 + data[0]] + bytes([0x00] * (4 - data[0])))},
           "pack": lambda obj: from_uint8(len(obj.data)) + obj.data},
    0x91: {"name": "AVATAR_RPC_DTH_REPLY_GET_REGISTER",
           "unpack": lambda data: {"value": to_uint32(data[0:4])},
           "pack": lambda obj: from_uint32(obj.value)},
    0x92: {"name": "AVATAR_RPC_DTH_REPLY_EXTRACT_PAGE",
           "unpack": lambda data: {"data": bytes(data[0:64])},
           "pack": lambda obj: obj.data},
    0x93: {"name": "AVATAR_RPC_DTH_REPLY_GET_DIRTY_PAGES",
           "unpack": lambda data: {"addresses": map(to_uint32, map("".join, zip(*[iter(data[1: 1 + 4 * data[0]])] * 4)))},
           "pack": lambda obj: bytes([len(obj.addresses)]) + reduce(lambda r, x: r + x, map(from_uint32, obj.addresses), bytes())},
    0x94: {"name": "AVATAR_RPC_DTH_REPLY_READ_UNTYPED_MEMORY",
           "unpack": lambda data: {"data": data},
           "pack": lambda obj: obj.data},
    0x95: {"name": "AVATAR_RPC_DTH_REPLY_CODELET_EXECUTION_FINISHED",
           "unpack": lambda data: {},
           "pack": lambda obj: bytes()}}

MESSAGE_NAME_DICTIONARY = dict([(x[1]["name"], dict(list(x[1].items()) + [("opcode", x[0])])) 
                                       for x in MESSAGE_DECLARATIONS.items()])

EXCEPTION_FLAGS = {
    0x00001: "AVATAR_EXCEPTION_IRQ_FORWARD",
    0x00002: "AVATAR_EXCEPTION_FIQ_FORWARD",
    0x00004: "AVATAR_EXCEPTION_SVC_FORWARD",
    0x00008: "AVATAR_EXCEPTION_UND_FORWARD",
    0x00010: "AVATAR_EXCEPTION_DAB_FORWARD",
    0x00020: "AVATAR_EXCEPTION_PAB_FORWARD",
    0x00100: "AVATAR_EXCEPTION_IRQ_PRINT",
    0x00200: "AVATAR_EXCEPTION_FIQ_PRINT",
    0x00400: "AVATAR_EXCEPTION_SVC_PRINT",
    0x00800: "AVATAR_EXCEPTION_UND_PRINT",
    0x01000: "AVATAR_EXCEPTION_DAB_PRINT",
    0x02000: "AVATAR_EXCEPTION_PAB_PRINT",
    0x10000: "AVATAR_EXCEPTION_INTERRUPTS_DISABLE"}

AVATAR_ERROR_NONE = 0
AVATAR_ERROR_CHECKSUM = 1
AVATAR_ERROR_OUT_OF_BOUNDS = 2
AVATAR_ERROR_NOT_FOUND = 3
AVATAR_ERROR_OUT_OF_MEMORY = 4

AVATAR_ERRORS = {
    AVATAR_ERROR_NONE: "AVATAR_ERROR_NONE", 
    AVATAR_ERROR_CHECKSUM: "AVATAR_ERROR_CHECKSUM",
    AVATAR_ERROR_OUT_OF_BOUNDS: "AVATAR_ERROR_OUT_OF_BOUNDS",
    AVATAR_ERROR_NOT_FOUND: "AVATAR_ERROR_NOT_FOUND",
    AVATAR_ERROR_OUT_OF_MEMORY: "AVATAR_ERROR_OUT_OF_MEMORY"}

AVATAR_VMSTATE_RUNNING =    0x01
AVATAR_VMSTATE_PAGE_MISS =  0x02
AVATAR_VMSTATE_BREAKPOINT = 0x04
AVATAR_VMSTATE_EMULATE_INSTRUCTION = 0x08
AVATAR_VMSTATE_EXCEPTION = 0x10

AVATAR_VM_STATE = {
    AVATAR_VMSTATE_RUNNING: "AVATAR_VMSTATE_RUNNING",
    AVATAR_VMSTATE_PAGE_MISS: "AVATAR_VMSTATE_PAGE_MISS",
    AVATAR_VMSTATE_BREAKPOINT: "AVATAR_VMSTATE_BREAKPOINT",
    AVATAR_VMSTATE_EMULATE_INSTRUCTION: "AVATAR_VMSTATE_EMULATE_INSTRUCTION",
    AVATAR_VMSTATE_EXCEPTION: "AVATAR_VMSTATE_EXCEPTION"}

AVATAR_ARM_REG_R0 = 0
AVATAR_ARM_REG_R1 = 1
AVATAR_ARM_REG_R2 = 2
AVATAR_ARM_REG_R3 = 3
AVATAR_ARM_REG_R4 = 4
AVATAR_ARM_REG_R5 = 5
AVATAR_ARM_REG_R6 = 6
AVATAR_ARM_REG_R7 = 7
AVATAR_ARM_REG_R8 = 8
AVATAR_ARM_REG_R9 = 9
AVATAR_ARM_REG_R10 = 10
AVATAR_ARM_REG_R11 = 11
AVATAR_ARM_REG_R12 = 12
AVATAR_ARM_REG_SP = 13
AVATAR_ARM_REG_LR = 14
AVATAR_ARM_REG_PC = 15
AVATAR_ARM_REG_CPSR = 16

MEMORY_MAP_FLAG_READ  = 1 #Memory region is readable
MEMORY_MAP_FLAG_WRITE = 2 #Memory region is writable
MEMORY_MAP_FLAG_EXECUTE = 4 #Memory region is executable
MEMORY_MAP_FLAG_PAGING = 8 #MEMORY_MAP_FLAG_PAGING signals that this memory region has an entry in the page table
MEMORY_MAP_FLAG_TRACE = 0x10 #Accesses to this memory region should be recorded in the memory access trace buffer

class AvatarMessage(object):
    """
        Base class for all avatar protocol messages.
        Abstract class, should not be instantiated directly.
    """
    def __init__(self, opcode):
        self.opcode = opcode & 0xFF
        self.fields = []
        
    def serialize(self):
        return bytes([self.opcode]) + self.packer(self)  
    
    def __str__(self):
        result = ["AvatarMessage(name = %s" % self.name]
        for field in self.fields:
            value = getattr(self, field)
            if isinstance(value, int):
                value = "0x%x" % value
            elif isinstance(value, bytes):
                value = "".join(["%02x" % x for x in value])
            result.append("%s = %s" % (field, str(value)))
            
        return ", ".join(result) + ")"
    
    def __repr(self):
        return self.__str__()
    
    
def parse_avatar_message(data):
    opcode = data[0]
    try:
        message_declaration = MESSAGE_DECLARATIONS[opcode]
    except KeyError:
        raise AvatarProtocolUnknownOpcodeException(opcode)
    
    try:
        fields = message_declaration["unpack"](data[1:])
        msg = AvatarMessage(opcode)
        
        for (field, value) in fields.items():
            setattr(msg, field, value)
            msg.fields.append(field)
         
        setattr(msg, "name", message_declaration["name"])   
        setattr(msg, "packer", message_declaration["pack"])
        return msg
    except IndexError as err:
        raise AvatarProtocolOutOfDataException(err)
    
    return None

def create_avatar_message(name, options):
    message_declaration = MESSAGE_NAME_DICTIONARY[name]
    msg = AvatarMessage(message_declaration["opcode"])
    
    for (field, value) in options.items():
        setattr(msg, field, value)
        msg.fields.append(field)
    
    setattr(msg, "name", message_declaration["name"])   
    setattr(msg, "packer", message_declaration["pack"])
    
    return msg
