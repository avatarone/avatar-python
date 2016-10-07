class AvatarPlugin:
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
