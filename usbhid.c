/*
This code is taken from Bruno Freita's USB HID bootloader for STM32F10X, then compacted, simplified, and expanded to multiple interfaces. Diego Cortez, 2026, diego@mathisart.org
*/

/*
* STM32 HID Bootloader - USB HID bootloader for STM32F10X
* Copyright (c) 2018 Bruno Freitas - bruno@brunofreitas.com
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <string.h>
#include "usbhid.h"

#define USB_DEVICE_DESC_TYPE      0x01  // USB Descriptor Types
#define USB_CFG_DESC_TYPE         0x02
#define USB_STR_DESC_TYPE         0x03
#define USB_IFACE_DESC_TYPE       0x04
#define USB_EP_DESC_TYPE          0x05
#define USB_DEVICE_QR_DESC_TYPE   0x06
#define USB_OSPEED_CFG_DESC_TYPE  0x07
#define USB_IFACE_PWR_DESC_TYPE   0x08
#define USB_REPORT_DESC_TYPE      0x22

typedef struct{
	u16  RXB[USB_BUF_BMAX/2];  // rx buf?
	u16* TXB;                  // tx buf?
	u8   RXL;                  // rx len?
	u8   TXL;                  // tx len?
	u8   MaxPacketSize;
}usb_rxtxbuf_t;

typedef struct{
	u8 L:8;
	u8 H:8;
}USB_WByte;

typedef struct{
	u8        bmRequestType;  // 7.1.1 Get_Descriptor Request. For standard USB descriptors, bits 0-4 of bmRequestType indicate whether the requested descriptor is associated with the device, interface, endpoint, or other
	u8        bRequest;
	USB_WByte wValue;
	USB_WByte wIndex;
	u16       wLength;
}USB_SetupPacket;

// -----------------------------------------------------------------------------------------------------------------------------------------
usb_rxtxbuf_t RxTxBuf[USB_EP_IMAX];

volatile u8  DeviceAddress    = 0;
volatile u16 DeviceConfigured = 0;
volatile u16 DeviceStatus     = 0;
void (*_EPHandler)      (u16) = NULL;
void (*_USBResetHandler)()    = NULL;

static USB_SetupPacket* SetupPacket;

// ----------------------------------------------------------------------------------------------------------------------------------------- usb string descriptors
const u8 USB_SD_VENDOR[]  = {0x14/*bdim*/, 0x03/*descriptor type*/, 'm',0x00, 'a',0x00, 't',0x00, 'h',0x00, 'i',0x00, 's',0x00, 'a',0x00, 'r',0x00, 't',0x00};
const u8 USB_SD_PRODUCT[] = {0x12/*bdim*/, 0x03/*descriptor type*/, 'K',0x00, 'e',0x00, 'y',0x00, 'b',0x00, 'o',0x00, 'a',0x00, 'r',0x00, 'd',0x00};
const u8 USB_SD_SERIAL[]  = {0x16/*bdim*/, 0x03/*descriptor type*/, '0',0x00, '1',0x00, '2',0x00, '3',0x00, '4',0x00, '5',0x00, '6',0x00, '7',0x00, '8',0x00, '9',0x00};
const u8 USB_SD_LANGID[]  = {0x04/*bdim*/, 0x03/*descriptor type*/,  0x09, 0x04};


// -----------------------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  usb: low lvl usb protocol fns
static void usb_pma2buf(u8 EPn){  // @meta  this fn COULD be @extern, but we may not need it in the app code? and @static uses less space than @extern
	u8   Count       = RxTxBuf[EPn].RXL = (_GetEPRxCount(EPn) & 0x3ff);
	u32* Address     = (u32*)(PMAAddr + _GetEPRxAddr(EPn) * 2);
	u16* Destination = (u16*)RxTxBuf[EPn].RXB;
	for(u8 i=0; i<Count; ++i){
		*(u16*)Destination = *(u16*) Address;
		++Destination;
		++Address;
	}
}

