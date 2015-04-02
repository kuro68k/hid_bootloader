// crc.cpp

#include <stdint.h>
#include <stdlib.h>
#include "crc.h"


/**************************************************************************************************
* Reverse bits in a uint32
*/
uint32_t reverse(uint32_t x)
{
	x = ((x & 0x55555555) << 1) | ((x >> 1) & 0x55555555);
	x = ((x & 0x33333333) << 2) | ((x >> 2) & 0x33333333);
	x = ((x & 0x0F0F0F0F) << 4) | ((x >> 4) & 0x0F0F0F0F);
	x = (x << 24) | ((x & 0xFF00) << 8) |
		((x >> 8) & 0xFF00) | (x >> 24);
	return x;
}

/**************************************************************************************************
* Slow but correct CRC32
*/
uint32_t crc32(uint8_t *buffer, uint32_t buffer_length)
{
	int32_t i, j;
	uint32_t byte, crc;

	i = 0;
	crc = 0xFFFFFFFF;

	while (buffer_length--)
	{
		byte = *buffer++;
		byte = reverse(byte);

		for (j = 0; j < 8; j++)
		{
			if ((int32_t)(crc ^ byte) < 0)
				crc = (crc << 1) ^ 0x04C11DB7;
			else
				crc <<= 1;
			byte <<= 1;
		}
		i++;
	}

	return reverse(~crc);
}