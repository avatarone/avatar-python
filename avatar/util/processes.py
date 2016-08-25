'''
Created on Jun 26, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
from __future__ import unicode_literals
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from builtins import int
from future import standard_library
standard_library.install_aliases()
import subprocess

def get_process_list():
    processes = []
    ps_output = subprocess.check_output(["ps", "-A", "-w", "-w", "-o", "pid", "-o", "command"])
    ps_output = ps_output.decode('latin-1')
    for line in ps_output.split("\n")[1:]:
        line = line.strip()
        if not line:
            continue
        pid = line[:line.find(" ")]
        cmd = line[line.find(" ") + 1:]
        processes.append({"pid": int(pid), "cmd": cmd})
        
    return processes

def find_processes(name):
    process_list = get_process_list()
    found_processes = []
    
    for process in process_list:
        if process["cmd"].startswith(name):
            found_processes.append(process)
    return found_processes