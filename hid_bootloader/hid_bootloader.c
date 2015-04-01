/*
 * hid_bootloader.c
 *
 * Created: 29/03/2015 21:15:01
 *  Author: MoJo
 */ 


#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <asf.h>
#include "sp_driver.h"
#include "protocol.h"

typedef void (*AppPtr)(void) __attribute__ ((noreturn));

uint8_t	page_buffer[APP_SECTION_PAGE_SIZE];

/**************************************************************************************************
* Main entry point
*/
int main(void)
{
	// check for VUSB
	PORTD.DIRCLR = PIN5_bm;
	PORTD.PIN5CTRL = (PORTD.PIN5CTRL & ~PORT_OPC_gm) | PORT_OPC_PULLDOWN_gc;
	_delay_ms(1);
	if (!(PORTD.IN & PIN5_bm))	// not connected to USB
	{
		// exit bootloader
		AppPtr application_vector = (AppPtr)0x000000;
		CCP = CCP_IOREG_gc;		// unlock IVSEL
		PMIC.CTRL = 0;			// disable interrupts
		EIND = 0;				// indirect jumps go to app section
		RAMPZ = 0;				// LPM uses lower 64k of flash
		application_vector();
	}
	
	// set up USB HID bootloader interface
	sysclk_init();
	irq_initialize_vectors();
	cpu_irq_enable();
	udc_start();
	
	for(;;);
}

/**************************************************************************************************
* Calculate app section and bootloader CRC32 values
*/
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

/**************************************************************************************************
* Handle received HID reports
*/
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
		// no-op
		case CMD_NOP:
			break;
		
		// write to RAM page buffer
		case CMD_WRITE_BUFFER:
			if (addr > (APP_SECTION_PAGE_SIZE - 32))
				return;
			memcpy(&page_buffer[addr], &report[3], 32);
			return;		// no response to speed things up
		
		// read from RAM page buffer
		case CMD_READ_BUFFER:
			memcpy(&response[3], &page_buffer[addr], 32);
			break;
		
		// erase entire application section
		case CMD_ERASE_APP_SECTION:
			SP_WaitForSPM();
			SP_EraseApplicationSection();
			break;
		
		// calculate application and bootloader section CRCs
		case CMD_READ_FLASH_CRCS:
			SP_WaitForSPM();
			calc_fw_crcs((uint32_t *)&response[3], (uint32_t *)&response[7]);
			break;
		
		// read MCU IDs
		case CMD_READ_MCU_IDS:
			report[3] = MCU.DEVID0;
			report[3] = MCU.DEVID1;
			report[3] = MCU.DEVID2;
			report[3] = MCU.REVID;
			break;
		
		// read fuses
		case CMD_READ_FUSES:
			response[3] = SP_ReadFuseByte(0);
			response[4] = SP_ReadFuseByte(1);
			response[5] = SP_ReadFuseByte(2);
			response[6] = 0xFF;
			response[7] = SP_ReadFuseByte(4);
			response[8] = SP_ReadFuseByte(5);
			break;
		
		// write RAM page buffer to application section page
		case CMD_WRITE_PAGE:
			if (addr > (APP_SECTION_SIZE / APP_SECTION_PAGE_SIZE))	// out of range
			{
				response[1] = 0xFF;
				response[2] = 0xFF;
				break;
			}
			SP_WaitForSPM();
			SP_LoadFlashPage(page_buffer);
			SP_WriteApplicationPage(APP_SECTION_START + ((uint32_t)addr * APP_SECTION_PAGE_SIZE));
			break;
		
		// read application page to RAM buffer and return first 32 bytes
		case CMD_READ_PAGE:
			if (addr > (APP_SECTION_SIZE / APP_SECTION_PAGE_SIZE))	// out of range
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

		// read user signature row to RAM buffer and return first 32 bytes
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
		
		// unknown command
		default:
			response[0] = 0xFF;
			break;
	}

	udi_hid_generic_send_report_in(response);
}
