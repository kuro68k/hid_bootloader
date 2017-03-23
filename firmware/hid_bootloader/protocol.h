/*
 * protocol.h
 *
 */


#ifndef PROTOCOL_H_
#define PROTOCOL_H_


#define	CMD_NOP						0x00

//#define CMD_RESET_POINTER			0x01
#define CMD_SET_POINTER				0x01

//#define CMD_READ_BUFFER				0x02
#define CMD_READ_FLASH				0x02

#define CMD_ERASE_APP_SECTION		0x03
#define CMD_READ_FLASH_CRCS			0x04
#define CMD_READ_MCU_IDS			0x05
#define CMD_READ_FUSES				0x06
#define CMD_WRITE_PAGE				0x07
//#define CMD_READ_PAGE				0x08
#define CMD_ERASE_USER_SIG_ROW		0x09
#define CMD_WRITE_USER_SIG_ROW		0x0A
#define CMD_READ_USER_SIG_ROW		0x0B
#define CMD_READ_SERIAL				0x0C
//#define CMD_READ_BOOTLOADER_VERSION	0x0D
#define CMD_RESET_MCU				0x0E
#define CMD_READ_EEPROM				0x0F
//#define CMD_WRITE_EEPROM			0x10
#define CMD_WRITE_EEPROM_PAGE		0x10
#define CMD_READ_EEPROM_CRC			0x11



#endif /* PROTOCOL_H_ */