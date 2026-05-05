/* WORKS! THANK YOU, LORD GOD JESUS!!!!!!!!
make flash
*/
#include <string.h>
#include "usbhid.h"

typedef struct{  // @meta  input report: boot keyboard
	u8 mod;
	u8 pad;
	u8 keys[6];
}__attribute__((packed)) hid_irp_bkb_t;

typedef struct{  // @meta  input report: classical (and finicky) HID nkro kb input report, which the Linux HID driver recognizes (if you remove even the LED byte, it STOPS recognizing it)
	u8 id;  // report ID
	u8 mods;
	u8 keys[30];
}__attribute__((packed)) hid_irp_kb_t;

typedef struct{  // @meta  input report: 8-bit mouse, relative
	u8 id;
	u8 mask;
	u8 x,y;
	u8 wy;  // wheel y
	u8 wx;  // wheel x
}__attribute__((packed)) hid_irp_msr_t;

typedef struct{  // @meta  input report: 16-bit mouse, relative
	u8  mask;
	u16 x,y;
}__attribute__((packed)) hid_irp_msa16_t;

void usb_ini(void (*EPHandlerPtr)(u16), void (*ResetHandlerPtr)());
void usb_datasend(u8 EPn, u16* Data, u16 Length);
void hid_reset();
void hid_ep_handle(u16 Status);

extern volatile u16 DeviceConfigured;

// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  stm32 system. system_stm32f10x.c @author  MCD Application Team. COPYRIGHT 2016 STMicroelectronics. Licensed under MCD-ST Liberty SW License Agreement V2
// 1. this has 2 fns and 1 global variable to be called from the usr app: 1.1) SystemInit(): set up the sys clock (sys clock source, PLL Multiplier factors, AHB/APBx prescalers and Flash settings), called at startup just after reset and before branch to main app. This call is made in startup_stm32f10x_xx.s; 1.2) SystemCoreClock variable: core clock (HCLK); can be used by the user app to setup the SysTick timer or config other params
// 2. after each device reset the HSI (8 MHz) is used as sys clock source. Then SystemInit() runs, in startup_stm32f10x_xx.s, to config the sys clock before to branch to main app
// 3. if the sys clock source selected by the usr fails to ini, SystemInit() is a no-op and HSI is still used as sys clock source. usr can add some code in SetSysClock() to deal w/ this
#include "stm32f10x.h"

// Uncomment the line corresponding to the desired sys clock (SYSCLK) frequency (after reset the HSI is used as SYSCLK source)
// 1. After each device reset the HSI is used as sys clock source.
// 2. Make sure that the selected sys clock doesn't exceed your device's maximum frequency.
// 3. If none of the define below is enabled, the HSI is used as sys clock source.
// 4. The sys clock configuration functions provided within this file assume that:
// 	- For Low, Medium and High density Value line devices an external 8MHz crystal is used to drive the sys clock.
// 	- For Low, Medium and High density devices an external 8MHz crystal is used to drive the sys clock.
// 	- For Connectivity line devices an external 25MHz crystal is used to drive the sys clock. If you are using different crystal you have to adapt those functions accordingly.
#define VECT_TAB_OFFSET  0x00000000  // Vector Table base offset field.  This value must be a multiple of 0x200
// #define VECT_TAB_SRAM  // uncomment if you need to relocate your vector table in internal SRAM

u32 SystemCoreClock = 72000000;  // System Clock Frequency (Core Clock)

