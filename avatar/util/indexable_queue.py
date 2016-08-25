from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from queue import Queue

class IndexableQueue(Queue):
    """ Taken from http://stackoverflow.com/questions/1293966/best-way-to-obtain-indexed-access-to-a-python-queue-thread-safe"""
    def __getitem__(self, index):
        with self.mutex:
            return self.queue[index]
        
    def find_and_remove(self, expr):
        with self.mutex:
            for item in self.queue:
                if expr(item):
                    self.queue.remove(item)
                    return item
        return None