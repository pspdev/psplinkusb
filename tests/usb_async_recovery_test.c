/*
 * Exercise thread-safe host USB handle recovery without a PSP attached.
 * The stalled writer must finish before the main thread can close the handle.
 */
#define main usbhostfs_pc_main
#include "../usbhostfs_pc/main.c"
#undef main

#include <assert.h>

static char old_device, new_device;
static int releases, closes, resets;
static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static int writer_acquired, complete_writer;
static libusb_device_handle *retired_device;
static int retired_after_async_error;

int LIBUSB_CALL libusb_release_interface(libusb_device_handle *dev, int number)
{
	assert(dev == (libusb_device_handle *)&old_device ||
	       dev == (libusb_device_handle *)&new_device);
	assert(number == 0);
	releases++;
	return LIBUSB_SUCCESS;
}

int LIBUSB_CALL libusb_reset_device(libusb_device_handle *dev)
{
	(void)dev;
	resets++;
	return LIBUSB_SUCCESS;
}

void LIBUSB_CALL libusb_close(libusb_device_handle *dev)
{
	assert(dev == (libusb_device_handle *)&old_device ||
	       dev == (libusb_device_handle *)&new_device);
	closes++;
}

static void *writer(void *data)
{
	libusb_device_handle *dev = usb_acquire_async_handle();
	(void)data;
	assert(dev == (libusb_device_handle *)&old_device);
	pthread_mutex_lock(&test_mutex);
	writer_acquired = 1;
	pthread_cond_signal(&test_cond);
	while(!complete_writer)
		pthread_cond_wait(&test_cond, &test_mutex);
	pthread_mutex_unlock(&test_mutex);
	usb_complete_async_transfer(LIBUSB_ERROR_TIMEOUT, 14);
	return NULL;
}

static void *retirer(void *data)
{
	(void)data;
	retired_device = usb_take_handle(&retired_after_async_error);
	return NULL;
}

int main(void)
{
	pthread_t writing, retiring;
	struct timespec tick = {0, 1000000};
	int i, requested;
	libusb_device_handle *dev;

	usb_publish_handle((libusb_device_handle *)&old_device);
	assert(pthread_create(&writing, NULL, writer, NULL) == 0);
	pthread_mutex_lock(&test_mutex);
	while(!writer_acquired)
		pthread_cond_wait(&test_cond, &test_mutex);
	pthread_mutex_unlock(&test_mutex);

	assert(pthread_create(&retiring, NULL, retirer, NULL) == 0);
	for(i = 0; i < 1000 && !usb_reconnect_pending(); ++i)
		nanosleep(&tick, NULL);
	assert(usb_reconnect_pending());
	assert(usb_acquire_async_handle() == NULL);
	assert(closes == 0);

	pthread_mutex_lock(&test_mutex);
	complete_writer = 1;
	pthread_cond_signal(&test_cond);
	pthread_mutex_unlock(&test_mutex);
	assert(pthread_join(writing, NULL) == 0);
	assert(pthread_join(retiring, NULL) == 0);
	assert(retired_device == (libusb_device_handle *)&old_device);
	assert(retired_after_async_error == 1);
	/* Default recovery resets the USB device, only after async I/O finishes. */
	close_device(retired_device);
	assert(releases == 1 && closes == 1 && resets == 1);

	/* After re-opening, successful commands do not request another reset. */
	usb_publish_handle((libusb_device_handle *)&new_device);
	dev = usb_acquire_async_handle();
	assert(dev == (libusb_device_handle *)&new_device);
	usb_complete_async_transfer(14, 14);
	assert(!usb_reconnect_pending());
	dev = usb_take_handle(&requested);
	assert(dev == (libusb_device_handle *)&new_device);
	assert(!requested);
	assert(usb_acquire_async_handle() == NULL);
	/* The -N diagnostic flag intentionally leaves USB reset disabled. */
	g_skip_close_reset = 1;
	close_device(dev);
	assert(releases == 2 && closes == 2 && resets == 1);
	puts("USB async recovery synchronization test passed");
	return 0;
}
