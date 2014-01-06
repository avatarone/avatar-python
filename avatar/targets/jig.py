class Jig():
    """This is an interface for all objects that acts as hardware jig (ie. gdb monitors)"""
    def attach(self):
        """Attach to target board"""
        assert(False) #No implementation
    
    def detach(self, address, size, value):
        """Detach from target board"""
        assert(False) #No implementation
    
    def get_gdb_jigstr(self):
        """Return a socket endpoint string for a Gdbserver"""
        assert(False) #No implementation
        
    def get_gdb_jigosck(self):
        """Return a socket endpoint string for a Gdbserver"""
        assert(False) #No implementation

    def get_telnet_jigstr(self):
      """Return a socket endpoint string for a Gdbserver"""
      assert(False) #No implementation
        
    def get_telnet_jigosck(self):
        """Return a socket endpoint string for a Gdbserver"""
        assert(False) #No implementation
