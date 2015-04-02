// intel_hex.cpp

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "intel_hex.h"
#include "crc.h"


uint8_t firmware_buffer[FIRMWARE_BUFFER_SIZE];
uint32_t firmware_crc = 0;


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
	while (!feof(fp))
	{
		line_num++;
		char line[1024];
		if (fgets(line, sizeof(line), fp) == NULL)
			break;

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
				if (addr > FIRMWARE_BUFFER_SIZE)
					break;
				firmware_buffer[base_addr + (addr++)] = ReadBase16(c, 2);
				c += 2;
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
			c += 4;
		}

		uint8_t checksum = ReadBase16(c, 2);
		// todo: check checksum

		if (res != true)
			break;
	}

	fclose(fp);
	firmware_crc = crc32(firmware_buffer, FIRMWARE_BUFFER_SIZE);
	printf("Firmware CRC:\t%lX\n", firmware_crc);

	return res;
}