from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from builtins import object


class Crc8(object):
    """
        Implements the 1-wire CRC8 checksum.
        (The polynomial should be  X^8 + X^5 + X^4 + X^0)
    """
    R1 = [0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
          0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41]
    
    R2 = [0x00, 0x9d, 0x23, 0xbe, 0x46, 0xdb, 0x65, 0xf8,
          0x8c, 0x11, 0xaf, 0x32, 0xca, 0x57, 0xe9, 0x74]
    
    def __init__(self, data = None):
        self.crc = 0
        if data:
            self.add_bytes(data)
    
    def add_bytes(self, data):
        for byte in data:
            x = (byte ^ self.crc) & 0xFF
            self.crc = (self.R1[x & 0xF] ^ self.R2[(x >> 4) & 0xF]) & 0xFF
        return self.crc
    
    def get_crc(self):
        return self.crc
