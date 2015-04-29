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

#define BOOTLOADER_VERSION	1

typedef void (*AppPtr)(void) __attribute__ ((noreturn));

uint8_t		page_buffer[APP_SECTION_PAGE_SIZE + UDI_HID_REPORT_OUT_SIZE];	// needed size + safety buffer
uint16_t	page_ptr = 0;

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
* Handle received HID set feature reports
*/
void HID_set_feature_report_out(uint8_t *report)
{
	uint8_t		response[UDI_HID_REPORT_OUT_SIZE];
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
		case CMD_RESET_POINTER:
			page_ptr = 0;
			return;

		// read from RAM page buffer
		case CMD_READ_BUFFER:
			memcpy(response, &page_buffer[page_ptr], UDI_HID_REPORT_OUT_SIZE);
			page_ptr += UDI_HID_REPORT_OUT_SIZE;
			page_ptr &= APP_SECTION_PAGE_SIZE-1;
			break;

		// erase entire application section
		case CMD_ERASE_APP_SECTION:
			SP_WaitForSPM();
			SP_EraseApplicationSection();
			return;

		// calculate application and bootloader section CRCs
		case CMD_READ_FLASH_CRCS:
			SP_WaitForSPM();
			*(uint32_t *)&response[3] = SP_ApplicationCRC();
			*(uint32_t *)&response[7] = SP_BootCRC();
			break;

		// read MCU IDs
		case CMD_READ_MCU_IDS:
			response[3] = MCU.DEVID0;
			response[4] = MCU.DEVID1;
			response[5] = MCU.DEVID2;
			response[6] = MCU.REVID;
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
			page_ptr = 0;
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
				page_ptr = 0;
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
				page_ptr = 0;
			}
			break;

		case CMD_READ_SERIAL:
			{
				uint8_t	i;
				uint8_t	j = 3;
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
		
		case CMD_READ_BOOTLOADER_VERSION:
			response[3] = BOOTLOADER_VERSION;
			break;
		
		// unknown command
		default:
			response[0] = 0xFF;
			break;
	}

	udi_hid_generic_send_report_in(response);
}
