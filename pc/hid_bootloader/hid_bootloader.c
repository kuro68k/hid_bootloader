// hid_bootloader.cpp
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

#include "hidapi.h"
#include "libusb.h"
#include "intel_hex.h"
#include "bootloader.h"
#include "getopt.h"
#include "opt_output.h"


#define MAX_STR				255
#define	BUFFER_SIZE			(64+1)		// +1 for mandatory HID report ID


bool ExecuteHIDCommand(hid_device *handle, BLCOMMAND_t *cmd);
bool ExecuteHIDCommandWithResponse(hid_device *handle, BLCOMMAND_t *cmd, uint8_t *buffer, uint8_t buffer_size);
bool UpdateFirmware(hid_device *handle);
bool VerifyFirmware(hid_device *handle);
bool GetBootloaderInfo(hid_device *handle);


uint8_t target_mcu_id[4] = { 0, 0, 0, 0 };
uint8_t	target_mcu_fuses[6] = { 0, 0, 0, 0, 0, 0 };
char *hexfile = NULL;
unsigned short vid, pid;

bool opt_reset = false;
bool opt_quiet = false;
bool opt_silent = false;
bool opt_verify = false;


/**************************************************************************************************
* Handle command line args
*/
int parse_args(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "rqsv")) != -1)
	{
		switch (c)
		{
		case 'r':
			opt_reset = true;
			break;

		case 'q':
			opt_quiet = true;
			break;

		case 's':
			opt_silent = true;
			opt_quiet = true;
			break;

		case 'v':
			opt_verify = true;
			break;

		case '?':
			printf("Unknown option -%c.\n", optopt);
			return 1;
		}
	}


	// non option arguments
	int j = 0;
	long int temp;
	for (int i = optind; i < argc; i++)
	{
		//printf("Opt: %s\n", argv[i]);
		switch (j)
		{
		case 0:
			temp = strtol(argv[i], NULL, 0);
			if (temp > 0xFFFF)
			{
				printf("Bad VID (%lX)\n", temp);
				return 1;
			}
			vid = (unsigned short)temp;
			break;

		case 1:
			temp = strtol(argv[i], NULL, 0);
			if (temp > 0xFFFF)
			{
				printf("Bad PID (%lX)\n", temp);
				return 1;
			}
			pid = (unsigned short)temp;
			break;

		case 2:
			hexfile = argv[i];
			break;

		default:
			printf("Too many arguments.\n");
			return 1;
		}
		j++;
	}

	if (j < 3)
	{
		printf("Usage: [-qrsv] <vid> <pid> <firmware.hex>\n");
		printf("\nOptions:\n");
		printf("\t-q\tquiet (less output)\n");
		printf("\t-r\treset after loading firmware\n");
		printf("\t-s\tsilent (no output, return code only)\n");
		printf("\t-v\tverify firmware by reading back\n");
		return 1;
	}

	return 0;
}


/**************************************************************************************************
* Main entry point
*/
int main(int argc, char* argv[])
{
	int res;
	wchar_t wstr[MAX_STR];

	res = parse_args(argc, argv);
	if (res != 0)
		return res;

	// find target HID device
	hid_device *handle;
	handle = hid_open(vid, pid, NULL);

	if (handle == NULL)
	{
		silent_printf("Unable to find target device.\n");
		return 1;
	}
	quiet_printf("Target found.\n");
	res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	quiet_printf("Manufacturer:\t%ls\n", wstr);
	res = hid_get_product_string(handle, wstr, MAX_STR);
	quiet_printf("Product:\t%ls\n", wstr);
	//res = hid_get_serial_number_string(handle, wstr, MAX_STR);
	//quiet_printf("Serial:\t%ls\n", wstr);

	// get bootloader info
	if (!GetBootloaderInfo(handle))
		return 1;

	// read .hex file
	if (!ReadHexFile(hexfile))
		return 1;
	
	// check firmware is suitable for target
	if (memcmp(&fw_info->mcu_signature, target_mcu_id, 3) != 0)
	{
		silent_printf("MCU signature does not match firmware image.\n");
		return 1;
	}
	quiet_printf("MCU signature matches firmware image.\n");

	if (!UpdateFirmware(handle))
		return 1;

	if (opt_verify && (!VerifyFirmware(handle)))
		return 1;

	// firmware written OK, reset target
	if (opt_reset)
	{
		quiet_printf("Resetting MCU...\n");
		BLCOMMAND_t cmd = { .report_id = 0, .params.u32 = 0 };
		cmd.command = CMD_RESET_MCU;
		if (!ExecuteHIDCommand(handle, &cmd))
		{
			silent_printf("Failed to reset target.\n");
			silent_printf("%ls\n", hid_error(handle));
			return false;
		}
	}

	silent_printf("Firmware update complete.\n");
	return 0;
}

