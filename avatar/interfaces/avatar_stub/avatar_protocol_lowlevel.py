from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import str
from builtins import bytes
from future import standard_library
standard_library.install_aliases()
from builtins import object
#!/usr/bin/env python3

import threading 
from select import select
import time
from avatar.util.indexable_queue import IndexableQueue
from avatar.util.checksum import Crc8
import logging
from functools import reduce
from avatar.interfaces.avatar_stub.avatar_messages import parse_avatar_message

log = logging.getLogger(__name__)

class AvatarLowlevelProtocol(object):
    """
        Encapsulates the communication between the avatar server and the 
        avatar stub.
    """
    

    
    def __init__(self, sock, message_handler):
        """
            Initialize the protocol.
            @param input_stream Stream from stub to avatar server.
            @param output_stream Stream from avatar server to stub.
        """
        self._socket = sock
        self.input_buffer = []
        self.condition = threading.Condition()
        self.messages = IndexableQueue()
        self.terminate = threading.Event()
        self.thread = threading.Thread(target = self.run)
        self.thread.start()
        self._received_message_handler = message_handler
        
    def run(self):
        while not self.terminate.is_set():
            (rd, _, _) = select([self._socket], [], [], 1) 
            if not rd:
                continue
            
            next_byte = self._socket.recv(1)
            
            if next_byte[0] == 0x55: #End message marker
                joined_buffer = reduce(lambda r, x: r + x, self.input_buffer, bytes())
                log.debug("Received raw message %s", "".join(
                    ["%02x" % x for x in joined_buffer]))
                self.parse_message(joined_buffer)
                self.input_buffer = []
            else:
                if next_byte[0] == 0xAA:
                    #There has to be a following byte for the escape byte
                    #TODO: do something to prevent program from hanging here
                    # if input is not protocol conformant
                    next_next_byte = self._socket.recv(1)
                    if next_next_byte[0] == 0x01:
                        next_byte = bytes([0x55])
                    elif next_next_byte[0] == 0x02:
                        next_byte = bytes([0xAA])
                    else:
                        #TODO: Error, escape sequence unknown
                        pass
                self.input_buffer.append(next_byte)
                
    def send_message(self, avatar_message):
        serialized_message = avatar_message.serialize()
        
        log.info("Sending message %s" % str(avatar_message))
        log.debug("Sending serialized message %s", "".join(["%02x" % x for x in serialized_message]))
        escaped_message = serialized_message.replace(bytes([0xAA]), bytes([0xAA, 0x02])).  \
                    replace(bytes([0x55]), bytes([0xAA, 0x01]))
        crc = Crc8(serialized_message).get_crc()     
        escaped_crc = bytes([crc & 0xFF]).replace(bytes([0xAA]), bytes([0xAA, 0x02])).  \
                    replace(bytes([0x55]), bytes([0xAA, 0x01]))
        raw_message = escaped_message + escaped_crc + bytes([0x55])
        self._socket.send(raw_message)
        log.debug("Sending raw message %s", "".join(["%02x" % x for x in raw_message]))
    
    def parse_message(self, data):
        if not data:
            return
        message_crc = data[-1]
        calculated_crc = Crc8(data[:-1]).get_crc()
        
        if message_crc != calculated_crc:
            log.warn("Message 0x%02x CRC 0x%02x != calculated CRC 0x%02x", \
                     data[0], message_crc, calculated_crc)
            return
        
        try:
            message = parse_avatar_message(data)
            if message:
                log.info("Received message %s", str(message))
                self._received_message_handler(message)
#                 self.messages.put(message)
#                 self.condition.acquire()
#                 self.condition.notify_all()
#                 self.condition.release()
        except Exception:
            log.exception("Error parsing received message")
            
#     def receive_message_blocking(self, timeout = -1, message_type = None):
#         if message_type:
#             start_time = time.time()
#             
#             self.condition.acquire()
#             while True:
#                 msg = self.messages.find_and_remove(
#                             lambda x: x.name == message_type)
#                 if msg:
#                     self.condition.release()
#                     return msg
#                 
#                 if timeout != -1 and time.time() - start_time >= timeout:
#                     self.condition.release()
#                     return None
#                 
#                 self.condition.wait(timeout)
#         else:
#             return self.messages.get(timeout)
        
    def stop(self):
        self.terminate.set()