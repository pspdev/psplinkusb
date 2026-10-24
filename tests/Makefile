CC ?= cc
HOST_CFLAGS = -g -O1 -DPC_SIDE -D_FILE_OFFSET_BITS=64 -I../usbhostfs -I../usbhostfs_pc $(shell pkg-config --cflags libusb-1.0)
HOST_LIBS = -lpthread $(shell pkg-config --libs libusb-1.0)

.PHONY: test
test:
	@bin=$$(mktemp /tmp/psplinkusb-hostfs-test.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CC) $(HOST_CFLAGS) -o "$$bin" hostfs_directory_test.c $(HOST_LIBS) && "$$bin"
	@bin=$$(mktemp /tmp/psplinkusb-usb-identity-test.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CC) $(HOST_CFLAGS) -o "$$bin" usb_device_identity_test.c $(HOST_LIBS) && "$$bin"
