#!/usr/bin/env python3

import threading 
import select
import logging
import functools

log = logging.getLogger(__name__)

START_OF_MESSAGE_MARKER = ord('$')
END_OF_MESSAGE_MARKER = ord('#')

HEX_CHARACTERS = bytes('0123456789ABCDEFabcdef', encoding = 'ascii')

STATE_IDLE = 0
STATE_IN_MESSAGE = 1
STATE_IN_CHECKSUM_1 = 2
STATE_IN_CHECKSUM_2 = 3


class GdbLowlevelProtocol():
    """
        Encapsulates the communication between the avatar server and the 
        avatar stub.
    """
    

    
    def __init__(self, sock, received_message_handler, verbose=False):
        """
            Initialize the protocol.
            @param input_stream Stream from stub to avatar server.
            @param output_stream Stream from avatar server to stub.
        """
        self._verbose=verbose
        self._socket = sock
        self._received_message_handler = received_message_handler
        self._condition = threading.Condition()
        self._terminate = threading.Event()
        self._thread = threading.Thread(target = self.run)
        
        
    def start(self):
        self._thread.start()
        
    def run(self):
        state = STATE_IDLE
        message = []
        received_checksum = 0
        
        while not self._terminate.is_set():
            (rd, _, _) = select.select([self._socket], [], [], 1) 
            if not rd:
                continue
            
            data = self._socket.recv(1024)
            if not data:
                continue
            
            for next_byte in data:
                # if next_byte == "\x03": 
                #     state = STATE_IDLE
                #     message = []
                #     self._received_message_handler("ctrl-c")
                #     log.warn("TODO: recieved interrupt, ctrl-c not fully handeled yet... ")
                if next_byte == START_OF_MESSAGE_MARKER: 
                    state = STATE_IN_MESSAGE
                    message = []
                elif state == STATE_IN_MESSAGE and next_byte == END_OF_MESSAGE_MARKER:
                    state = STATE_IN_CHECKSUM_1
                elif state == STATE_IN_MESSAGE:
                    message.append(next_byte)
                elif state == STATE_IN_CHECKSUM_1:
                    assert(next_byte in HEX_CHARACTERS)
                    received_checksum = (int(chr(next_byte), 16) << 4) & 0xF0
                    state = STATE_IN_CHECKSUM_2
                elif state == STATE_IN_CHECKSUM_2:
                    assert(next_byte in HEX_CHARACTERS)
                    received_checksum |= int(chr(next_byte), 16) & 0x0F
                    state = STATE_IDLE
                    
                    #Verify checksum
                    calculated_checksum = functools.reduce(lambda r, x: (r + x) & 0xFF, message, 0)
                    if self._verbose:
                        print("Calculated checksum: %02x, received checksum: %02x" % (calculated_checksum, received_checksum))
                    if calculated_checksum == received_checksum:
                        self._socket.send(bytes('+', encoding = 'ascii'))
                        self._received_message_handler(bytes(message).decode('ascii'))
                    else:
                        self._socket.send(bytes('-', encoding = 'ascii'))
                
    def send_message(self, message):
        if self._verbose:
            print("Sending reply: '%s'" % message)
        raw_message = bytes(message, encoding = 'ascii')
        checksum = functools.reduce(lambda r, x: (r + x) & 0xFF, raw_message, 0)
        encoded_message = bytes('$', encoding = 'ascii') + \
            raw_message + \
            bytes('#%02x' % checksum, encoding = 'ascii')
        self._socket.send(encoded_message)
