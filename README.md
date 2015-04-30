# hid_bootloader

(C) 2015 Paul Qureshi
Licence: GPL 3.0

USB HID bootloader for Atmel XMEGA devices. Needs an 8k bootloader section because the Atmel ASF USB stack is about 6k on its own. The use of HID avoids the need for drivers on most operating systems.

PC host software uses HIDAPI (http://github.com/signal11/hidapi/commits/master).

Supports up to 256k devices.

Also included is a demonstration firmware (test_image) for bootloading, which includes an embedded FW_INFO_t struct. This struct includes some basic information about the firmware, such as the target MCU, which is checked by the host software.

Note that the XMEGA NVM controller's CRC function uses an odd variant of the more common CRC32. An implementation is included in the host software.
