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


extern int HID_mode(hid_device *handle);
extern int LIBUSB_mode(unsigned short vid, unsigned short pid);


bool ExecuteHIDCommand(hid_device *handle, BLCOMMAND_t *cmd);
bool ExecuteHIDCommandWithResponse(hid_device *handle, BLCOMMAND_t *cmd, uint8_t *buffer, uint8_t buffer_size);
bool UpdateFirmware(hid_device *handle);
bool VerifyFirmware(hid_device *handle);
bool GetBootloaderInfo(hid_device *handle);


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
	int res = parse_args(argc, argv);
	if (res != 0)
		return res;

	// read .hex file
	if (!ReadHexFile(hexfile))
		return 1;

	// find target HID device
	hid_device *hid_handle;
	hid_handle = hid_open(vid, pid, NULL);

	if (hid_handle != NULL)
		return HID_mode(hid_handle);
	else
		return LIBUSB_mode(vid, pid);
}