static void usb_buf2pma(u8 EPn){  // @meta  this fn COULD be @extern, but we may not need it in the app code? and @static uses less space than @extern
	u8   Count       = RxTxBuf[EPn].TXL <= RxTxBuf[EPn].MaxPacketSize ? RxTxBuf[EPn].TXL : RxTxBuf[EPn].MaxPacketSize;  _SetEPTxCount(EPn, Count);
	u32* Destination = (u32*)(PMAAddr + _GetEPTxAddr(EPn) * 2);
	for(u8 i=0; i < (Count+1)/2; ++i){
		*(u32*) Destination = *(u16*) RxTxBuf[EPn].TXB;
		++Destination;
		++RxTxBuf[EPn].TXB;
	}
	RxTxBuf[EPn].TXL -= Count;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
extern void usb_datasend(u8 EPn, u16* data, u16 len){  if(0<EPn && !DeviceConfigured)  return;
	RxTxBuf[EPn].TXL = len;
	RxTxBuf[EPn].TXB = data;
	if(0<len)  usb_buf2pma(  EPn);
	else       _SetEPTxCount(EPn,0);
	_SetEPTxValid(EPn);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
static void usb_shutdown(){  // @meta  this fn COULD be @extern, but we may not need it in the app code? and @static uses less space than @extern
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

	NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);  // disable USB IRQ
	*ISTR = 0x0000;

	DeviceConfigured = DeviceStatus = 0;
	_EPHandler       = NULL;
	_USBResetHandler = NULL;

	*CNTR         = 0x0003;                             // turn USB Macrocell off (FRES + PWDN)
	GPIOA->CRH   |=  GPIO_CRH_CNF12_0|GPIO_CRH_MODE12;  // PA_12 output mode: General purpose output open drain (b01). PA_12 set as: Output mode, max speed 50 MHz.  // Set PA_12 to output
	GPIOA->CRH   &= ~GPIO_CRH_CNF12_1;
	GPIOA->BRR    =  GPIO_BRR_BR12;                     // sink A12 to GND
	RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;                 // disable USB Clock on APB1
}

// -----------------------------------------------------------------------------------------------------------------------------------------
extern void usb_ini(void (*EPHandlerPtr)(u16), void (*ResetHandlerPtr)()){
	for(int i=0; i<USB_EP_IMAX; ++i)  RxTxBuf[i].RXL = RxTxBuf[i].TXL = 0;  // reset RX and TX lengths inside RxTxBuf struct for all endpoints
	_EPHandler       = EPHandlerPtr;
	_USBResetHandler = ResetHandlerPtr;
	DeviceConfigured = DeviceStatus = 0;

	RCC->APB2ENR |=  RCC_APB2ENR_IOPAEN;                   // usb on
	GPIOA->CRH   |=  GPIO_CRH_CNF12_0;                     // usb on: PA_12 output mode: General Purpose Input Float (b01)
	GPIOA->CRH   &= ~GPIO_CRH_CNF12_1 & ~GPIO_CRH_MODE12;  // usb on. set PA_12 to input mode

	RCC->APB1ENR |= RCC_APB1ENR_USBEN;
	NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);  // NOTE! necessary?

	*CNTR = CNTR_FRES;                                    // force USB reset                                // NOTE! not necessary?
	*CNTR = 0x0000;                                       // CNTR_FRES = 0                                  // NOTE! necessary?
	while(!(*ISTR & ISTR_RESET));                         // wait until RESET flag = 1 (polling)            // NOTE! not necessary?
	*ISTR = 0x0000;                                       // clear ISTR register: clear pending interrupts  // NOTE! not necessary?
	*CNTR = CNTR_CTRM|CNTR_RESETM|CNTR_SUSPM|CNTR_WKUPM;  // set interrupt mask                             // NOTE! necessary?
}

// -----------------------------------------------------------------------------------------------------------------------------------------
extern void USB_LP_CAN1_RX0_IRQHandler(){  // @meta  defined/called/used in assembly?
	if(*ISTR & ISTR_RESET){  // handle reset
		*ISTR = *ISTR & CLR_RESET;
		if(_USBResetHandler)  _USBResetHandler();
		return;
	}

	if(*ISTR & ISTR_CTR){  // handle EP data
		if(_EPHandler)  _EPHandler(*ISTR);
		*ISTR = *ISTR & CLR_CTR;
		return;
	}

	if(*ISTR & ISTR_DOVR){  *ISTR = *ISTR & CLR_DOVR;  return;  }  // handle DOVR
	if(*ISTR & ISTR_SUSP){  // handle Suspend
		*ISTR = *ISTR & CLR_SUSP;
		if(*DADDR & 0x007f){
			*DADDR = 0x0000;
			*CNTR  = *CNTR & ~CNTR_SUSPM;
		}  // if device address is assigned, then reset it
		return;
	}

	if(*ISTR & ISTR_ERR) {  *ISTR = *ISTR & CLR_ERR;   return;  }  // handle Error
	if(*ISTR & ISTR_WKUP){  *ISTR = *ISTR & CLR_WKUP;  return;  }  // handle Wakeup
	if(*ISTR & ISTR_SOF) {  *ISTR = *ISTR & CLR_SOF;   return;  }  // handle SOF
	if(*ISTR & ISTR_ESOF){  *ISTR = *ISTR & CLR_ESOF;  return;  }  // handle ESOF
	*ISTR = 0x0000;  // default to clear all interrupt flags
}


