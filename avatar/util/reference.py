'''
Created on May 2, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from builtins import object
class Reference(object):
    def __init__(self, obj = None):
        self._obj = obj
        
    def set_value(self, obj):
        self._obj = obj
        
    def get_value(self):
        return self._obj