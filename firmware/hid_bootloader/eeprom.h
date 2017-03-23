/*
 * eeprom.h
 *
 */


#ifndef EEPROM_H
#define EEPROM_H



/**************************************************************************************************
** Macros and inline functions
*/

// get the address of data in EEPROM when memory mapping is enabled
#define	EEP_MAPPED_ADDR(page, byte)	(MAPPED_EEPROM_START + (EEPROM_PAGE_SIZE * page) + byte)

#define EEP_EnablePowerReduction()	( NVM.CTRLB |= NVM_EPRM_bm )
#define EEP_DisablePowerReduction() ( NVM.CTRLB &= ~NVM_EPRM_bm )
#define EEP_EnableMapping()			( NVM.CTRLB |= NVM_EEMAPEN_bm )
#define EEP_DisableMapping()		( NVM.CTRLB &= ~NVM_EEMAPEN_bm )

// Execute NVM command. Timing critical, temporarily suspends interrupts.
// Atmel did a horrible job with this code, but it works so no point fixing it.
#define NVM_EXEC()	asm("push r30"      "\n\t"	\
					    "push r31"      "\n\t"	\
    					"push r16"      "\n\t"	\
    					"push r18"      "\n\t"	\
						"ldi r30, 0xCB" "\n\t"	\
						"ldi r31, 0x01" "\n\t"	\
						"ldi r16, 0xD8" "\n\t"	\
						"ldi r18, 0x01" "\n\t"	\
						"out 0x34, r16" "\n\t"	\
						"st Z, r18"	    "\n\t"	\
    					"pop r18"       "\n\t"	\
						"pop r16"       "\n\t"	\
						"pop r31"       "\n\t"	\
						"pop r30"       "\n\t"	\
					    )


// Wait for NVM access to finish.
inline static void EEP_WaitForNVM(void)
{
	while ((NVM.STATUS & NVM_NVMBUSY_bm) == NVM_NVMBUSY_bm)
		;
}


/**************************************************************************************************
** Externally accessible functions
*/
extern void EEP_WaitForNVM(void);
extern void EEP_LoadPageBuffer(const uint8_t *data, uint8_t size)	__attribute__((nonnull));
extern void EEP_AtomicWritePage(uint8_t page_addr);
extern void EEP_EraseAll(void);



#endif
