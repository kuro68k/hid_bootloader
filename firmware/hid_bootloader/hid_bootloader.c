/*
 * hid_bootloader.c
 *
 */


#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <asf.h>
#include "eeprom.h"
#include "sp_driver.h"
#include "protocol.h"

#define BOOTLOADER_VERSION	1

typedef void (*AppPtr)(void) __attribute__ ((noreturn));

struct {
	uint8_t version;
	uint8_t busy_flags;
	uint16_t page_ptr;
	uint8_t result;
} feature_response = { .version = BOOTLOADER_VERSION };

typedef struct
{
//	uint8_t	report_id;
	uint8_t	command;
	union
	{
		uint32_t u32;
		uint16_t u16[2];
		uint8_t u8[4];
	} params;
} BLCOMMAND_t;


uint8_t		page_buffer[APP_SECTION_PAGE_SIZE + UDI_HID_REPORT_OUT_SIZE];	// needed size + safety buffer
uint16_t	page_ptr = 0;

//uint8_t		feature_response[5];

/**************************************************************************************************
* Main entry point
*/
int main(void)
{
	CCP = CCP_IOREG_gc;				// unlock IVSEL
	PMIC.CTRL |= PMIC_IVSEL_bm;		// set interrupt vector table to bootloader section

	// check for VUSB
	PORTD.DIRCLR = PIN5_bm;
	PORTD.PIN5CTRL = (PORTD.PIN5CTRL & ~PORT_OPC_gm) | PORT_OPC_PULLDOWN_gc;
	_delay_ms(1);
	if (0)
	//if (!(PORTD.IN & PIN5_bm))	// not connected to USB
	{
		// exit bootloader
		AppPtr application_vector = (AppPtr)0x000000;
		CCP = CCP_IOREG_gc;		// unlock IVSEL
		PMIC.CTRL = 0;			// disable interrupts, set vector table to app section
		EIND = 0;				// indirect jumps go to app section
		RAMPZ = 0;				// LPM uses lower 64k of flash
		application_vector();
	}

	// set up USB HID bootloader interface
	sysclk_init();
	irq_initialize_vectors();
	cpu_irq_enable();
	udc_start();
	udc_attach();

	for(;;);
}

/**************************************************************************************************
** Convert lower nibble to hex char
*/
uint8_t hex_to_char(uint8_t hex)
{
	if (hex < 10)
		hex += '0';
	else
		hex += 'A' - 10;

	return(hex);
}

/**************************************************************************************************
* Handle received HID report out requests
*/
void HID_report_out(uint8_t *report)
{
	memcpy(&page_buffer[page_ptr], report, UDI_HID_REPORT_OUT_SIZE);
	page_ptr += UDI_HID_REPORT_OUT_SIZE;
	page_ptr &= APP_SECTION_PAGE_SIZE-1;
}

/**************************************************************************************************
* Handle received HID get feature reports
*/
bool HID_get_feature_report_out(uint8_t **payload, uint16_t *size)
{
	feature_response.busy_flags = NVM.STATUS & (~NVM_FLOAD_bm);
	feature_response.page_ptr = page_ptr;
	*payload = (uint8_t *)&feature_response;
	*size = sizeof(feature_response);

	return true;
}

