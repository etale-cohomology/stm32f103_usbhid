MCU    = STM32F10X_MD
SPECS  = -specs=nano.specs -specs=nosys.specs
ARCH   = -mcpu=cortex-m3 -mthumb
CFLAGS = -Wall -Wno-unused-variable -static -ffunction-sections -fdata-sections  -Wno-comment -Wno-unused-function

all: kb

startup_stm32f10x_md.o: startup_stm32f10x_md.s
	arm-none-eabi-as  -c $< -o $@ $(ARCH)

usbhid.o: usbhid.c usbhid.h
	arm-none-eabi-gcc -c $< -o $@  -Os $(ARCH) $(CFLAGS) $(SPECS) -D$(MCU)  # -I ../CMSIS/Device/ST/STM32F10x/Include  -I ../CMSIS/Include

main.o: main.c
	arm-none-eabi-gcc -c $< -o $@  -Os $(ARCH) $(CFLAGS) $(SPECS) -D$(MCU)  #-I ../CMSIS/Device/ST/STM32F10x/Include  -I ../CMSIS/Include

kb: startup_stm32f10x_md.o usbhid.o main.o
	tput rmam
	arm-none-eabi-gcc  $^  -o $@.elf  -Wl,-Map=$@.map,--cref -Wl,--gc-sections -Wl,--print-memory-usage -TSTM32F103C8T6.ld $(ARCH) $(CFLAGS) -Os $(SPECS)  -I ../CMSIS/Device/ST/STM32F10x/Include  -I ../CMSIS/Include  -D$(MCU)
	arm-none-eabi-objcopy  -O binary $@.elf $@.bin
	rm -rf *.o *.elf $@.map

flash: kb
	st-flash --reset write kb.bin 0x08000000
	# st-flash --reset --connect-under-reset write wb1.bin 0x08000000

clean:
	rm -f *.elf *.bin *.map *.hex *.lst *.dis *.size *.o