// -----------------------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  usb hid: high-lvl usb hid protocol fns
extern void hid_reset(){  // @meta  MAJOR fn?
	*BTABLE = BTABLE_ADDRESS&0xfff8;

	// ----------------------------------------------------------------------
	_SetEPType(    ENDP0, EP_CONTROL);  // ini Endpoint 0: Control Endpoint
	_SetEPRxAddr(  ENDP0, ENDP0_RXADDR);  // OUT endpoint: host2device
	_SetEPTxAddr(  ENDP0, ENDP0_TXADDR);  // IN  endpoint: device2host
	_ClearEP_KIND( ENDP0);
	_SetEPRxCount( ENDP0, EP0_OUT_SIZE);  // this one matters?
	_SetEPRxValid( ENDP0);
	_SetEPAddress( ENDP0, ENDP0);
	RxTxBuf[ENDP0].MaxPacketSize = 0x08;

	// ----------------------------------------------------------------------
	_SetEPType(    ENDP1, EP_INTERRUPT);  // ini Endpoint 1: Interrupt Endpoint
	_SetEPTxAddr(  ENDP1, ENDP1_TXADDR);  // IN endpoint: device2host
	_SetEPTxCount( ENDP1, EP1_IN_SIZE);   // is this correct?
	_SetEPRxStatus(ENDP1, EP_RX_DIS);
	_SetEPTxStatus(ENDP1, EP_TX_NAK);
	_SetEPAddress( ENDP1, ENDP1);
	RxTxBuf[ENDP1].MaxPacketSize = EP1_IN_SIZE;

	// ----------------------------------------------------------------------
	_SetEPType(    ENDP2, EP_INTERRUPT);
	_SetEPTxAddr(  ENDP2, ENDP2_TXADDR);  // IN  endpoint: device2host. make sure the address is aligned and non-overlapping
	_SetEPTxCount( ENDP2, EP2_IN_SIZE);   // is this correct?
	_SetEPRxStatus(ENDP2, EP_RX_DIS);     // EP_RX_DIS:disable out, EP_RX_VALID:enable out?
	_SetEPTxStatus(ENDP2, EP_TX_NAK);
	_SetEPAddress( ENDP2, ENDP2);
	RxTxBuf[ENDP2].MaxPacketSize = EP2_IN_SIZE;  // EP2_OUT_SIZE

	// ----------------------------------------------------------------------
	_SetEPType(    ENDP3, EP_INTERRUPT);
	_SetEPRxAddr(  ENDP3, ENDP3_RXADDR);  // OUT enpdoint: host2device 
	_SetEPTxAddr(  ENDP3, ENDP3_TXADDR);  // IN  endpoint: device2host. make sure the address is aligned and non-overlapping
	_SetEPRxCount( ENDP3, EP3_OUT_SIZE);  // this one matters?
	_SetEPTxCount( ENDP3, EP3_IN_SIZE);   // is this correct?
	_SetEPRxStatus(ENDP3, EP_RX_VALID);   // EP_RX_DIS:disable out, EP_RX_VALID:enable out?
	_SetEPTxStatus(ENDP3, EP_TX_NAK);
	_SetEPAddress( ENDP3, ENDP3);
	RxTxBuf[ENDP3].MaxPacketSize = EP3_OUT_SIZE;  // EP3_OUT_SIZE

	// ----------------------------------------------------------------------
	*DADDR = DADDR_EF;  // set device address and enable function
}

