// hid_bootloader.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

//#include "getopt.h"
#include "hidapi.h"
#include "intel_hex.h"
#include "bootloader.h"


#define MAX_STR			255
#define	BUFFER_SIZE		(1+1+2+HID_DATA_BYTES)	// HID report ID, command, address, data
#define	IDX_REPORT_ID	0
#define	IDX_COMMAND		1
#define	IDX_ADDR		2
#define	IDX_DATA		4


bool UpdateFirmware(hid_device *handle);
bool ExecuteHIDCommand(hid_device *handle, uint8_t command, uint16_t addr, int timeout_ms, uint8_t *buffer);


int main(int argc, char* argv[])
{
	int res;
	wchar_t wstr[MAX_STR];
	
	// command line arguments
	if (argc != 4)
	{
		printf("Usage: <vid> <pid> <firmware.hex>\n");
		return -1;
	}
	unsigned short vid, pid;
	long int temp;
	temp = strtol(argv[1], NULL, 0);
	if (temp > 0xFFFF)
	{
		printf("Bad VID (%lX)\n", temp);
		return -1;
	}
	vid = (unsigned short)temp;
	temp = strtol(argv[2], NULL, 0);
	if (temp > 0xFFFF)
	{
		printf("Bad PID (%lX)\n", temp);
		return -1;
	}
	pid = (unsigned short)temp;
	
	// find target HID device
	hid_device *handle;
	handle = hid_open(0x8282, 0xB71D, NULL);

	if (handle == NULL)
	{
		printf("Unable to find target device.\n");
		return -1;
	}
	res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	printf("Manufacturer:\t%ls\n", wstr);
	res = hid_get_product_string(handle, wstr, MAX_STR);
	printf("Product:\t%ls\n", wstr);
	res = hid_get_serial_number_string(handle, wstr, MAX_STR);
	printf("Serial:\t%ls\n", wstr);

	// read .hex file
	if (!ReadHexFile(argv[3]))
		return 1;
	return 0;
	if (!UpdateFirmware(handle))
		return -1;

	return 0;
}

bool ExecuteHIDCommand(hid_device *handle, uint8_t command, uint16_t addr, int timeout_ms, uint8_t *buffer)
{
	int res;

	// send command
	buffer[IDX_REPORT_ID] = 0;
	buffer[IDX_COMMAND] = command;
	buffer[IDX_ADDR] = addr & 0xFF;
	buffer[IDX_ADDR+1] = (addr >> 8) & 0xFF;
	res = hid_write(handle, buffer, BUFFER_SIZE);
	if (res == -1)
		return false;

	// wait for response
	if (timeout_ms == 0)	// no response required
		return true;

	res = hid_read_timeout(handle, buffer, BUFFER_SIZE, timeout_ms);
	if (res == -1)
		return false;

	// validate response
	if (buffer[IDX_COMMAND] != (command | 0x80))
		return false;
	if ((buffer[IDX_ADDR] != (addr & 0xFF)) || (buffer[IDX_ADDR + 1] != ((addr >> 8) & 0xFF)))
		return false;

	return true;
}

bool UpdateFirmware(hid_device *handle)
{
	uint8_t buffer[BUFFER_SIZE];

	// erase app section
	if (!ExecuteHIDCommand(handle, CMD_ERASE_APP_SECTION, 0, APP_SECTION_ERASE_TIMEOUT_MS, buffer))
	{
		printf("Failed to erase application section.\n");
		return false;
	}

	// write app section
	for (int page = 0; page < APP_SECTION_NUM_PAGES; page++)
	{
		// load page into RAM buffer
		for (int byte = 0; byte < APP_SECTION_PAGE_SIZE; byte += HID_DATA_BYTES)
		{
			memcpy(&buffer[IDX_DATA], &firmware_buffer[(page*APP_SECTION_PAGE_SIZE) + byte], HID_DATA_BYTES);
			if (!ExecuteHIDCommand(handle, CMD_WRITE_BUFFER, byte, 100, buffer))
			{
				printf("Failed to write to RAM buffer (page %d, byte %d).\n", page, byte);
				return false;
			}
		}

		// write RAM buffer to page
		if (!ExecuteHIDCommand(handle, CMD_WRITE_PAGE, page, 100, buffer))
		{
			printf("Failed to write to page %d.\n", page);
			return false;
		}
	}

	// verify CRC
	if (!ExecuteHIDCommand(handle, CMD_READ_FLASH_CRCS, 0, READ_FLASH_CRCS_TIMEOUT_MS, buffer))
	{
		printf("Failed to read flash CRCs.\n");
		return false;
	}

	uint32_t app_crc;
	app_crc = buffer[IDX_DATA] | (buffer[IDX_DATA + 1] << 8) | (buffer[IDX_DATA + 2] << 16) | (buffer[IDX_DATA + 1] << 24);
	printf("Application CRC:\t%lX\n", app_crc);
	if (app_crc != firmware_crc)
	{
		printf("Firmware image CRC does not match device.\n");
		return false;
	}

	return true;
}