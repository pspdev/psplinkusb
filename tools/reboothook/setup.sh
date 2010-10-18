# Load reboot hook into memory and patch the kernel to call it
# Must be run in the current directory of reboothook.bin
loadmem 0x883e0000 reboothook.bin
pokeh @sceLoadExec@+0x2384 0x883E
pokew 0x883e0080 0
dcache wi
icache