/**************************************************************************************************
* Check if the bootloader is busy. True = busy, false = ready for next command
*/
bool CheckBusy(hid_device *handle)
{
	BLSTATUS_t buffer;
	buffer.report_id = 0;
	int i = hid_get_feature_report(handle, (unsigned char *)&buffer, sizeof(buffer));
	if ((i == 6) && (buffer.busy_flags == 0))
		return true;
	return false;
}

/**************************************************************************************************
* Wait for MCU to clear busy flags
*/
bool WaitNotBusy(hid_device *handle)
{
	uint8_t timeout = 100;
	while (CheckBusy(handle))
	{
		if (--timeout == 0)
			return false;
		//Sleep(10);
	}
	return true;
}

/**************************************************************************************************
* Execute a HID bootloader command
*/
bool ExecuteHIDCommand(hid_device *handle, BLCOMMAND_t *cmd)
{
	int res = hid_send_feature_report(handle, (unsigned char *)cmd, sizeof(BLCOMMAND_t));
	if (res == -1)
	{
		silent_printf("hid_send_feature_report failed.\n");
		silent_printf("%ls\n", hid_error(handle));
		return false;
	}

	BLSTATUS_t status;
	status.report_id = 0;
	res = hid_get_feature_report(handle, (uint8_t *)&status, sizeof(status));
	if ((res != sizeof(status) - 1) || (status.result != 0))	// size-1 because report ID not transmitted
		return false;
	return true;
}

/**************************************************************************************************
* Execute a HID bootloader command and get response.
*/
bool ExecuteHIDCommandWithResponse(hid_device *handle, BLCOMMAND_t *cmd, uint8_t *buffer, uint8_t buffer_size)
{
	// clear out any unread reports
	while ((hid_read_timeout(handle, buffer, buffer_size, 0)) > 0);
	
	if (!ExecuteHIDCommand(handle, cmd))
		return false;

	int res = hid_read_timeout(handle, buffer, buffer_size, 100);
	if ((res == -1) || (res == 0))	// -1 == failure, 0 == no report available
	{
		silent_printf("hid_read failed.\n");
		silent_printf("%ls\n", hid_error(handle));
		return false;
	}

	return true;
}

/**************************************************************************************************
* Write loaded firmware image to target
*/
bool UpdateFirmware(hid_device *handle)
{
	BLCOMMAND_t cmd = { .report_id = 0, .params.u32 = 0 };
	uint8_t buffer[BUFFER_SIZE];
	int num_pages = fw_info->flash_size_b / fw_info->page_size_b;

	quiet_printf("Pages:\t%d\n", num_pages);

	// erase app section
	silent_printf("Erasing application section\n");
	cmd.command = CMD_ERASE_APP_SECTION;
	if ((!ExecuteHIDCommand(handle, &cmd)) || (!WaitNotBusy(handle)))
	{
		silent_printf("Failed to erase application section.\n");
		return false;
	}

	cmd.command = CMD_SET_POINTER;
	cmd.params.u16[0] = 0;
	if (!ExecuteHIDCommand(handle, &cmd))
		return false;

	// write app section
	silent_printf("Writing firmware image");
	uint8_t	c = 0;
	for (int page = 0; page < num_pages; page++)
	{
		// load page into RAM buffer
		for (int byte = 0; byte < fw_info->page_size_b; byte += HID_DATA_BYTES)
		{
			buffer[0] = 0;	// mandatory report ID
			memcpy(&buffer[1], &firmware_buffer[(page*fw_info->page_size_b) + byte], HID_DATA_BYTES);
			int res = hid_write(handle, buffer, BUFFER_SIZE);
			if (res == -1)
			{
				silent_printf("\nFailed to write to RAM buffer (page %d, byte %d).\n", page, byte);
				silent_printf("%ls\n", hid_error(handle));
				return false;
			}
		}

		// write RAM buffer to page
		cmd.command = CMD_WRITE_PAGE;
		cmd.params.u16[0] = page;
		if ((!ExecuteHIDCommand(handle, &cmd)) || (!WaitNotBusy(handle)))
		{
			silent_printf("\nFailed to write to page %d.\n", page);
			silent_printf("%ls\n", hid_error(handle));
			return false;
		}

		c++;
		c &= 0x0F;
		if (c == 0)
			quiet_printf(".");
	}
	silent_printf("\n");


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

	return true;
}

