'''
Created on May 2, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
class Reference():
    def __init__(self, obj = None):
        self._obj = obj
        
    def set_value(self, obj):
        self._obj = obj
        
    def get_value(self):
        return self._obj