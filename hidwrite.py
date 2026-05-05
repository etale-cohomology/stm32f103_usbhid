#!/usr/bin/env py
import hid

for device in hid.enumerate():
	keys = list(device.keys())
	keys.sort()
	if 'mathisart'!=device['manufacturer_string'].lower() or device['interface_number']!=2 or device['usage_page']!=1 or device['usage']!=6:  continue
	print(device['path'], device['interface_number'], device['usage_page'], device['usage'])
	h = hid.Device(path=device['path'])
	h.write(b'\x60' + b'\x00' + b'\x00'*62)
	h.close()