static void clk_set_72mhz(){  // @meta  set system clock freq to 72MHz and configure HCLK, PCLK2 and PCLK1 prescalers. call only after reset. if this doesn't run, the HSI is used as System clock source (default after reset)
	RCC->CR |= (u32)RCC_CR_HSEON;  // enable HSE
	while((RCC->CR & RCC_CR_HSERDY) == 0);

	FLASH->ACR &= ~FLASH_ACR_LATENCY;                     // flash 2 wait state
	FLASH->ACR |=  FLASH_ACR_PRFTBE|FLASH_ACR_LATENCY_2;  // enable prefetch buffer, and something else

	RCC->CFGR  &= ~(RCC_CFGR_PLLSRC|RCC_CFGR_PLLXTPRE|RCC_CFGR_PLLMULL);                                                                        // clear PLL config?
	RCC->CFGR  |= RCC_CFGR_HPRE_DIV1|RCC_CFGR_PPRE2_DIV1|RCC_CFGR_PPRE1_DIV2|RCC_CFGR_PLLSRC_HSE|RCC_CFGR_PLLMULL9|RCC_CFGR_PLLXTPRE_HSE_Div2;  // HCLK = SYSCLK, PCLK2 = HCLK, PCLK1 = HCLK. set PLL config. PLLCLK: HSE * 9 |--> 72 MHz, eg. 16 * 9/2 |--> 72 MHz  // 16MHz: RCC_CFGR_PLLSRC_HSE|RCC_CFGR_PLLMULL9|RCC_CFGR_PLLXTPRE_HSE_Div2, 8MHz: RCC_CFGR_PLLSRC_HSE|RCC_CFGR_PLLMULL9
	RCC->CR    |= RCC_CR_PLLON;                           // enable PLL
	while((RCC->CR & RCC_CR_PLLRDY) == 0);                // wait till PLL is ready

	RCC->CFGR  &= ~RCC_CFGR_SW;                           // select PLL as system clock source
	RCC->CFGR  |=  RCC_CFGR_SW_PLL;
	while((RCC->CFGR & (u32)RCC_CFGR_SWS) != (u32)0x08);  // wait till PLL is used as system clock source
}

void SystemInit(){  // @meta ini MCU, embedded flash interface, and PLL. This function should be used only after reset. Reset the RCC clock configuration to the default reset state(for debug purpose)
	RCC->CR   |= 0x00000001;             // set HSION bit
	RCC->CR   &= 0xfef6ffff&0xfffbffff;  // reset HSEON, CSSON and PLLON bits. reset HSEBYP bit
	RCC->CFGR &= 0xf8ff0000&0xff80ffff;  // reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits. reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits
	RCC->CIR   = 0x009f0000;             // disable all interrupts and clear pending bits
	clk_set_72mhz();                     // config sys clock freq, HCLK, PCLK2 and PCLK1 prescalers. config the Flash Latency cycles and enable prefetch buffer

#if !defined(VECT_TAB_SRAM)
	SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;  // vector table relocation in internal FLASH
#else
	SCB->VTOR = SRAM_BASE  | VECT_TAB_OFFSET;  // vector table relocation in internal SRAM
#endif
}

// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  sleep
#if 1
#define SystemCoreClock  72000000
#define sleep_us(us)  do{  \
	SysTick->LOAD = SystemCoreClock/1000000u - 1;                        /*configure SysTick to count at CPU clock speed: 1µs per tick*/  \
	SysTick->VAL  = 0;                                                   /*clear current value*/  \
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_ENABLE_Msk;  /*enable SysTick, use processor clock*/  \
	uint32_t _us  = us;  \
	while(_us--)  while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));  /*wait for tick*/  \
}while(0)
#define sleep_ms(ms)  do{  \
	SysTick->LOAD = SystemCoreClock/1000u - 1;                            /*configure SysTick to count at CPU clock speed: 1ms per tick*/  \
	SysTick->VAL  = 0;                                                    /*clear current value*/  \
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_ENABLE_Msk;   /*enable SysTick, use processor clock*/  \
	uint32_t _ms  = ms;  \
	while(_ms--)  while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));   /*wait for tick*/  \
}while(0)

// ----------------------------------------------------------------------------------------------------------------------------------------- @blk
void hid_outreport_cb(u8* data, u16 bdim){
	hid_irp_kb_t irp_kb;  // NKRO keyboard

	// for(int i=0; i<sizeof(irp_kb.keys); ++i)  if(bdim>=i+1) irp_kb.keys[i] = data[i];

	// if(bdim>=1)  irp_kb.keys[0] = data[0];
	// if(bdim>=2)  irp_kb.keys[1] = data[1];
	// if(bdim>=3)  irp_kb.keys[2] = data[2];
	// if(bdim>=4)  irp_kb.keys[3] = data[3];
	// usb_datasend(2, (u16*)&irp_kb, sizeof(irp_kb));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll

	irp_kb.keys[0] = 0b01100000;
	usb_datasend(2, (u16*)&irp_kb, sizeof(irp_kb));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll

	memset(&irp_kb,0x00,sizeof(irp_kb));  irp_kb.id=6;  usb_datasend(2, (u16*)&irp_kb, sizeof(irp_kb));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll
}

