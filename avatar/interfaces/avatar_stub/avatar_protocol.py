import threading
import logging
from queue import Queue, Empty
from avatar.util.reference import Reference
from avatar.interfaces.avatar_stub.avatar_protocol_lowlevel import AvatarLowlevelProtocol
from avatar.interfaces.avatar_stub.avatar_messages import create_avatar_message
from avatar.interfaces.avatar_stub.avatar_exceptions import AvatarRemoteError

    
log = logging.getLogger(__name__)
        
    
ASYNCHRONOUS_MESSAGES = ["AVATAR_RPC_DTH_STATE", "AVATAR_RPC_DTH_PAGEFAULT", "AVATAR_RPC_DTH_INFO_EXCEPTION"]
RESPONSE_TIMEOUT = 10

class AvatarProtocol():
    CONNECT_TIMEOUT = 10
    def __init__(self, sock, paging_handler = lambda x: None):
        #TODO: Put a meaningful timeout
        self._socket = sock
        self._protocol = AvatarLowlevelProtocol(self._socket, self._handle_received_message)

        self._terminate = threading.Event()
        self._state = None
        self._asynchronous_messages_handler = paging_handler
        self._queued_commands = Queue()
        self._received_synchronous_messages = Queue()
        self._send_thread = threading.Thread(target = self.run_send)
        self._send_lock = threading.Lock()
        self._send_thread.start()
        
    def _handle_received_message(self, msg): 
        if msg.name in ASYNCHRONOUS_MESSAGES:
            self.handle_asynchronous_message(msg)
        else:
            self._received_synchronous_messages.put(msg)
                
    def run_send(self):
        while not self._terminate.is_set():
            result = None
            try:
                result = self._queued_commands.get(timeout = 1)
            except Empty:
                continue
            
            if not result:
                continue
            
            self._send_lock.acquire()
            self._protocol.send_message(result[0])
            self._send_lock.release()
            
            if result[1]:
                log.debug("Waiting for response to message %s", result[0].name)
                recv_msg = self._received_synchronous_messages.get(timeout = RESPONSE_TIMEOUT)
                if recv_msg and recv_msg.name in result[1]:
                    if result[2]:
                        result[2].acquire()
                        if result[3]:
                            result[3].set_value(recv_msg)
                        result[2].notify()
                        result[2].release()
                else:
                    log.warn("Unexpected message received: %s", recv_msg.name)
                
    def handle_asynchronous_message(self, msg):
        if msg.name == "AVATAR_RPC_DTH_PAGEFAULT":
            page_data = self._page_fault_handler(msg.page_address)
            self.insert_page(msg.page_address, page_data)
            self._send_lock.acquire()
            self._protocol.send_message(create_avatar_message("AVATAR_RPC_HTD_CONTINUE_FROM_PAGEFAULT", {}))
            self._send_lock.release()
        elif msg.name == "AVATAR_RPC_DTH_STATE":
            self._state = msg.state
            
        elif msg.name == "AVATAR_RPC_DTH_INFO_EXCEPTION":
            self._exception_handler(msg.exception)
        self._asynchronous_messages_handler(msg)

    def set_register(self, register, value):
        msg = create_avatar_message("AVATAR_RPC_HTD_SET_REGISTER", {"register": register, "value": value})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_OK",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        
    def get_register(self, register):
        msg = create_avatar_message("AVATAR_RPC_HTD_GET_REGISTER", {"register": register})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_GET_REGISTER",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        return ref.get_value().value
        
    def read_memory(self, address, size):
        msg = create_avatar_message("AVATAR_RPC_HTD_READ_MEMORY", {"address": address, "size": size})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_READ_MEMORY",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        
        return ref.get_value().value
        
    def write_memory(self, address, size, value):
        msg = create_avatar_message("AVATAR_RPC_HTD_WRITE_MEMORY", {"address": address, "size": size, "value": value})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_OK",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        
    def read_memory_untyped(self, address, size):
        assert(size <= 255)
        msg = create_avatar_message("AVATAR_RPC_HTD_READ_UNTYPED_MEMORY", {"address": address, "size": size})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_READ_UNTYPED_MEMORY",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        
        return ref.get_value().data
        
    def write_memory_untyped(self, address, data):
        assert(len(data) <= 255)
        assert(isinstance(data, bytes))
        msg = create_avatar_message("AVATAR_RPC_HTD_WRITE_UNTYPED_MEMORY", {"address": address, "data": data})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_OK",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        
    def execute_codelet(self, address):
        msg = create_avatar_message("AVATAR_RPC_HTD_CODELET_EXECUTE", {"address": address})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_CODELET_EXECUTION_FINISHED",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        
    def cont(self):   
        msg = create_avatar_message("AVATAR_RPC_HTD_RESUME_VM", {})
        expected_replies = ["AVATAR_RPC_DTH_REPLY_OK",  "AVATAR_RPC_DTH_REPLY_ERROR"]
        cv = threading.Condition()
        ref = Reference()
        
        cv.acquire()
        self._queued_commands.put((msg, expected_replies, cv, ref))
        cv.wait()
        cv.release()
        if ref.get_value().name == "AVATAR_RPC_DTH_REPLY_ERROR":
            raise AvatarRemoteError(ref.get_value().error)
        
    def stop(self):
        self._terminate.set()
        self._protocol.stop()
                
