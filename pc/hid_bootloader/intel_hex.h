// intel_hex.h

#ifndef __INTEL_HEX_H
#define __INTEL_HEX_H


#define	FIRMWARE_BUFFER_SIZE		(128*1024)


extern uint8_t firmware_buffer[FIRMWARE_BUFFER_SIZE];
extern uint32_t firmware_crc;


extern bool ReadHexFile(char *filename);


#endif