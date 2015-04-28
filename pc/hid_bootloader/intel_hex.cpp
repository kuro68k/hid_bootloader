// intel_hex.cpp

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "intel_hex.h"
#include "crc.h"


uint8_t firmware_buffer[FIRMWARE_BUFFER_SIZE];
uint32_t firmware_crc = 0;
uint32_t firmware_size = 0;
FW_INFO_t *info = NULL;


/**************************************************************************************************
* Find the embedded FW_INFO_t struct by it's signature in the firmware image. Returns the address
* of the image, or 0xFFFFFFFF if not found.
*/
uint32_t FindEmbeddedInfo(void)
{
	uint32_t	addr;

	for (addr = 0; addr < (FIRMWARE_BUFFER_SIZE - 8); addr++)
	{
		if (memcmp(&firmware_buffer[addr], MAGIC_STRING, 8) == 0)
			return addr;
	}
	return 0xFFFFFFFF;
}

uint32_t ReadBase16(char *c, int num_chars)
{
	uint32_t val = 0;

	while (num_chars--)
	{
		val += (*c <= '9' ? *c - '0' : *c - 'A' + 10) << (num_chars * 4);
		c++;
	}

	return val;
}

// load an Intel hex file into buffer
bool ReadHexFile(char *filename)
{
	FILE *fp;
	bool res = true;

	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		printf("Unable to open %s.\n", filename);
		return false;
	}

	memset(firmware_buffer, 0xFF, sizeof(firmware_buffer));
	uint32_t	base_addr = 0;

	int line_num = 0;
	while (feof(fp) != EOF)
	{
		line_num++;
		char line[1024];
		if (fgets(line, sizeof(line), fp) == NULL)
			break;
		//printf("%u:\t%s", line_num, line);

		if (line[0] != ':')
		{
			printf("Invalid line %d (missing colon)\n", line_num);
			res = false;
			break;
		}

		char *c = &line[1];
		uint8_t len = ReadBase16(c, 2);
		c += 2;
		uint16_t addr = ReadBase16(c, 4);
		c += 4;
		uint8_t type = ReadBase16(c, 2);
		c += 2;

		//printf("%u\t%X\t%u\n", len, addr, type);

		switch (type)
		{
		case 0:		// data record
			for (uint16_t i = 0; i < len; i++)
			{
				uint32_t absadr = base_addr + (addr++);
				if (absadr > FIRMWARE_BUFFER_SIZE)
				{
					printf("Firmware image too large for buffer (%X).\n", absadr);
					res = false;
					goto exit;
				}
				firmware_buffer[absadr] = ReadBase16(c, 2);
				c += 2;
				if (absadr > firmware_size)
					firmware_size = absadr;
			}
			break;

		case 2:		// extended segment address record
			if (len != 2)
			{
				printf("Invalid line %d (bad extended segment address length: %u)\n", line_num, len);
				res = false;
				break;
			}
			base_addr = ReadBase16(c, 4) << 4;
			//printf("%u:\tbase_addr = %X\n", line_num, base_addr);
			c += 4;
		}

		uint8_t checksum = ReadBase16(c, 2);
		// todo: check checksum

		if (res != true)
			break;
	}

	printf("Firmware size:\t%u bytes (0x%X)\n", firmware_size, firmware_size);
	/*
	// find embedded info
	uint32_t ptr = FindEmbeddedInfo();
	if (ptr == 0xFFFFFFFF)
	{
		printf("Embedded info struct not found.\n");
		info = NULL;
		res = false;
		goto exit;
	}
	info = (FW_INFO_t *)&firmware_buffer[ptr];
	if (info->flash_size_b > FIRMWARE_BUFFER_SIZE)
	{
		printf("Embedded flash size greater than buffer size.\n");
		res = false;
		goto exit;
	}
	printf("Flash size:\t%u bytes (0x%X)\n", info->flash_size_b, info->flash_size_b);
	*/
	FW_INFO_t zzz;
	info = &zzz;
	info->flash_size_b = 0x20000;
	firmware_crc = crc32(firmware_buffer, info->flash_size_b);
	printf("Firmware CRC:\t%lX\n", firmware_crc);

exit:
	fclose(fp);
	return res;
}