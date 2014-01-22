'''
Created on Jun 24, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''

from collections import defaultdict

class EmulatorTargetCallProxy():
    MONITOR_EVENTS = ["emulator_pre_read_request", 
                      "emulator_post_read_request",
                      "emulator_pre_write_request",
                      "emulator_post_write_request"]
    
    def __init__(self):
        self._target = None
        self._monitor_hooks = defaultdict(list)
        
    def set_target(self, target):
        self._target = target
        
    def add_monitor(self, monitor):
        for monitor_event in self.MONITOR_EVENTS:
            if hasattr(monitor, monitor_event):
                self._monitor_hooks[monitor_event].append(monitor)
                
    def remove_monitor(self, monitor):
        for (_, monitor_hooks) in self._monitor_hooks.items():
            try:
                monitor_hooks.remove(monitor)
            except ValueError:
                pass
        
    def handle_emulator_read_request(self, params):
        assert(self._target)
        
        for monitor in self._monitor_hooks["emulator_pre_read_request"]:
            monitor.emulator_pre_read_request(params)
            
        params["value"] = self._target.read_typed_memory(params["address"], params["size"])
        
        for monitor in self._monitor_hooks["emulator_post_read_request"]:
            monitor.emulator_post_read_request(params)
            
        return params["value"]
            
    def handle_emulator_write_request(self, params):
        assert(self._target)
        
        for monitor in self._monitor_hooks["emulator_pre_write_request"]:
            monitor.emulator_pre_write_request(params)
            
        self._target.write_typed_memory(params["address"], params["size"], params["value"])
        
        for monitor in self._monitor_hooks["emulator_post_write_request"]:
            monitor.emulator_post_write_request(params)

    def handle_emulator_set_cpu_state_request(self, params):
        # this function sets the CPU state on the target device
        assert(self._target)

        # TODO: fire events?

        for reg in params["cpu_state"]:
            if reg == "cpsr":
                # skip cpsr register
                continue
            value = int(params["cpu_state"][reg], 16)
            self._target.set_register(reg, value)

    def handle_emulator_continue_request(self, params):
        assert(self._target)

        self._target.cont()

