/*
 * hid_bootloader.c
 *
 * Created: 29/03/2015 21:15:01
 *  Author: MoJo
 */ 


#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <asf.h>
#include "sp_driver.h"
#include "protocol.h"

uint8_t	page_buffer[APP_SECTION_PAGE_SIZE];

int main(void)
{
	sysclk_init();
	irq_initialize_vectors();
	cpu_irq_enable();
	udc_start();
	
	for(;;);
}

void calc_fw_crcs(uint32_t *app_crc, uint32_t *boot_crc)
{
	uint32_t address;

	// application
	CRC.CTRL = CRC_RESET_RESET1_gc;
	CRC.CTRL = CRC_CRC32_bm | CRC_SOURCE_IO_gc;
	for(address = APP_SECTION_START; address < (APP_SECTION_END + 1); address++)
		CRC.DATAIN = pgm_read_byte_far(address);
	CRC.STATUS = CRC_BUSY_bm;

	*app_crc = (uint32_t)CRC.CHECKSUM0 | ((uint32_t)CRC.CHECKSUM1 << 8) | ((uint32_t)CRC.CHECKSUM2 << 16) | ((uint32_t)CRC.CHECKSUM3 << 24);

	// bootloader
	CRC.CTRL = CRC_RESET_RESET1_gc;
	CRC.CTRL = CRC_CRC32_bm | CRC_SOURCE_IO_gc;
	for(address = BOOT_SECTION_START; address < (BOOT_SECTION_END + 1); address++)
		CRC.DATAIN = pgm_read_byte_far(address);
	CRC.STATUS = CRC_BUSY_bm;

	*boot_crc = (uint32_t)CRC.CHECKSUM0 | ((uint32_t)CRC.CHECKSUM1 << 8) | ((uint32_t)CRC.CHECKSUM2 << 16) | ((uint32_t)CRC.CHECKSUM3 << 24);

	CRC.CTRL = 0;
	RAMPZ = 0;		// clean up after pgm_read_byte_far()
}

void hid_report_out(uint8_t *report)
{
	uint8_t		response[32+2+1];
	response[0] = report[0] | 0x80;
	response[1] = report[1];
	response[2] = report[2];
	
	uint16_t	addr;
	addr = *(uint16_t *)(report+1);

	switch(report[0])
	{
		case CMD_NOP:
			break;
		
		case CMD_WRITE_BUFFER:
			if (addr > (APP_SECTION_PAGE_SIZE - 32))
				return;
			memcpy(&page_buffer[addr], &report[3], 32);
			return;		// no response to speed things up
		
		case CMD_READ_BUFFER:
			memcpy(&response[3], &page_buffer[addr], 32);
			break;
		
		case CMD_ERASE_APP_SECTION:
			SP_WaitForSPM();
			SP_EraseApplicationSection();
			break;
		
		case CMD_READ_APP_SECTION_CRC:
			SP_WaitForSPM();
			calc_fw_crcs((uint32_t *)&response[3], (uint32_t *)&response[7]);
			break;
		
		case CMD_READ_MCU_IDS:
			report[3] = MCU.DEVID0;
			report[3] = MCU.DEVID1;
			report[3] = MCU.DEVID2;
			report[3] = MCU.REVID;
			break;
		
		case CMD_READ_FUSES:
			response[3] = SP_ReadFuseByte(0);
			response[4] = SP_ReadFuseByte(1);
			response[5] = SP_ReadFuseByte(2);
			response[6] = 0xFF;
			response[7] = SP_ReadFuseByte(4);
			response[8] = SP_ReadFuseByte(5);
			break;
		
		case CMD_WRITE_PAGE:
			SP_WaitForSPM();
			SP_LoadFlashPage(page_buffer);
			SP_WriteApplicationPage(APP_SECTION_START + ((uint32_t)addr * APP_SECTION_PAGE_SIZE));
			break;
		
		case CMD_READ_PAGE:
			if (addr > (APP_SECTION_SIZE / APP_SECTION_PAGE_SIZE))
			{
				response[1] = 0xFF;
				response[2] = 0xFF;
			}
			else
			{
				memcpy_P(page_buffer, (const void *)((APP_SECTION_START) + (APP_SECTION_PAGE_SIZE * addr)), APP_SECTION_PAGE_SIZE);
				memcpy(&response[3], page_buffer, 32);
			}
			break;
		
		case CMD_ERASE_USER_SIG_ROW:
			SP_WaitForSPM();
			SP_EraseUserSignatureRow();
			break;
		
		case CMD_WRITE_USER_SIG_ROW:
			SP_WaitForSPM();
			SP_LoadFlashPage(page_buffer);
			SP_WriteUserSignatureRow();
			break;

		case CMD_READ_USER_SIG_ROW:
			if (addr > (USER_SIGNATURES_PAGE_SIZE - 32))
			{
				response[1] = 0xFF;
				response[2] = 0xFF;
			}
			else
			{
				memcpy_P(page_buffer, (const void *)(USER_SIGNATURES_SIZE + addr), USER_SIGNATURES_SIZE);
				memcpy(&response[3], page_buffer, 32);
			}
			break;
		
		default:
			response[0] = 0xFF;
			break;
	}

	udi_hid_generic_send_report_in(response);
}