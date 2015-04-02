# hid_bootloader

(C) 2015 Paul Qureshi
Licence: GPL 3.0

USB HID bootloader for Atmel XMEGA devices. Needs an 8k bootloader section because the Atmel ASF USB stack is about 6k on its own. The use of HID avoids the need for drivers on most operating systems.

PC software uses HIDAPI (http://github.com/signal11/hidapi/commits/master).

Currently supports 128k devices. Modify as required for other memory sizes.
