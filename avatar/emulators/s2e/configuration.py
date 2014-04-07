import os
import shutil
import json
import tempfile
import logging
from avatar.util.ostools import get_random_free_port
from collections import OrderedDict

log = logging.getLogger(__name__)

class S2EConfiguration():
    def __init__(self, config):
        assert("s2e" in config) #S2E configuration must be present
        assert("machine_configuration" in config) #Configurable machine configuration must be present
        
        self._s2e_configuration = config["s2e"]
        self._cm_configuration = config["machine_configuration"]
        self._output_directory = config["output_directory"]
        self._config_directory = config["configuration_directory"]
        self._avatar_configuration = "avatar_configuration" in config and config["avatar_configuration"] or {}
        self._qemu_configuration = ("qemu_configuration" in config) and config["qemu_configuration"] or {}
        
        mem_addr = "127.0.0.1"
        mem_port = get_random_free_port()
        if not isinstance(self._s2e_configuration["plugins"],OrderedDict):
            log.warn("plugins dictionnary should be ordered (use OrderedDict), s2e should take care of ordering plugins one day !")
        if "RemoteMemory" in self._s2e_configuration["plugins"] \
          and "listen_address" in self._s2e_configuration["plugins"]["RemoteMemory"] \
          and self._s2e_configuration["plugins"]["RemoteMemory"]["listen_address"]:
            listen_addr = self._s2e_configuration["plugins"]["RemoteMemory"]["listen_address"]
            mem_addr = str(listen_addr[:listen_addr.rfind(":")])
            mem_port = int(listen_addr[listen_addr.rfind(":") + 1:])
        self._s2e_remote_memory_plugin_sockaddr = (mem_addr, mem_port)
        
        #TODO: Test if this is specified in configuration, and use values from config if so
        self._s2e_gdb_sockaddr = ("127.0.0.1", get_random_free_port())
        

    def get_klee_cmdline(self):
        cmdline = []
        
        if "klee" in self._s2e_configuration:
            klee_conf = self._s2e_configuration["klee"]
            cmdline.append("--use-batching-search=%s" % (("use-batching-search" in klee_conf and klee_conf["use-batching-search"]) and "true" or "false"))
            if "batch-time" in klee_conf:
                cmdline.append("--batch-time=%f" % klee_conf["batch-time"])
            if "use-random-path" in klee_conf and klee_conf["use-random-path"]:
                cmdline.append("--use-random-path")
            cmdline.append("--use-cex-cache=%s" % (("use-cex-cache" in klee_conf and klee_conf["use-cex-cache"]) and "true" or "false"))
            cmdline.append("--use-cache=%s" % (("use-cache" in klee_conf and klee_conf["use-cache"]) and "true" or "false"))
            cmdline.append("--use-fast-cex-solver=%s" % (("use-fast-cex-solver" in klee_conf and klee_conf["use-fast-cex-solver"]) and "true" or "false"))
            if "max-stp-time" in klee_conf:
                cmdline.append("--max-stp-time=%f" % klee_conf["max-stp-time"])
            cmdline.append("--use-expr-simplifier=%s" % (("use-expr-simplifier" in klee_conf and klee_conf["use-expr-simplifier"]) and "true" or "false"))
            cmdline.append("--use-concolic-execution=%s" % (("use-concolic-execution" in klee_conf and klee_conf["use-concolic-execution"]) and "true" or "false"))
            cmdline.append("--print-mode-switch=true")
            cmdline.append("--concretize-io-address=false")
            cmdline.append("--concretize-io-writes=true")
            cmdline.append("--allow-external-sym-calls=false")
            cmdline.append("--verbose-fork-info=true")

        return cmdline
        
        
    def get_s2e_lua(self):
        lua = []
        lua.append("-- Automatically generated Lua script configuration for S2E\n")
        lua.append("-- Do not edit!\n")
        lua.append("\n")
        lua.append("AVATAR_SRC_ROOT_PATH = \"%s\"\n" % self._config_directory)
        lua.append("s2e = {\n")
        lua.append("generate_testcase_on_kill = %s," % (("generate_testcase_on_kill" not in self._s2e_configuration \
                                                        or self._s2e_configuration["generate_testcase_on_kill"]) and "true" or "false"))        
        # First klee configuration
        lua.append("\tkleeArgs = {\n\t\t")
        lua.append(",\n\t\t".join(["\"%s\"" % x for x in self.get_klee_cmdline()]))
        lua.append("\n\t}")
        lua.append("\n}")
        
        #Then list of enabled plugins
        if "plugins" in self._s2e_configuration and self._s2e_configuration["plugins"]:
            lua.append("\n\nplugins = {\n\t")
            lua.append(",\n\t".join(["\"%s\"" % x for x in self._s2e_configuration["plugins"]]))
            lua.append("\n}\n\n")
        
            #Then configuration for each plugin
            plugin_configs = [(plugin, self.get_plugin_lua(plugin)) for plugin in self._s2e_configuration["plugins"]]
            lua.append("pluginsConfig = {\n\t")
            lua.append(",\n\t".join(["%s = {\n\t\t%s\n\t}" % (plg_name, "\n\t\t".join(plg_conf.split("\n"))) for (plg_name, plg_conf) in plugin_configs]))
            lua.append("\n}\n")

        #Then include raw external files (eg. annotation functions)
        if "include" in self._s2e_configuration and self._s2e_configuration["include"]:
            for fname in self._s2e_configuration["include"]:
                f = open(os.path.join(self._config_directory,  fname), 'r')
                lua.append("\n\n--Including content of file %s\n" % fname)
                for line in f.readlines():
                    lua.append(line)
                lua.append("--End of file %s\n" % fname)
                f.close()
        return "".join(lua)
        
    def get_plugin_lua(self, plugin):
        if plugin in ["BaseInstructions", "Initializer", "FunctionMonitor"]:
            return "" #Plugins not supposed to have options
        elif plugin == "RemoteMemory":
            plug_conf = self._s2e_configuration["plugins"]["RemoteMemory"]
            lua = []
            lua.append("verbose = %s," % (("verbose" in plug_conf and plug_conf["verbose"]) and "true" or "false"))
            if "listen" in plug_conf:
                # using the listen config from the main python config file
                host, port = plug_conf["listen"].split(':')
                self._s2e_remote_memory_plugin_sockaddr = (host, int(port))
            lua.append("listen = \"%s:%d\"," % self._s2e_remote_memory_plugin_sockaddr)
            lua.append("ranges = {")
            ranges = []
            for (range_name, mem_range) in plug_conf["ranges"].items():
                ranges.append(
                        """
                        \t%s = {
                        \t\taddress = 0x%x,
                        \t\tsize = 0x%x,
                        \t\taccess = {%s}
                        \t}
                    """ % (range_name,
                        mem_range["address"],
                        mem_range["size"],
                        ", ".join(["\"%s\"" % x for x in mem_range["access"]])))
            lua.append(",\n".join(ranges))
            lua.append("}")
            return "\n".join(lua)
        elif plugin == "MemoryInterceptorMediator":
            plug_conf = self._s2e_configuration["plugins"]["MemoryInterceptorMediator"]
            lua = []
            lua.append("verbose = %s,\n" % (("verbose" in plug_conf and plug_conf["verbose"]) and "true" or "false"))
            interceptors = []
            for interceptor in ("interceptors" in plug_conf and plug_conf["interceptors"] or {}):
                mem_regions = []
                for mem_region_name in plug_conf["interceptors"][interceptor]:
                    mem_region = plug_conf["interceptors"][interceptor][mem_region_name]
                    mem_regions.append(
                        ("\t\t%s = {\n" % mem_region_name) + \
                        ("\t\t\trange_start = 0x%08x,\n" % mem_region["range_start"]) + \
                        ("\t\t\trange_end = 0x%08x,\n" % mem_region["range_end"]) + \
                        ("\t\t\tpriority = %d,\n" % mem_region["priority"]) + \
                        "\t\t\taccess_type = {\n" + \
                        ",\n".join(["\t\t\t\t\"%s\"" % x for x in mem_region["access_type"]]) + \
                        "\n\t\t\t}" + \
                        "\n\t\t}")
                interceptors.append("\t%s = {\n" % interceptor + \
                    ",\n".join(mem_regions) + \
                    "\n\t}")
            lua.append("interceptors = {\n" + \
                       ",\n".join(interceptors) + \
                       "\n}")
            return "".join(lua)
        else:
            log.warn("Unknown plugin '%s' in configuration - including raw config", plugin)
            return self._s2e_configuration["plugins"][plugin]
    
    def get_s2e_executable(self, arch, endianness='little'):
        """
        This method returns the absolute path to S2E binary.
        """
        # explicit binary path in config
        if "QEMU_S2E" in os.environ:
            return os.environ["QEMU_S2E"]
        elif "s2e_binary" in self._s2e_configuration and self._s2e_configuration["s2e_binary"]:
            return self._s2e_configuration["s2e_binary"]
        # fallback, architecture specific
        elif arch == "arm" and "s2e_debug" in self._avatar_configuration and self._avatar_configuration["s2e_debug"]:
            return "/home/zaddach/projects/eurecom-s2e/build/qemu-debug/arm-s2e-softmmu/qemu-system-arm"
        elif arch == "arm" and endianness == 'little':
            return "~/projects/eurecom-s2e/build/qemu-release/arm-s2e-softmmu/qemu-system-arm"
        elif arch == "arm" and endianness == 'big':
            return "/home/lucian/eurecom/s2e-build-release/qemu-release/armeb-s2e-softmmu/qemu-system-armeb"
        else:
            assert(False) #Architecture not yet implemented
        
    def get_command_line(self):
        cmdline = []
        
        # Check if debugging/tracing facilities are to be employed.
        # See http://wiki.qemu.org/Documentation/Debugging for details.
        if "gdbserver" in self._qemu_configuration and self._qemu_configuration["gdbserver"]:
            cmdline.append("gdbserver")
            # TODO: make this a configurable IP:port tuple
            cmdline.append("localhost:1222")
        elif "valgrind" in self._qemu_configuration and self._qemu_configuration["valgrind"]:     
            cmdline.append("valgrind")
            cmdline.append("--smc-check=all")
            cmdline.append("--leak-check=full")
          
        # S2E parameters  
        cmdline.append(self.get_s2e_executable(self._cm_configuration["architecture"], "endianness" in self._cm_configuration and self._cm_configuration["endianness"] or "little"))
        cmdline.append("-s2e-config-file")
        cmdline.append(os.path.join(self._output_directory, "s2e_conf.lua"))
        if "verbose" in self._s2e_configuration and self._s2e_configuration["verbose"]:
            cmdline.append("-s2e-verbose")        
        if "max-process" in self._s2e_configuration :
            cmdline.append("-s2e-max-processes")
            cmdline.append(" %d"% self._s2e_configuration["verbose"])
            cmdline.append("-nographic")

        # QEMU parameters 
        cmdline.append("-M")
        cmdline.append("configurable")
        cmdline.append("-kernel")
        cmdline.append(os.path.join(self._output_directory, "configurable_machine.json"))
        if "halt_processor_on_startup" in self._qemu_configuration and self._qemu_configuration["halt_processor_on_startup"]:
            cmdline.append("-S")
        self._qemu_configuration["gdb"] = "tcp::%d,server" % get_random_free_port()
        cmdline.append("-gdb")
        cmdline.append("tcp:127.0.0.1:%d,server" % self._s2e_gdb_sockaddr[1])
        if "append" in self._qemu_configuration:
            for val in self._qemu_configuration["append"]:
                cmdline.append(val)
        TRACE_OPTIONS = {"trace_instructions": "in_asm", "trace_microops": "op"}    
        trace_opts = []
        for (config_trace_opt, qemu_trace_opt) in TRACE_OPTIONS.items():
            if config_trace_opt in self._qemu_configuration and self._qemu_configuration[config_trace_opt]:
                trace_opts.append(qemu_trace_opt)
        if trace_opts:
            cmdline.append("-D")
            cmdline.append(os.path.join(self._output_directory, "qemu_trace.log"))
            cmdline.append("-d")
            cmdline.append(",".join(trace_opts))

        if "extra_opts" in self._qemu_configuration:
            for o in self._qemu_configuration["extra_opts"]:
                cmdline.append(o)

        return cmdline
            
    def write_configurable_machine_configuration_file(self):
        cm_conf = {}
        conf_dir = self._config_directory
        output_dir = self._output_directory
        
        assert("architecture" in self._cm_configuration) #Architecture must be specified
        assert("cpu_model" in self._cm_configuration) #CPU must be specified
        assert("entry_address" in self._cm_configuration) #Entry address must be specified
        assert("memory_map" in self._cm_configuration and self._cm_configuration["memory_map"]) #Memory map must be specified
        
        cm_conf["architecture"] = self._cm_configuration["architecture"]
        cm_conf["cpu_model"] = self._cm_configuration["cpu_model"]
        cm_conf["entry_address"] = self._cm_configuration["entry_address"]
        if "init_state" in self._cm_configuration: #Initial state is optional
            cm_conf["init_state"] = self._cm_configuration["init_state"]
        cm_conf["memory_map"] = []
        
        for region in self._cm_configuration["memory_map"]:
            new_region = {"size": region["size"], "name": region["name"]}
            if "is_rom" in region:
                new_region["is_rom"] = region["is_rom"]
            assert(not ("file" in region and "data" in region)) #Cannot have both file and data attribute
            
            if "file" in region:
                #Copy from source directory to output directory
                shutil.copy(os.path.join(conf_dir, region["file"]), os.path.join(output_dir, os.path.basename(region["file"])))
                new_region["file"] = os.path.join(output_dir, os.path.basename(region["file"]))
            if "data" in region:
                #Output data to file
                (f, dest_file) = tempfile.mkstemp(suffix = '.bin', dir = output_dir, text = False)
                os.write(f, region["data"]) 
                os.close(f)
                new_region["file"] = dest_file
            new_region["map"] = []
            for mapping in region["map"]:
                new_region["map"].append({"address": mapping["address"],
                               "type": mapping["type"],
                               "permissions": mapping["permissions"]})
            cm_conf["memory_map"].append(new_region)

        cm_conf["devices"] = []
        devices = "devices" in self._cm_configuration and self._cm_configuration["devices"] or []
        for dev in devices:
            cm_conf["devices"].append(dev)
        
        f = open(os.path.join(output_dir, "configurable_machine.json"), 'w')
        json.dump(cm_conf, f, indent = 4)
        f.write("\n\n")
        f.close()
        
    
    def write_configuration_files(self, output_dir):
        f = open(os.path.join(output_dir,  "s2e_conf.lua"), 'w')
        f.write(self.get_s2e_lua())
        f.close()
        self.write_configurable_machine_configuration_file()

    def get_output_directory(self):
        return self._output_directory
    
    def get_s2e_gdb_port(self):
        return self._s2e_gdb_sockaddr[1]

    def get_remote_memory_listen_address(self):
        return self._s2e_remote_memory_plugin_sockaddr
