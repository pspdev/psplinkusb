/*
 * HostFS directory regression test. Include the implementation so its
 * otherwise-internal directory helpers can be exercised without USB hardware.
 */
#define main usbhostfs_pc_main
#include "../usbhostfs_pc/main.c"
#undef main

#include <assert.h>

/* Exercise the on-wire directory response without a physical USB device. */
static int usb_write_count;
static struct HostFsDreadResp usb_dread_response;
static SceIoDirent usb_dread_entry;

int LIBUSB_CALL libusb_bulk_transfer(libusb_device_handle *dev, unsigned char endpoint,
		unsigned char *data, int length, int *transferred, unsigned int timeout)
{
	(void) dev;
	(void) timeout;
	assert(endpoint == 0x02);
	if(usb_write_count == 0)
	{
		assert(length == sizeof(usb_dread_response));
		memcpy(&usb_dread_response, data, length);
	}
	else if(usb_write_count == 1)
	{
		assert(length == sizeof(usb_dread_entry));
		memcpy(&usb_dread_entry, data, length);
	}
	else
	{
		assert(0 && "unexpected USB transfer");
	}
	usb_write_count++;
	*transferred = length;
	return LIBUSB_SUCCESS;
}

int main(void)
{
	char root[] = "/tmp/psplinkusb-dir-XXXXXX";
	char linkpath[PATH_MAX];
	const char *iterations = getenv("HOSTFS_TEST_ITERATIONS");
	int repetitions = iterations ? atoi(iterations) : 1000;
	int did;
	int found_link = 0;
	int i;

	assert(repetitions > 0);
	assert(mkdtemp(root) != NULL);
	assert(snprintf(linkpath, sizeof(linkpath), "%s/broken-link", root) < (int)sizeof(linkpath));
	assert(symlink("missing-target", linkpath) == 0);

	init_hostfs();
	strcpy(g_drives[0].rootdir, root);

	did = dir_open(0, "/");
	assert(did >= 0);
	for(i = 0; i < open_dirs[did].count; i++)
	{
		if(strcmp(open_dirs[did].pDir[i].name, "broken-link") == 0)
		{
			found_link = 1;
			assert(LE32(open_dirs[did].pDir[i].stat.mode) & FIO_S_IFLNK);
		}
	}
	assert(found_link);
	{
		struct HostFsDreadCmd cmd;
		int ret;
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd.magic = LE32(HOSTFS_MAGIC);
		cmd.cmd.command = LE32(HOSTFS_CMD_DREAD);
		cmd.did = LE32(did);
		ret = handle_dread(NULL, &cmd, sizeof(cmd));
		assert(ret == sizeof(usb_dread_entry));
		assert(usb_write_count == 2);
		assert(LE32(usb_dread_response.cmd.magic) == HOSTFS_MAGIC);
		assert(LE32(usb_dread_response.cmd.command) == HOSTFS_CMD_DREAD);
		assert(LE32(usb_dread_response.cmd.extralen) == sizeof(SceIoDirent));
		assert(LE32(usb_dread_response.res) > 0);
		assert(usb_dread_entry.name[0] != '\0');
	}
	assert(dir_close(did) == 0);

	/* Run this under a leak sanitizer to catch scandir allocations on success. */
	for(i = 0; i < repetitions; i++)
	{
		did = dir_open(0, "/");
		assert(did >= 0);
		assert(dir_close(did) == 0);
	}

	assert(unlink(linkpath) == 0);
	assert(rmdir(root) == 0);
	puts("HostFS directory test passed");
	return 0;
}