/**************************************************************************************************
* Handle received HID set feature reports
*/
void HID_set_feature_report_out(uint8_t *report)
{
	BLCOMMAND_t *cmd = (BLCOMMAND_t *)report;
	uint8_t		response[UDI_HID_REPORT_OUT_SIZE];
	feature_response.result = 0;

	switch(cmd->command)
	{
		// no-op
		case CMD_NOP:
			return;

		// write to RAM page buffer
		case CMD_SET_POINTER:
			page_ptr = cmd->params.u16[0];
			return;

		// read from RAM page buffer
/*		case CMD_READ_BUFFER:
			memcpy(response, &page_buffer[page_ptr], UDI_HID_REPORT_OUT_SIZE);
			page_ptr += UDI_HID_REPORT_OUT_SIZE;
			page_ptr &= APP_SECTION_PAGE_SIZE-1;
			break;
*/
		// read flash
		case CMD_READ_FLASH:
			if (cmd->params.u32 > APP_SECTION_SIZE)
			{
				feature_response.result = -1;
				return;
			}
			memcpy_PF(response, (uint_farptr_t)cmd->params.u32, sizeof(response));
			break;

		// erase entire application section
		case CMD_ERASE_APP_SECTION:
			SP_WaitForSPM();
			SP_EraseApplicationSection();
			return;

		// calculate application and bootloader section CRCs
		case CMD_READ_FLASH_CRCS:
			SP_WaitForSPM();
			*(uint32_t *)&response[0] = SP_ApplicationCRC();
			*(uint32_t *)&response[4] = SP_BootCRC();
			response[8] = 0xA5;
			break;

		// read MCU IDs
		case CMD_READ_MCU_IDS:
			response[0] = MCU.DEVID0;
			response[1] = MCU.DEVID1;
			response[2] = MCU.DEVID2;
			response[3] = MCU.REVID;
			break;

		// read fuses
		case CMD_READ_FUSES:
			memset(response, 0, 6);
			#ifdef FUSE_FUSEBYTE0
			response[0] = SP_ReadFuseByte(0);
			#endif
			#ifdef FUSE_FUSEBYTE1
			response[1] = SP_ReadFuseByte(1);
			#endif
			#ifdef FUSE_FUSEBYTE2
			response[2] = SP_ReadFuseByte(2);
			#endif
			#ifdef FUSE_FUSEBYTE3
			response[3] = SP_ReadFuseByte(3);
			#endif
			#ifdef FUSE_FUSEBYTE4
			response[4] = SP_ReadFuseByte(4);
			#endif
			#ifdef FUSE_FUSEBYTE5
			response[5] = SP_ReadFuseByte(5);
			#endif
			break;

		// write RAM page buffer to application section page
		case CMD_WRITE_PAGE:
			if (cmd->params.u16[0] > (APP_SECTION_SIZE / APP_SECTION_PAGE_SIZE))	// out of range
			{
				feature_response.result = -1;
				return;
			}
			SP_WaitForSPM();
			SP_LoadFlashPage(page_buffer);
			SP_WriteApplicationPage(APP_SECTION_START + ((uint32_t)cmd->params.u16[0] * APP_SECTION_PAGE_SIZE));
			page_ptr = 0;
			break;

		// erase user signature row
		case CMD_ERASE_USER_SIG_ROW:
			SP_WaitForSPM();
			SP_EraseUserSignatureRow();
			break;

		// write RAM buffer to user signature row
		case CMD_WRITE_USER_SIG_ROW:
			SP_WaitForSPM();
			SP_LoadFlashPage(page_buffer);
			SP_WriteUserSignatureRow();
			break;

		// read user signature row
		case CMD_READ_USER_SIG_ROW:
			if (cmd->params.u16[0] > USER_SIGNATURES_PAGE_SIZE)
			{
				feature_response.result = -1;
				return;
			}
			for (uint8_t i = 0; i < sizeof(response); i++)
				response[i] = SP_ReadUserSignatureByte(cmd->params.u16[0] + i);
			break;

		case CMD_READ_SERIAL:
			{
				uint8_t	i;
				uint8_t	j = 0;
				uint8_t b;

				for (i = 0; i < 6; i++)
				{
					b = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0) + i);
					response[j++] = hex_to_char(b >> 4);
					response[j++] = hex_to_char(b & 0x0F);
				}
				response[j++] = '-';
				b = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0) + 6);
				response[j++] = hex_to_char(b >> 4);
				response[j++] = hex_to_char(b & 0x0F);
				response[j++] = '-';

				for (i = 7; i < 11; i++)
				{
					b = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0) + i);
					response[j++] = hex_to_char(b >> 4);
					response[j++] = hex_to_char(b & 0x0F);
				}

				response[j] = '\0';
				break;
			}

		case CMD_RESET_MCU:
			//reset_do_soft_reset();
			ccp_write_io((void *)&WDT.CTRL, WDT_PER_128CLK_gc | WDT_WEN_bm | WDT_CEN_bm);	// watchdog will reset us in ~128ms
			break;

		case CMD_READ_EEPROM:
			if (cmd->params.u16[0] > EEPROM_SIZE)
			{
				feature_response.result = -1;
				return;
			}
			EEP_EnableMapping();
			memcpy(response, (const void *)(MAPPED_EEPROM_START + cmd->params.u16[0]), sizeof(response));
			EEP_DisableMapping();
			break;

		case CMD_WRITE_EEPROM_PAGE:
			if (cmd->params.u16[0] > (EEPROM_SIZE / EEPROM_PAGE_SIZE))
			EEP_LoadPageBuffer(page_buffer, EEPROM_PAGE_SIZE);
			EEP_AtomicWritePage(cmd->params.u16[0]);
			break;

		case CMD_READ_EEPROM_CRC:
			CRC.CTRL = CRC_RESET_RESET1_gc;
			CRC.CTRL = CRC_SOURCE_FLASH_gc | CRC_CRC32_bm;
			uint8_t *ptr = (uint8_t *)MAPPED_EEPROM_START;
			uint16_t i = EEPROM_SIZE;
			EEP_EnableMapping();
			while (i--)
				CRC.DATAIN = *ptr++;
			EEP_DisableMapping();
			CRC.STATUS = CRC_BUSY_bm;
			response[0] = CRC.CHECKSUM0;
			response[1] = CRC.CHECKSUM1;
			response[2] = CRC.CHECKSUM2;
			response[3] = CRC.CHECKSUM3;
			break;

		// unknown command
		default:
			feature_response.result = -1;
			return;
	}

	udi_hid_generic_send_report_in(response);
}