/**************************************************************************************************
* Read back firmware from device for byte-by-byte comparison
*/
bool VerifyFirmware(hid_device *handle)
{
	BLCOMMAND_t cmd;
	cmd.report_id = 0;
	cmd.command = CMD_READ_FLASH;
	uint8_t buffer[BUFFER_SIZE];

	silent_printf("Verifying firmware image");
	uint32_t addr = 0;
	uint8_t	c = 0;
	while (addr < fw_info->flash_size_b)
	{
		cmd.params.u32 = addr;
		if (!ExecuteHIDCommandWithResponse(handle, &cmd, buffer, sizeof(buffer)))
		{
			silent_printf("\nRead failed at address 0x%08X\n", addr);
			return false;
		}
		
		for (uint8_t i = 0; i < 64; i++)
		{
			if (buffer[i] != firmware_buffer[addr])
			{
				silent_printf("\nVerify failed at address 0x%08X\n", addr);
				return false;
			}
			addr++;
		}

		if (!opt_quiet)
		{
			c++;
			c &= 0x0F;
			if (c == 0)
				printf(".");
		}
	}
	silent_printf("\n");
	
	return true;
}

/**************************************************************************************************
* Check device serial number, MCU ID and fuses
*/
bool GetBootloaderInfo(hid_device *handle)
{
	BLCOMMAND_t cmd = { .report_id = 0, .params.u32 = 0 };
	uint8_t buffer[BUFFER_SIZE];

	// serial number
	cmd.command = CMD_READ_SERIAL;
	if (!ExecuteHIDCommandWithResponse(handle, &cmd, buffer, sizeof(buffer)))
		return false;
	buffer[BUFFER_SIZE - 1] = '\0';		// ensure string is null terminated
	quiet_printf("Serial:\t\t%s\n", buffer);

	// MCU ID
	cmd.command = CMD_READ_MCU_IDS;
	if (!ExecuteHIDCommandWithResponse(handle, &cmd, buffer, sizeof(buffer)))
		return false;
	target_mcu_id[0] = buffer[0];
	target_mcu_id[1] = buffer[1];
	target_mcu_id[2] = buffer[2];
	target_mcu_id[3] = buffer[3];
	quiet_printf("MCU ID:\t\t%02X%02X%02X-%c\n", target_mcu_id[0], target_mcu_id[1], target_mcu_id[2], target_mcu_id[3] + 'A');

	// MCU fuses
	cmd.command = CMD_READ_FUSES;
	if (!ExecuteHIDCommandWithResponse(handle, &cmd, buffer, sizeof(buffer)))
		return false;
	target_mcu_fuses[0] = buffer[0];
	target_mcu_fuses[1] = buffer[1];
	target_mcu_fuses[2] = buffer[2];
	target_mcu_fuses[3] = buffer[3];
	target_mcu_fuses[4] = buffer[4];
	target_mcu_fuses[5] = buffer[5];
	quiet_printf("MCU fuses:\t%02X %02X %02X %02X %02X %02X\n", target_mcu_fuses[0], target_mcu_fuses[1], target_mcu_fuses[2], target_mcu_fuses[3], target_mcu_fuses[4], target_mcu_fuses[5]);

	return true;
}