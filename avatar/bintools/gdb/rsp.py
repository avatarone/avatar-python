#! /usr/bin/env python
import socket
from threading import Thread
from queue import Queue
from logging import debug, info, basicConfig, DEBUG


class RemoteException(Exception):
    pass


class GdbRemoteSerialProtocol(Thread):
    def __init__(self, host='localhost', port=1212):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((host,port))

    def __chk(self,pkt):
        sum = 0
        for c in pkt:
            sum += ord(c)
        return sum & 0xff

    def __send(self, msg):
        debug('[GDB][TX] %s' % msg)
        self.socket.send(msg)

    def __recv(self, n):
        r = self.socket.recv(n)
        debug('[GDB][RX] %s' % r)
        return r

    def __expect_ack(self):
        reply = self.socket.recv(1)
        if reply != '+':
            raise RemoteException('Wrong ack: "%s"'% str(reply))

    def __send_ack(self):
        self.__send('+')

    def __send_msg(self, msg):
        self.__send('$%s#%02x' % (msg, self.__chk(msg)))
        self.__expect_ack()

    def __recv_msg(self):
        c = self.socket.recv(1)
        if c != '$':
            raise RemoteException('Expected "$" received: "%s"' % str(c))
        
        msg = ''
        while True:
            c = self.socket.recv(1)
            if c == '#': break
            msg += c
        
        chk = int(self.socket.recv(2), 16)
        if chk != self.__chk(msg):
            raise RemoteException('Wrong checksum')
        debug('[GDB][RX] $%s#%02x' % (msg, chk))
        
        self.__send_ack()
        
        return msg

    def __del__(self):
        try:
            self.close()
        except:
            # close() most likely already called
            pass

    def close(self):
        self.__send_msg('k')
        self.socket.close()

    def cont(self, addr=None):
        pkt = 'c'
        if addr != None:
            pkt += '%x' % (addr)
        self.__send_msg(pkt)

    def step(self, addr=None):
        pkt = 's'
        if addr != None:
            pkt += '%x' % (addr)
        self.__send_msg(pkt)

    def __z_packet(self, pkt):
        self.__send_msg(pkt)
        reply = self.__recv_msg()
        if reply == '':
            info('Z packets are not supported by target.')
        else:
            if (reply != 'OK'):
                raise RemoteException('Unexpected reply: %s' % str(reply))

    def break_insert(self, addr, _len=0, _type=0):
        self.__z_packet('Z%d,%x,%x' % (_type, addr, _len))

    def break_remove(self, addr, _len=0, _type=0):
        self.__z_packet('z%d,%x,%x' % (_type, addr, _len))

    def expect_signal(self):
        msg = self.__recv_msg()
        assert len(msg) == 3 and msg[0] == 'S', 'Expected "S", received "%c" % msg[0]'
        
        return int(msg[1:], 16)


if __name__ == '__main__':
    basicConfig(level=DEBUG, format='%(levelname)-8s %(message)s')
    remote = GdbRemoteSerialProtocol()
    
