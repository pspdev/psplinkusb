all: reboothook.elf reboothook.bin

CFLAGS = -Wl,-Ttext,0x883E0000 -nostartfiles

%.elf: %.S
	psp-gcc $(CFLAGS) -o $@ $^

%.bin: %.elf
	psp-objcopy -O binary $^ $@

clean:
	rm -f reboothook.bin reboothook.elf *.o
