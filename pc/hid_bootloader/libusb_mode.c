// libusb_mode.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

#include "libusb.h"
#include "intel_hex.h"
#include "bootloader.h"
#include "opt_output.h"

/**************************************************************************************************
* Update firmware via libusb with WINUSB driver
*/
int LIBUSB_mode(unsigned short vid, unsigned short pid)
{
	int res;
	libusb_context *context = NULL;
	struct libusb_device_handle *dev;
	struct libusb_device_descriptor desc;

	if ((res = libusb_init(&context)) < 0)
		return res;

	libusb_set_debug(context, 3);	// verbosity level 3

	dev = libusb_open_device_with_vid_pid(context, vid, pid);
	if (dev == NULL)
	{
		silent_printf("libusb: could not open %04X:%04X.", vid, pid);
		res = -1;
		goto exit;
	}

	if ((res = libusb_get_device_descriptor(libusb_get_device(dev), &desc)) < 0)
		goto exit;

	quiet_printf("Target found.\n");
	char temp[64];
	res = libusb_get_string_descriptor_ascii(dev, desc.iManufacturer, temp, sizeof(temp));
	quiet_printf("Manufacturer:\t%s\n", temp);
	res = libusb_get_string_descriptor_ascii(dev, desc.iProduct, temp, sizeof(temp));
	quiet_printf("Product:\t%s\n", temp);
	if ((res = libusb_get_string_descriptor_ascii(dev, desc.iSerialNumber, temp, sizeof(temp))) < 0)
		strncpy(temp, "(none)", sizeof(temp));
	quiet_printf("Serial:\t\t%s\n", temp);

	if ((res = libusb_claim_interface(dev, 0)) < 0)
	{
		silent_printf("libusb: unable to claim interface 0 (%s)", libusb_strerror(res));
		goto exit;
	}

	#define BUFFER_SIZE 4
	uint8_t buffer[BUFFER_SIZE];
	for (int i = 0; i < BUFFER_SIZE; i++)
		buffer[i] = i & 0xFF;

	int actual_length;
	res = libusb_bulk_transfer(dev, 0x02, buffer, BUFFER_SIZE, &actual_length, 500);
	printf("Bulk result: %d (%s)\n", res, libusb_strerror(res));
	libusb_clear_halt(dev, 0x02);

	libusb_release_interface(dev, 0);

	res = 0;
exit:
	if (dev != NULL)
		libusb_close(dev);
	libusb_exit(context);

	return res;
}