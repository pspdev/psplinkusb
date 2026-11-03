all: reboothook.elf reboothook.bin

CFLAGS = -Wall -Werror -Wl,-Ttext,0x883E0000 -nostartfiles

%.elf: %.S
	psp-gcc $(CFLAGS) -o $@ $^

%.bin: %.elf
	# The ELF contains PSP SDK ABI metadata at a distant LMA; only the
	# load-addressed reboot hook instructions belong in this raw binary.
	psp-objcopy --only-section=.text --change-section-lma .text=0 -O binary $^ $@

clean:
	rm -f reboothook.bin reboothook.elf *.o
