=== How to reverse engineer a device with Avatar ===

== Using JTAG ==

== Using a software debugger (GDB stub) ==
* Dump everything of the code that you can get.
* Reverse engineer code to understand how to use the serial port and to find a suitable first
  memory location for the GDB stub.
* Find a way to inject and execute code on the embedded system.
* Adapt the GDB stub for your target, i.e., set up a start file that puts the stack in the right
  location, and add a driver for the serial port (or use an existing driver if you are lucky enough
  to have a known serial port)
* Write a flasher script that copies the GDB stub to the target and executes it
* Run the GDB stub and poke around in memory to see if there are any memory regions you don't know of yet ...
  in particular look for a ROM code section that sets up a very basic environment and loads code from 
  flash/serial port/etc. Probing the memory can be annoying if the device reboots on an invalid memory access.
* Extract the ROM loader code
* Create an AVATAR script that forwards all memory accesses except for the ROM code and the serial port for
  your device and start executing.