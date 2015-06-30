
/*
 * misc.S
 *
 * Created: 30/06/2015 15:12:43
 *  Author: MoJo
 */ 

 #include <avr/io.h>

 .global reset_mcu

reset_mcu:
	push	r16
	push	ZH
	push	ZL

	cli

#ifdef RAMPZ
	ldi		r16, 0
	out		RAMPZ, r16
#endif

	// reset
	ldi		ZH, RST_CTRL >> 8
	ldi		ZL, RST_CTRL & 0xFF
	ldi		r16, 0xD8				// magic number for CCP register
	out		CCP, r16
	st		Z, RST_SWRST_bm
	nop

	// failed, return
	sei

	pop		ZL
	pop		ZH
	pop		r16
	ret