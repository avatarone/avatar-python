'''
Created on Jun 26, 2013

@author: Jonas Zaddach <zaddach@eurecom.fr>
'''
import subprocess

def get_process_list():
    processes = []
    ps_output = subprocess.check_output(["ps", "-A", "-w", "-w", "-o", "pid,cmd"])
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