release: all
	mksfoex -d MEMSIZE=1 'PSPLink v3.1.1' PARAM.SFO
	pack-pbp EBOOT.PBP PARAM.SFO psplink.png NULL NULL NULL NULL psplink_boot.prx NULL

clean_release:
	rm -f PARAM.SFO
	rm -f EBOOT.PBP

TARGET = psplink_boot
OBJS = main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -g
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

PSP_LARGE_MEMORY = 1
BUILD_PRX = 1

EXTRA_CLEAN = clean_release

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
