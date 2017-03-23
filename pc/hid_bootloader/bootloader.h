// bootloader.h

#define	CMD_NOP							0x00

//#define CMD_RESET_POINTER				0x01
#define CMD_SET_POINTER					0x01

//#define CMD_READ_BUFFER					0x02
#define CMD_READ_FLASH					0x02

#define CMD_ERASE_APP_SECTION			0x03
#define CMD_READ_FLASH_CRCS				0x04
#define CMD_READ_MCU_IDS				0x05
#define CMD_READ_FUSES					0x06
#define CMD_WRITE_PAGE					0x07
//#define CMD_READ_PAGE					0x08
#define CMD_ERASE_USER_SIG_ROW			0x09
#define CMD_WRITE_USER_SIG_ROW			0x0A
#define CMD_READ_USER_SIG_ROW			0x0B
#define CMD_READ_SERIAL					0x0C
//#define CMD_READ_BOOTLOADER_VERSION		0x0D
#define CMD_RESET_MCU					0x0E
#define CMD_READ_EEPROM					0x0F
//#define CMD_WRITE_EEPROM				0x10
#define CMD_WRITE_EEPROM_PAGE			0x10
#define CMD_READ_EEPROM_CRC				0x11


#define	APP_SECTION_ERASE_TIMEOUT_MS	100
#define	READ_FLASH_CRCS_TIMEOUT_MS		5000


#define	HID_DATA_BYTES					64


#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )

PACK(
typedef struct
{
	uint8_t	report_id;
	uint8_t version;
	uint8_t busy_flags;
	uint16_t page_ptr;
	uint8_t result;
} BLSTATUS_t;
)

PACK(
typedef struct
{
	uint8_t	report_id;
	uint8_t	command;
	union
	{
		uint32_t u32;
		uint16_t u16[2];
		uint8_t u8[4];
	} params;
} BLCOMMAND_t;
)