// -----------------------------------------------------------------------------------------------------------------------------------------
static void hid_descriptor_get(USB_SetupPacket* SPacket){
	switch(SPacket->wValue.H){
		case USB_DEVICE_DESC_TYPE:  usb_datasend(0, (u16*)USB_DEVICE_DESC,      SPacket->wLength > sizeof(USB_DEVICE_DESC)      ? sizeof(USB_DEVICE_DESC)      : SPacket->wLength);  break;
		case USB_CFG_DESC_TYPE:     usb_datasend(0, (u16*)USB_DEVICE_CFG_DESC,  SPacket->wLength > sizeof(USB_DEVICE_CFG_DESC)  ? sizeof(USB_DEVICE_CFG_DESC)  : SPacket->wLength);  break;
		case USB_REPORT_DESC_TYPE:{  // IMPORTANT! use this to select interface by interface idx
			if(     SPacket->wIndex.L==0x00)  usb_datasend(0, (u16*)USB_HID_REPORT_DESC0, SPacket->wLength > sizeof(USB_HID_REPORT_DESC0) ? sizeof(USB_HID_REPORT_DESC0) : SPacket->wLength);  // interface 0
			else if(SPacket->wIndex.L==0x01)  usb_datasend(0, (u16*)USB_HID_REPORT_DESC1, SPacket->wLength > sizeof(USB_HID_REPORT_DESC1) ? sizeof(USB_HID_REPORT_DESC1) : SPacket->wLength);  // interface 1
			else if(SPacket->wIndex.L==0x02)  usb_datasend(0, (u16*)USB_HID_REPORT_DESC2, SPacket->wLength > sizeof(USB_HID_REPORT_DESC2) ? sizeof(USB_HID_REPORT_DESC2) : SPacket->wLength);  // interface 2
			else                              usb_datasend(0,0,0);
		}break;
		case USB_STR_DESC_TYPE:
			switch(SPacket->wValue.L){
				case 0x00:  usb_datasend(0, (u16*)USB_SD_LANGID,  SPacket->wLength > sizeof(USB_SD_LANGID)  ? sizeof(USB_SD_LANGID)  : SPacket->wLength);  break;
				case 0x01:  usb_datasend(0, (u16*)USB_SD_VENDOR,  SPacket->wLength > sizeof(USB_SD_VENDOR)  ? sizeof(USB_SD_VENDOR)  : SPacket->wLength);  break;
				case 0x02:  usb_datasend(0, (u16*)USB_SD_PRODUCT, SPacket->wLength > sizeof(USB_SD_PRODUCT) ? sizeof(USB_SD_PRODUCT) : SPacket->wLength);  break;
				default:    usb_datasend(0,0,0);                                                                                                           break;
			}
			break;
		default:  usb_datasend(0,0,0);  break;
	}
}

__attribute__((weak)) void hid_outreport_cb(u8* data, u16 bdim){}  // BUG? is @bdim really the number of BYTES, or the number of 16-bit words in @data?  // __attribute__((optimize("O0")))v
void hid_ep0_outreport_cb(u8* data){}
	
// -----------------------------------------------------------------------------------------------------------------------------------------
extern void hid_ep_handle(u16 status){  // @meta  MAJOR fn?
	u8  EPn = status & USB_ISTR_EP_ID;
	u16 EP  = _GetENDPOINT(EPn);

	// ---------------------------------------------------------------------- data received?
	if(EP & EP_CTR_RX){  // OUT and SETUP report
		usb_pma2buf(EPn);  // cpy from packet area to usr buffer
		if(EPn==0){        // CONTROL Endpoint
			if(EP & USB_EP0R_SETUP){
				SetupPacket = (USB_SetupPacket*)RxTxBuf[EPn].RXB;
				switch(SetupPacket->bRequest){
					case USB_REQUEST_SET_ADDRESS:        DeviceAddress = SetupPacket->wValue.L;  usb_datasend(0,0,0);  break;
					case USB_REQUEST_GET_DESCRIPTOR:     hid_descriptor_get(SetupPacket);                              break;
					case USB_REQUEST_GET_STATUS:         usb_datasend(0, (u16*)&DeviceStatus,     2);                  break;
					case USB_REQUEST_GET_CONFIGURATION:  usb_datasend(0, (u16*)&DeviceConfigured, 1);                  break;
					case USB_REQUEST_SET_CONFIGURATION:  DeviceConfigured=1;                     usb_datasend(0,0,0);  break;
					case USB_REQUEST_GET_INTERFACE:      usb_datasend(0,0,0);                                          break;
					default:                             usb_datasend(0,0,0);  _SetEPTxStatus(0,EP_TX_STALL);          break;
				}
			}else if(RxTxBuf[EPn].RXL)  hid_ep0_outreport_cb((u8*)RxTxBuf[EPn].RXB);  // EP0: OUT report?
		}else  hid_outreport_cb((u8*)RxTxBuf[EPn].RXB, RxTxBuf[EPn].RXL);           // EPn: OUT report, for n>0?
		_ClearEP_CTR_RX(EPn);
		_SetEPRxValid(EPn);
	}

	// ---------------------------------------------------------------------- data transmitted
	if(EP & EP_CTR_TX){
		if(DeviceAddress){
			*DADDR        = DeviceAddress|DADDR_EF;  // set device address and enable function
			DeviceAddress = 0;
		}

		if(RxTxBuf[EPn].TXL)  usb_buf2pma(EPn);  // must transmit data?
		else                  _SetEPTxCount(EPn, 0);
		_SetEPTxValid(EPn);
		_ClearEP_CTR_TX(EPn);
	}
}
