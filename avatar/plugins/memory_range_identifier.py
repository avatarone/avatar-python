from avatar.plugins.avatar_plugin import AvatarPlugin
import logging
from avatar.system import EVENT_REQUEST_WRITE_MEMORY_VALUE,\
    EVENT_REQUEST_READ_MEMORY_VALUE
from functools import reduce

log = logging.getLogger("avatar")



class MemoryRangeIdentifier(AvatarPlugin):
    def __init__(self, system, page_size = 512):
        super(self, AvatarPlugin).__init__(system)
        self._page_size = page_size
        self._pages = {}
        
    def init(self):
        pass
        
    def start(self):
        pass
        
    def _get_page(self, address):
        pg_address = int(address / self._page_size) * self._page_size
        try:
            return self._pages[pg_address]
        except KeyError:
            self.pages[pg_address] = {"read": [0] * self._page_size, 
                                      "write": [0] * self._page_size, 
                                      "execute": [0] * self._page_size, 
                                      "stack": [0] * self._page_size, 
                                      "io": [0] * self._page_size,
                                      "data": [-1] * self._page_size}
            log.debug("Added page for address: 0x%x", pg_address)
            return self._pages[pg_address]
            
    def write_data(self, address, size, value):
        assert(size == 1 or size == 2 or size == 4 or size == 8)
        assert(int(address / self.PAGE_SIZE) == int((address + size - 1) / self.PAGE_SIZE))

        page = self.get_page(address)
        offset = address % self.PAGE_SIZE
        for i in range(0, size):
            page["data"][offset + i] = (value >> (8 * i)) & 0xFF

    def _read_cached_data(self, address, size):
        assert(size == 1 or size == 2 or size == 4 or size == 8)
        assert(int(address / self.PAGE_SIZE) == int((address + size - 1) / self._page_size))

        page = self._get_page(address)
        offset = address % self._page_size
        value = 0
        #TODO: Only works for little endian
        for i in range(0, size):
            if page["data"][offset + i] < 0:
                #Memory has not yet been written to
                return None
            value = value | (page["data"][offset + i] << (8 * i))

        return value

    def _mark_data(self, address, size, mark):
        assert(size == 1 or size == 2 or size == 4 or size == 8)
        assert(int(address / self.PAGE_SIZE) == int((address + size - 1) / self.PAGE_SIZE))
        assert(mark in ["read", "write", "execute", "stack", "io"])

        page = self._get_page(address)
        offset = address % self.PAGE_SIZE
        for i in range(offset, offset + size):
            page[mark][i] += 1

    def _mark_data_read(self, address, size):
        self._mark_data(address, size, "read")

    def _mark_data_write(self, address, size):
        self._mark_data(address, size, "write")

    def _mark_data_execute(self, address, size):
        self._mark_data(address, size, "execute")

    def _mark_data_stack(self, address, size):
        self._mark_data(address, size, "stack")  

    def _mark_data_io(self, address, size):
        self._mark_data(address, size, "io")  

    def _add_stack_pointer_value(self, sp, cpsr):
#        self.access_log.append({"type": "sp", "sp": sp, "cpsr": cpsr})
        self._mark_data_stack(sp, 4)

    def _add_program_counter_value(self, pc, cpsr):
#        self.access_log.append({"type": "pc", "pc": pc, "cpsr": cpsr})
        if cpsr & (1 << 5):
            self._mark_data_execute(pc, 2)
        else:
            self._mark_data_execute(pc, 4)

    def _add_memory_read_access(self, address, size, value, cpsr):
#        evt = {"type": "read", "address": address, "size": size, "value": value, "cpsr": cpsr}
#        self.access_log_file.write(json.dumps(evt) + "\n")
#        self.access_log.append(evt)
        stored_value = self.read_data(address, size)

        if (not stored_value is None) and stored_value != value:
            log.debug("stored_value = 0x%08x, value = 0x%08x" % (stored_value, value))
            self._mark_data_io(address, size)
        self._write_data(address, size, value)

        self._mark_data_read(address, size)

    def _add_memory_write_access(self, address, size, value, cpsr):
#        evt = {"type": "write", "address": address, "size": size, "value": value, "cpsr": cpsr}
#        self.access_log_file.write(json.dumps(evt) + "\n")
#        self.access_log.append(evt)
        self._mark_data_write(address, size)
        self._write_data(address, size, value)

    def get_pageinfo(self, path):
        pageinfo = []
        for (pg_address, page) in sorted(self._pages.items(), key = operator.itemgetter(0)):
            pageinfo.append({"address": pg_address, 
                             "read": sum(page["read"]),
                             "write": sum(page["write"]),
                             "execute": sum(page["execute"]),
                             "stack": sum(page["stack"]),
                             "io": sum(page["io"])})
                             
        return pageinfo

    def _process_event(self, evt):
        if evt["source"] == "emulator":
            memory_access = False
            if EVENT_REQUEST_WRITE_MEMORY_VALUE in evt["tags"]:
                memory_access = True
                self._add_memory_write_access(evt["properties"]["address"], 
                                              evt["properties"]["size"], 
                                              evt["properties"]["value"],
                                              int(evt["properties"]["cpu_state"]["cpsr"], 16))
            elif EVENT_REQUEST_READ_MEMORY_VALUE in evt["tags"]:
                self._add_memory_read_access(evt["properties"]["address"], 
                                             evt["properties"]["size"], 
                                             int(evt["properties"]["cpu_state"]["cpsr"], 16))
                memory_access = True
            if (memory_access):
                self._add_stack_pointer_value(int(evt["properties"]["cpu_state"]["sp"], 16),
                                              int(evt["properties"]["cpu_state"]["cpsr"], 16))
                self._add_program_counter_value(int(evt["properties"]["cpu_state"]["pc"], 16),
                                                int(evt["properties"]["cpu_state"]["cpsr"], 16))
        
