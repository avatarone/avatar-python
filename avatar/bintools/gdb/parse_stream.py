from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from builtins import object
class ParseError(Exception):
    pass


class ParseStreamError(ParseError):
    def __init__(self, msg, s):
        ParseError.__init__(self, '%s: %s@%d' % (msg, s.getvalue(), s.pos))


class ParseStream(object):
    def __init__(self, string):
        self._string = string
        self._pos = 0
    
    def char(self):
        return self.read(1)
    
    def read(self, num):
        data = self._string[self._pos:self._pos + num]
        self._pos += num
        return data
    
    def seek(self, pos):
        assert(pos >= 0 and pos < len(self._string))
        self._pos = pos
    
    def back(self, n=1):
        self.seek(self._pos - n)
    
    def skip(self, n=1):
        self.seek(self._pos + n)
    
    def peek(self):
        c = self.read(1)
        self.back()
        return c
    
    def next_char(self, rc):
        if self._pos + 1 >= len(self._string):
            return False
        c = self.read(1)
        if c != rc:
            self.back()
            return False
        return True
    
    def expect_char(self, expected):
        c = self.read(1)
        if c != expected:
            raise ParseStreamError('Expected "%c", got "%c"' % (expected, c), self)
    
    def check_limit(self):
        if self._pos >= len(self._string):
            raise ParseStreamError('Unexpected end of string', self)
        return True