// ----------------------------------------------------------------------
int main(){  // WORKS! THANK YOU, LORD GOD JESUS!!!!!!!
	usb_ini(hid_ep_handle, hid_reset);
	while(!DeviceConfigured);  // wait for host to configure device

	while(1){  // main app logic
		hid_irp_bkb_t   irp_bkb;  memset(&irp_bkb,0x00,sizeof(irp_bkb));                 // 6KRO boot keyboard
		hid_irp_kb_t    irp_kb;   memset(&irp_kb, 0x00,sizeof(irp_kb));   irp_kb.id =6;  // NKRO keyboard
		hid_irp_msr_t   irp_msr;  memset(&irp_msr,0x00,sizeof(irp_msr));  irp_msr.id=2;  // mouse, relative
		hid_irp_msa16_t irp_msa;  memset(&irp_msa,0x00,sizeof(irp_msa));                 // mouse, absolute

#if 1  // works: boot keyboard. interface 0, endpoint 1 (endpoint indexing in global across all interfaces, not local to each interface)
		irp_bkb.keys[0] = 0x04;                 usb_datasend(1, (u16*)&irp_bkb, sizeof(irp_bkb));  while(_GetEPTxStatus(1)==EP_TX_VALID);  // wait for host to poll
		memset(&irp_bkb,0x00,sizeof(irp_bkb));  usb_datasend(1, (u16*)&irp_bkb, sizeof(irp_bkb));  while(_GetEPTxStatus(1)==EP_TX_VALID);  // wait for host to poll
#endif

#if 1  // works: nkro keyboard. interface 1, endpoint 2 (endpoint indexing in global across all interfaces, not local to each interface)
		irp_kb.keys[0] = 0b00010000;                        usb_datasend(2, (u16*)&irp_kb, sizeof(irp_kb));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll
		memset(&irp_kb,0x00,sizeof(irp_kb));  irp_kb.id=6;  usb_datasend(2, (u16*)&irp_kb, sizeof(irp_kb));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll
#endif

#if 0 // works not: interleaved sends across interfaces. there should be a way to make it work...
		irp_bkb.keys[0] = 0x04;
		irp_kb.keys[0]  = 0b00010000;
		usb_datasend(1, (u16*)&irp_bkb, sizeof(irp_bkb));  
		usb_datasend(2, (u16*)&irp_kb,  sizeof(irp_kb));
		while(_GetEPTxStatus(1)==EP_TX_VALID || _GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll

		memset(&irp_bkb,0x00,sizeof(irp_bkb));
		memset(&irp_kb, 0x00,sizeof(irp_kb));  irp_kb.id=6;
		usb_datasend(1, (u16*)&irp_bkb, sizeof(irp_bkb));
		usb_datasend(2, (u16*)&irp_kb,  sizeof(irp_kb));
		while(_GetEPTxStatus(1)==EP_TX_VALID || _GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll
#endif

#if 0  // works: relative mouse
		irp_msr.x = 0x10;                                      usb_datasend(2, (u16*)&irp_msr, sizeof(irp_msr));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll
		memset(&irp_msr,0x00,sizeof(irp_msr));  irp_msr.id=2;  usb_datasend(2, (u16*)&irp_msr, sizeof(irp_msr));  while(_GetEPTxStatus(2)==EP_TX_VALID);  // wait for host to poll
#endif

#if 1  // works: absolute mouse (with relative mouse on the same USB HID device). WORKS! THANK YOU, LORD GOD JESUS!!!!!!!
		memset(&irp_msa,0x00,sizeof(irp_msa));  usb_datasend(3, (u16*)&irp_msa, sizeof(irp_msa));  while(_GetEPTxStatus(3)==EP_TX_VALID);  // wait for host to poll
		irp_msa.x=30000;  irp_msa.y=1000;       usb_datasend(3, (u16*)&irp_msa, sizeof(irp_msa));  while(_GetEPTxStatus(3)==EP_TX_VALID);  // wait for host to poll
#endif

		sleep_ms(1000);  // for(volatile int i=0; i<1000000; ++i)  asm volatile("nop");  // delay
	}
}
#endif
