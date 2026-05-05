# STM32F103 USB HID keyboard & (relative + absolute) mouse

This is a minimal, bare metal USB HID keyboard & mouse stack for the STM32F103 based on Bruno Freitas' USB HID bootloader.  
The entire USB + HID stack (excluding the descriptors) is less than 700 SLOC.

It shows how to have a USB HID device with multiple interfaces (3 of them, in this case).

It also shows how to set up a USB HID device with both a relative and an absolute mouse that works on Linux.
(Since adding an absolute and a relative mouse on the same endpoint doesn't seem to work on Linux.
I don't know if having them on separate endpoints but on the same interface works.)

In this example,
- interface 0 (endpoint 1) has a 6KRO boot keyboard
- interface 1 (endpoint 2) has an NKRO keyboard and a relative mouse
- interface 2 (endpoint 3) has an absolute mouse (INPUT) and an OUTPUT buffer (for the host to send data to the device).

Of course, there's also an IN and OUT endpoint 0.

After you flash the firmware into your device and connect to the the host, every second it'll send an 'a' keypress via endpoint 1, a 'b' keypress via endpoint 2, and an absolute mouse command via endpoint 3.

If you write data to the device, it'll send a 'cd' keypress (see `hidwrite.py`).  
For some reason, it doesn't work well if the device is sending data (as in the previous paragraph), so comment out that part in `main.c` to test writing to the device.  
If anyone knows how to fix this, I'd be happy to know.

Tested on a STM32F103CB with a 16 MHz crystal.  
To use it with an 8 MHz crystal, delete `RCC_CFGR_PLLXTPRE_HSE_Div2` from `main.c`.

To build the firmware, run `make`.
