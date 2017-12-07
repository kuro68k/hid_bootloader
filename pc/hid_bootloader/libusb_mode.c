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

#define BUFFER_SIZE			64
#define	DEFAULT_TIMEOUT_MS	0
#define	EP_BULK_IN			0x81
#define	EP_BULK_OUT			0x02


/**************************************************************************************************
* Execute a bootloader command with uint16 parameters
*/
bool lusbExecuteCommand(struct libusb_device_handle *dev, uint8_t command, uint16_t param1, uint16_t param2)
{
	int res = libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
									  command, param1, param2,
									  NULL, 0, DEFAULT_TIMEOUT_MS);
	if (res < 0)
	{
		silent_printf("libusb_control_transfer failed (%d).\n", res);
		silent_printf("%s\n", libusb_strerror(res));
		return false;
	}
	return true;
}

/**************************************************************************************************
* Execute a bootloader command with no parameters
*/
bool lusbExecuteCommandNP(struct libusb_device_handle *dev, uint8_t command)
{
	return lusbExecuteCommand(dev, command, 0, 0);
}

/**************************************************************************************************
* Execute a bootloader command with uint32 parameter
*/
bool lusbExecuteCommandU32(struct libusb_device_handle *dev, uint8_t command, uint32_t param)
{
	return lusbExecuteCommand(dev, command, param >> 16, param & 0xFFFF);
}

/**************************************************************************************************
* Check if the bootloader is busy. 1 = busy, 0 = not busy, -1 = USB error
*/
int lusbCheckBusy(struct libusb_device_handle *dev)
{
	// request status
	if (!lusbExecuteCommandNP(dev, CMD_NOP))
		return -1;

	// read status
	BLSTATUS_t status;
	int tx;
	int res = libusb_bulk_transfer(dev, EP_BULK_IN, (uint8_t *)&status, sizeof(status), &tx, DEFAULT_TIMEOUT_MS);
	if (res < 0)
	{
		silent_printf("libusb_bulk_transfer failed (%d).\n", res);
		silent_printf("%s\n", libusb_strerror(res));
		return -1;
	}
	if (tx != sizeof(status))
	{
		silent_printf("libusb_bulk_transfer returned wrong number of bytes (%d).\n", tx);
		return -1;
	}
	if (status.busy_flags == 0)
		return 0;
	return 1;
}

/**************************************************************************************************
* Wait for MCU to clear busy flags
*/
bool lusbWaitNotBusy(struct libusb_device_handle *dev)
{
	uint8_t timeout = 100;
	int res;
	while ((res = lusbCheckBusy(dev)) != 0)
	{
		if (res == -1)	// error
			return false;
		if (--timeout == 0)
			return false;
	}
	return true;
}

/**************************************************************************************************
* Write loaded firmware image to target
*/
bool lusbUpdateFirmware(struct libusb_device_handle *dev)
{
	BLCOMMAND_t cmd = { .report_id = 0, .params.u32 = 0 };
	uint8_t buffer[BUFFER_SIZE];
	int num_pages = fw_info->flash_size_b / fw_info->page_size_b;

	quiet_printf("\nPages:\t%d\n", num_pages);

	// erase app section
	silent_printf("Erasing application section...");
	if ((!lusbExecuteCommandNP(dev, CMD_ERASE_APP_SECTION)) || (!lusbWaitNotBusy(dev)))
	{
		silent_printf("FAILED\nFailed to erase application section.\n");
		return false;
	}
	silent_printf("OK\n");


	if (!lusbExecuteCommandNP(dev, CMD_SET_POINTER))
		return false;

	// write app section
	silent_printf("Writing firmware image");
	uint8_t	c = 0;
	for (int page = 0; page < num_pages; page++)
	{
		// load page into RAM buffer
		for (int byte = 0; byte < fw_info->page_size_b; byte += BUFFER_SIZE)
		{
			memcpy(&buffer[0], &firmware_buffer[(page*fw_info->page_size_b) + byte], BUFFER_SIZE);
			int tx;
			int res = libusb_bulk_transfer(dev, EP_BULK_OUT, buffer, BUFFER_SIZE, &tx, DEFAULT_TIMEOUT_MS);
			if (res == -1)
			{
				silent_printf("\nFailed to write to RAM buffer (page %d, byte %d).\n", page, byte);
				silent_printf("libusb_bulk_transfer failed (%d).\n", res);
				silent_printf("%s\n", libusb_strerror(res));
				return false;
			}
			if (tx != BUFFER_SIZE)
			{
				silent_printf("\nFailed to write to RAM buffer (page %d, byte %d).\n", page, byte);
				silent_printf("libusb_bulk_transfer transferred incorrect number of bytes (%d).\n", tx);
				return false;
			}
		}

		// write RAM buffer to page
		if ((!lusbExecuteCommand(dev, CMD_WRITE_PAGE, page, 0)) || (!lusbWaitNotBusy(dev)))
		{
			silent_printf("\nFailed to write to page %d.\n", page);
			return false;
		}

		c++;
		c &= 0x0F;
		if (c == 0)
			quiet_printf(".");
	}
	silent_printf("\n");

	/*
	// verify CRC
	cmd.command = CMD_READ_FLASH_CRCS;
	if (!ExecuteHIDCommandWithResponse(handle, &cmd, buffer, sizeof(buffer)))
	{
		silent_printf("Failed to read flash CRCs.\n");
		return false;
	}

	uint32_t app_crc;
	app_crc = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
	quiet_printf("Target CRC:\t0x%lX\n", app_crc);
	quiet_printf("Local CRC:\t0x%lX\n", firmware_crc);
	if (app_crc != firmware_crc)
	{
		silent_printf("Firmware image CRC does not match device.\n");
		return false;
	}
	*/
	return true;
}

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
	/*
	#define BUFFER_SIZE 128
	uint8_t buffer[BUFFER_SIZE];
	for (int i = 0; i < BUFFER_SIZE; i++)
		buffer[i] = i & 0xFF;

	int actual_length;
	res = libusb_bulk_transfer(dev, 0x02, buffer, BUFFER_SIZE, &actual_length, 500);
	printf("Bulk result: %d (%s)\n", res, libusb_strerror(res));
	libusb_clear_halt(dev, 0x02);
	*/

	lusbUpdateFirmware(dev);

	libusb_release_interface(dev, 0);

	res = 0;
exit:
	if (dev != NULL)
		libusb_close(dev);
	libusb_exit(context);

	return res;
}