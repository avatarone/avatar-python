from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from builtins import object
class AvatarPlugin(object):
    """
    Abstract interface for all Avatar plugins

    Upon start() and stop(), plugins are expected to register/unregister
    their own event handlers by the means of :func:`System.register_event_listener`
    and :func:`System.unregister_event_listener`
    """

    def __init__(self, system):
        self._system = system
        
    def init(self, **kwargs):
        assert(False) #Not implemented
        
    def start(self, **kwargs):
        assert(False) #Not implemented

    def stop(self, **kwargs):
        assert(False) #Not implemented
