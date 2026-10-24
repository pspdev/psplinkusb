/*
 * Validate diagnostics for a libusb handle whose device is no longer present
 * after a USB disconnect/re-enumeration. No PSP hardware is required.
 */
#define main usbhostfs_pc_main
#include "../usbhostfs_pc/main.c"
#undef main

#include <assert.h>

static unsigned char original_device, replacement_device;
static libusb_device *devices[3];
static libusb_device *handle_device;
static ssize_t device_count;
static int frees;

ssize_t LIBUSB_CALL libusb_get_device_list(libusb_context *ctx, libusb_device ***list)
{
	(void)ctx;
	*list = devices;
	return device_count;
}

void LIBUSB_CALL libusb_free_device_list(libusb_device **list, int unref_devices)
{
	assert(list == devices);
	assert(unref_devices == 1);
	frees++;
}

libusb_device * LIBUSB_CALL libusb_get_device(libusb_device_handle *handle)
{
	assert(handle != NULL);
	return handle_device;
}

int main(void)
{
	libusb_device_handle *handle = (libusb_device_handle *) &original_device;
	handle_device = (libusb_device *) &original_device;
	devices[0] = handle_device;
	devices[1] = (libusb_device *) &replacement_device;
	devices[2] = NULL;
	device_count = 2;
	assert(usb_handle_enumerated(handle) == 1);
	assert(frees == 1);

	/* Device was replaced, but the old handle still retains its pointer. */
	devices[0] = devices[1];
	devices[1] = NULL;
	device_count = 1;
	assert(usb_handle_enumerated(handle) == 0);
	assert(frees == 2);

	device_count = LIBUSB_ERROR_IO;
	assert(usb_handle_enumerated(handle) == -1);
	assert(frees == 2);
	assert(usb_handle_enumerated(NULL) == 0);
	puts("USB device identity diagnostic test passed");
	return 0;
}
