/*
 * serial_num.c
 *
 * Created: 22/04/2015 11:24:05
 *  Author: paul.qureshi
 */ 

#include <avr/io.h>
#include <asf.h>

#include "sp_driver.h"
#include "serial_num.h"

uint8_t USB_serial_number[USB_DEVICE_GET_SERIAL_NAME_LENGTH];

/**************************************************************************************************
** Convert lower nibble to hex char
*/
uint8_t usb_hex_to_char(uint8_t hex)
{
	if (hex < 10)
		hex += '0';
	else
		hex += 'A' - 10;
	
	return(hex);
}

/**************************************************************************************************
** Set USB serial number string from MCU ID
*/
void USB_init_build_usb_serial_number(void)
{
	uint8_t	i;
	uint8_t	j = 0;
	uint8_t b;
	
	for (i = 0; i < 6; i++)
	{
		b = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0) + i);
		USB_serial_number[j++] = usb_hex_to_char(b >> 4);
		USB_serial_number[j++] = usb_hex_to_char(b & 0x0F);
	}
	USB_serial_number[j++] = '-';
	b = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0) + 7);
	USB_serial_number[j++] = usb_hex_to_char(b >> 4);
	USB_serial_number[j++] = usb_hex_to_char(b & 0x0F);
	USB_serial_number[j++] = '-';

	for (i = 7; i < 11; i++)
	{
		b = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0) + i);
		USB_serial_number[j++] = usb_hex_to_char(b >> 4);
		USB_serial_number[j++] = usb_hex_to_char(b & 0x0F);
	}

	USB_serial_number[j] = '\0';
}
