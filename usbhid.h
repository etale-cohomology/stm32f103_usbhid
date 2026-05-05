#include "stm32f10x.h"

#define USB_BUF_BMAX    0x40  // max buffer bdim
#define USB_EP_IMAX     4     // max number of endpoints

#define EP0_OUT_SIZE    0x08
#define EP0_IN_SIZE     0x08
#define EP1_IN_SIZE     0x08
#define EP2_IN_SIZE     0x20
#define EP3_IN_SIZE     0x07
#define EP3_OUT_SIZE    0x40
#define BTABLE_SIZE     (USB_EP_IMAX*0x8)

#define BTABLE_ADDRESS  0x000  // buffer table base address. does it cover 0x000..0x1ff ??

#define ENDP0_RXADDR    (BTABLE_ADDRESS + BTABLE_SIZE)   // EP0 rx buf address. BTABLE starts at 0x000 and reserves 8 bytes per endpoint (4 for TX descriptor, 4 for RX descriptor)?
#define ENDP0_TXADDR    (ENDP0_RXADDR + 2*EP0_OUT_SIZE)  // EP0 rx buf address.

#define ENDP1_TXADDR    (ENDP0_TXADDR + 2*EP0_IN_SIZE)   // EP1 tx buf address. address must leave enough room for the previous endpoint's report descriptor
#define ENDP2_TXADDR    (ENDP1_TXADDR + 2*EP1_IN_SIZE)   // EP2 tx buf address. address must leave enough room for the previous endpoint's report descriptor

#define ENDP3_TXADDR    (ENDP2_TXADDR + 2*EP2_IN_SIZE)
#define ENDP3_RXADDR    (ENDP3_TXADDR + 2*EP3_IN_SIZE)

// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  libusbK. LGPL
#define USB_ENDPOINT_TYPE_MASK         0x03  // Endpoint type mask for the bmAttributes field of a USB_ENDPOINT_DESCRIPTOR
#define USB_ENDPOINT_TYPE_CONTROL      0x00  // Indicates a control endpoint
#define USB_ENDPOINT_TYPE_ISOCHRONOUS  0x01  // Indicates an isochronous endpoint
#define USB_ENDPOINT_TYPE_BULK         0x02  // Indicates a bulk endpoint
#define USB_ENDPOINT_TYPE_INTERRUPT    0x03  // Indicates an interrupt endpoint
#define USB_CONFIG_POWERED_MASK        0xc0  // Config power mask for the bmAttributes field of a USB_CONFIGURATION_DESCRIPTOR
#define USB_ENDPOINT_DIRECTION_MASK    0x80  // Endpoint direction mask for the bEndpointAddress field of a USB_ENDPOINT_DESCRIPTOR
#define USB_ENDPOINT_ADDRESS_MASK      0x0f  // Endpoint address mask for the bEndpointAddress field of a USB_ENDPOINT_DESCRIPTOR
 
enum USB_DESCRIPTOR_TYPE{  // Standard USB descriptor types
	USB_DESCRIPTOR_TYPE_DEVICE                = 0x01,  // Device descriptor type
	USB_DESCRIPTOR_TYPE_CONFIGURATION         = 0x02,  // Configuration descriptor type
	USB_DESCRIPTOR_TYPE_STRING                = 0x03,  // String descriptor type
	USB_DESCRIPTOR_TYPE_INTERFACE             = 0x04,  // Interface descriptor type
	USB_DESCRIPTOR_TYPE_ENDPOINT              = 0x05,  // Endpoint descriptor type
	USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER      = 0x06,  // Device qualifier descriptor type
	USB_DESCRIPTOR_TYPE_CONFIG_POWER          = 0x07,  // Config power descriptor type
	USB_DESCRIPTOR_TYPE_INTERFACE_POWER       = 0x08,  // Interface power descriptor type
	USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION = 0x0b,  // Interface association descriptor type
	USB_DESCRIPTOR_TYPE_BOS                   = 0x0f,  // BOS descriptor type
	USB_DESCRIPTOR_TYPE_DEVICE_CAPS           = 0x10,  // Device capabilities descriptor type
	USB_SUPERSPEED_ENDPOINT_COMPANION         = 0x30,  // Superspeed endpoint companion descriptor type
};

enum USB_REQUEST_ENUM{  // USB defined request codes
	USB_REQUEST_GET_STATUS        = 0x00,  // Request status of the specific recipient
	USB_REQUEST_CLEAR_FEATURE     = 0x01,  // Clear or disable a specific feature
	USB_REQUEST_SET_FEATURE       = 0x03,  // Set or enable a specific feature
	USB_REQUEST_SET_ADDRESS       = 0x05,  // Set device address for all future accesses
	USB_REQUEST_GET_DESCRIPTOR    = 0x06,  // Get the specified descriptor
	USB_REQUEST_SET_DESCRIPTOR    = 0x07,  // Update existing descriptors or add new descriptors
	USB_REQUEST_GET_CONFIGURATION = 0x08,  // Get the current device configuration value
	USB_REQUEST_SET_CONFIGURATION = 0x09,  // Set device configuration
	USB_REQUEST_GET_INTERFACE     = 0x0a,  // Return the selected alternate setting for the specified interface
	USB_REQUEST_SET_INTERFACE     = 0x0b,  // Select an alternate interface for the specified interface
	USB_REQUEST_SYNC_FRAME        = 0x0c,  // Set then report an endpoint's synchronization frame
};

enum USB_DEVICE_CLASS_ENUM{  // USB defined class codes
	USB_DEVICE_CLASS_RESERVED        = 0x00,
	USB_DEVICE_CLASS_AUDIO           = 0x01,
	USB_DEVICE_CLASS_COMMUNICATIONS  = 0x02,
	USB_DEVICE_CLASS_HUMAN_INTERFACE = 0x03,
	USB_DEVICE_CLASS_IMAGING         = 0x06,
	USB_DEVICE_CLASS_PRINTER         = 0x07,
	USB_DEVICE_CLASS_STORAGE         = 0x08,
	USB_DEVICE_CLASS_HUB             = 0x09,
	USB_DEVICE_CLASS_VENDOR_SPECIFIC = 0xff,
};

// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  usb regs. (C) COPYRIGHT 2008 STMicroelectronics
typedef enum _EP_DBUF_DIR{
	EP_DBUF_ERR,  // double buffered endpoint direction
	EP_DBUF_OUT,
	EP_DBUF_IN
}EP_DBUF_DIR;

enum EP_BUF_NUM{  // endpoint buffer number
	EP_NOBUF,
	EP_BUF0,
	EP_BUF1
};

#define RegBase         0x40005c00ul  // USB_IP Peripheral Registers base address
#define PMAAddr         0x40006000ul  // USB_IP Packet Memory Area base address

// usb general registers
#define CNTR            ((volatile unsigned*)(RegBase + 0x40))  // Control register
#define ISTR            ((volatile unsigned*)(RegBase + 0x44))  // Interrupt status register
#define FNR             ((volatile unsigned*)(RegBase + 0x48))  // Frame number register
#define DADDR           ((volatile unsigned*)(RegBase + 0x4c))  // Device address register
#define BTABLE          ((volatile unsigned*)(RegBase + 0x50))  // Buffer Table address register

// endpoint registers
#define EP0REG          ((volatile unsigned *)(RegBase)) // endpoint 0 register address

// out endpoint addresses
#define EP0_OUT         ((u8)0x00)
#define EP1_OUT         ((u8)0x01)
#define EP2_OUT         ((u8)0x02)
#define EP3_OUT         ((u8)0x03)
#define EP4_OUT         ((u8)0x04)
#define EP5_OUT         ((u8)0x05)
#define EP6_OUT         ((u8)0x06)
#define EP7_OUT         ((u8)0x07)

// in endpoint addresses
#define EP0_IN          ((u8)0x80)
#define EP1_IN          ((u8)0x81)
#define EP2_IN          ((u8)0x82)
#define EP3_IN          ((u8)0x83)
#define EP4_IN          ((u8)0x84)
#define EP5_IN          ((u8)0x85)
#define EP6_IN          ((u8)0x86)
#define EP7_IN          ((u8)0x87)

// endpoints enumeration
#define ENDP0           ((u8)0)
#define ENDP1           ((u8)1)
#define ENDP2           ((u8)2)
#define ENDP3           ((u8)3)
#define ENDP4           ((u8)4)
#define ENDP5           ((u8)5)
#define ENDP6           ((u8)6)
#define ENDP7           ((u8)7)

// ISTR interrupt events
#define ISTR_CTR        0x8000  // Correct TRansfer (clear-only bit)
#define ISTR_DOVR       0x4000  // DMA OVeR/underrun (clear-only bit)
#define ISTR_ERR        0x2000  // ERRor (clear-only bit)
#define ISTR_WKUP       0x1000  // WaKe UP (clear-only bit)
#define ISTR_SUSP       0x0800  // SUSPend (clear-only bit)
#define ISTR_RESET      0x0400  // RESET (clear-only bit)
#define ISTR_SOF        0x0200  // Start Of Frame (clear-only bit)
#define ISTR_ESOF       0x0100  // Expected Start Of Frame (clear-only bit)
#define ISTR_DIR        0x0010  // DIRection of transaction (read-only bit)
#define ISTR_EP_ID      0x000f  // EndPoint IDentifier (read-only bit)

#define CLR_CTR         (~ISTR_CTR)    // clear Correct TRansfer bit
#define CLR_DOVR        (~ISTR_DOVR)   // clear DMA OVeR/underrun bit
#define CLR_ERR         (~ISTR_ERR)    // clear ERRor bit
#define CLR_WKUP        (~ISTR_WKUP)   // clear WaKe UP bit
#define CLR_SUSP        (~ISTR_SUSP)   // clear SUSPend bit
#define CLR_RESET       (~ISTR_RESET)  // clear RESET bit
#define CLR_SOF         (~ISTR_SOF)    // clear Start Of Frame bit
#define CLR_ESOF        (~ISTR_ESOF)   // clear Expected Start Of Frame bit

// CNTR control register bits definitions
#define CNTR_CTRM       0x8000  // Correct TRansfer Mask
#define CNTR_DOVRM      0x4000  // DMA OVeR/underrun Mask
#define CNTR_ERRM       0x2000  // ERRor Mask
#define CNTR_WKUPM      0x1000  // WaKe UP Mask
#define CNTR_SUSPM      0x0800  // SUSPend Mask
#define CNTR_RESETM     0x0400  // RESET Mask
#define CNTR_SOFM       0x0200  // Start Of Frame Mask
#define CNTR_ESOFM      0x0100  // Expected Start Of Frame Mask
#define CNTR_RESUME     0x0010  // RESUME request
#define CNTR_FSUSP      0x0008  // Force SUSPend
#define CNTR_LPMODE     0x0004  // Low-power MODE
#define CNTR_PDWN       0x0002  // Power DoWN
#define CNTR_FRES       0x0001  // Force USB RESet

// FNR Frame Number Register bit definitions
#define FNR_RXDP        0x8000  // status of D+ data line
#define FNR_RXDM        0x4000  // status of D- data line
#define FNR_LCK         0x2000  // LoCKed
#define FNR_LSOF        0x1800  // Lost SOF
#define FNR_FN          0x07ff  // Frame Number

// DADDR Device ADDRess bit definitions
#define DADDR_EF        0x80
#define DADDR_ADD       0x7f

// Endpoint register. bit positions
#define EP_CTR_RX       0x8000  // EndPoint Correct TRansfer RX
#define EP_DTOG_RX      0x4000  // EndPoint Data TOGGLE RX
#define EPRX_STAT       0x3000  // EndPoint RX STATus bit field
#define EP_SETUP        0x0800  // EndPoint SETUP
#define EP_T_FIELD      0x0600  // EndPoint TYPE
#define EP_KIND         0x0100  // EndPoint KIND
#define EP_CTR_TX       0x0080  // EndPoint Correct TRansfer TX
#define EP_DTOG_TX      0x0040  // EndPoint Data TOGGLE TX
#define EPTX_STAT       0x0030  // EndPoint TX STATus bit field
#define EPADDR_FIELD    0x000f  // EndPoint ADDRess FIELD

// EndPoint REGister MASK (no toggle fields)
#define EPREG_MASK      (EP_CTR_RX|EP_SETUP|EP_T_FIELD|EP_KIND|EP_CTR_TX|EPADDR_FIELD)

// EP_TYPE[1:0] EndPoint TYPE
#define EP_TYPE_MASK    0x0600  // EndPoint TYPE Mask
#define EP_BULK         0x0000  // EndPoint BULK
#define EP_CONTROL      0x0200  // EndPoint CONTROL
#define EP_ISOCHRONOUS  0x0400  // EndPoint ISOCHRONOUS
#define EP_INTERRUPT    0x0600  // EndPoint INTERRUPT
#define EP_T_MASK       (~EP_T_FIELD & EPREG_MASK)

// EP_KIND EndPoint KIND
#define EPKIND_MASK    (~EP_KIND & EPREG_MASK)

// STAT_TX[1:0] STATus for TX transfer
#define EP_TX_DIS       0x0000  // EndPoint TX DISabled
#define EP_TX_STALL     0x0010  // EndPoint TX STALLed
#define EP_TX_NAK       0x0020  // EndPoint TX NAKed
#define EP_TX_VALID     0x0030  // EndPoint TX VALID
#define EPTX_DTOG1      0x0010  // EndPoint TX Data TOGgle bit1
#define EPTX_DTOG2      0x0020  // EndPoint TX Data TOGgle bit2
#define EPTX_DTOGMASK   (EPTX_STAT|EPREG_MASK)

// STAT_RX[1:0] STATus for RX transfer
#define EP_RX_DIS       0x0000  // EndPoint RX DISabled
#define EP_RX_STALL     0x1000  // EndPoint RX STALLed
#define EP_RX_NAK       0x2000  // EndPoint RX NAKed
#define EP_RX_VALID     0x3000  // EndPoint RX VALID
#define EPRX_DTOG1      0x1000  // EndPoint RX Data TOGgle bit1
#define EPRX_DTOG2      0x2000  // EndPoint RX Data TOGgle bit1
#define EPRX_DTOGMASK   (EPRX_STAT|EPREG_MASK)

#define _SetCNTR(  wRegValue)           (*CNTR   = (u16)wRegValue)             // SetCNTR
#define _SetISTR(  wRegValue)           (*ISTR   = (u16)wRegValue)             // SetISTR
#define _SetDADDR( wRegValue)           (*DADDR  = (u16)wRegValue)             // SetDADDR
#define _SetBTABLE(wRegValue)           (*BTABLE = (u16)(wRegValue & 0xfff8))  // SetBTABLE
#define _GetCNTR()                      ((u16) *CNTR)                          // GetCNTR
#define _GetISTR()                      ((u16) *ISTR)                          // GetISTR
#define _GetFNR()                       ((u16) *FNR)                           // GetFNR
#define _GetDADDR()                     ((u16) *DADDR)                         // GetDADDR
#define _GetBTABLE()                    ((u16) *BTABLE)                        // GetBTABLE

#define _SetENDPOINT(bEpNum,wRegValue)  (*(EP0REG + bEpNum)= (u16)wRegValue)   // SetENDPOINT
#define _GetENDPOINT(bEpNum)            ((u16)(*(EP0REG + bEpNum)))            // GetENDPOINT

#define _SetEPType(bEpNum,wType)        (_SetENDPOINT(bEpNum, ((_GetENDPOINT(bEpNum) & EP_T_MASK) | wType)))   // SetEPType                                                  sets the type in the endpoint register(bits EP_TYPE[1:0])            Input: @bEpNum: Endpoint Number. @wType
#define _GetEPType(bEpNum)              (_GetENDPOINT(bEpNum) & EP_T_FIELD)                                    // GetEPType                                                  gets the type in the endpoint register(bits EP_TYPE[1:0])            Input: @bEpNum: Endpoint Number

#define _SetEPTxStatus(bEpNum,wState){                                                                         /* SetEPTxStatus                                              sets the status for tx transfer (bits STAT_TX[1:0]).                 Input: @bEpNum: Endpoint Number. @wState: new state */ \
	register u16 _wRegVal;       \
	_wRegVal = _GetENDPOINT(bEpNum) & EPTX_DTOGMASK;\
	if((EPTX_DTOG1 & wState)!= 0)  _wRegVal ^= EPTX_DTOG1;  /* toggle first bit ? */   \
	if((EPTX_DTOG2 & wState)!= 0)  _wRegVal ^= EPTX_DTOG2;  /* toggle second bit ? */  \
	_SetENDPOINT(bEpNum, _wRegVal);    \
}

#define _SetEPRxStatus(bEpNum,wState){                                                                         /* SetEPRxStatus                                              sets the status for rx transfer (bits STAT_TX[1:0])                  Input: @bEpNum: Endpoint Number. @wState: new state. */ \
	register u16 _wRegVal;   \
	_wRegVal = _GetENDPOINT(bEpNum) & EPRX_DTOGMASK;\
	if((EPRX_DTOG1 & wState)!= 0)  _wRegVal ^= EPRX_DTOG1;  /* toggle first bit ? */   \
	if((EPRX_DTOG2 & wState)!= 0)  _wRegVal ^= EPRX_DTOG2;  /* toggle second bit ? */  \
	_SetENDPOINT(bEpNum, _wRegVal); \
}

#define _SetEPRxTxStatus(bEpNum,wStaterx,wStatetx){                                                            /* @SetEPRxTxStatus.                                          sets the status for rx & tx (bits STAT_TX[1:0] & STAT_RX[1:0]).      Input: @bEpNum: Endpoint Number. @wStaterx: new state. @wStatetx: new state. */  \
	register u32 _wRegVal;  \
	_wRegVal = _GetENDPOINT(bEpNum) & (EPRX_DTOGMASK |EPTX_STAT);  \
	if((EPRX_DTOG1 & wStaterx)!= 0) _wRegVal ^= EPRX_DTOG1;  /*toggle 1st bit?*/  \
	if((EPRX_DTOG2 & wStaterx)!= 0) _wRegVal ^= EPRX_DTOG2;  /*toggle 2nd bit?*/  \
	if((EPTX_DTOG1 & wStatetx)!= 0) _wRegVal ^= EPTX_DTOG1;  /*toggle 1st bit?*/  \
	if((EPTX_DTOG2 & wStatetx)!= 0) _wRegVal ^= EPTX_DTOG2;  /*toggle 2nd bit?*/  \
	_SetENDPOINT(bEpNum, _wRegVal|EP_CTR_RX|EP_CTR_TX);  \
}

#define _GetEPTxStatus(bEpNum)       ((u16)_GetENDPOINT(bEpNum) & EPTX_STAT)                                   // GetEPTxStatus / GetEPRxStatus                              gets the status for tx/rx transfer (bits STAT_TX[1:0] STAT_RX[1:0])  Input: @bEpNum: Endpoint Number
#define _GetEPRxStatus(bEpNum)       ((u16)_GetENDPOINT(bEpNum) & EPRX_STAT)

#define _SetEPTxValid(bEpNum)        (_SetEPTxStatus(bEpNum, EP_TX_VALID))                                     // SetEPTxValid / SetEPRxValid                                sets directly the VALID tx/rx-status into the enpoint register       Input: @bEpNum: Endpoint Number
#define _SetEPRxValid(bEpNum)        (_SetEPRxStatus(bEpNum, EP_RX_VALID))

#define _GetTxStallStatus(bEpNum)    (_GetEPTxStatus(bEpNum) == EP_TX_STALL)                                   // GetTxStallStatus / GetRxStallStatus.                       checks stall condition in an endpoint.                               Input: @bEpNum: Endpoint Number.  // Return: TRUE = endpoint in stall condition
#define _GetRxStallStatus(bEpNum)    (_GetEPRxStatus(bEpNum) == EP_RX_STALL)

#define _SetEP_KIND(bEpNum)          (_SetENDPOINT(bEpNum, (_GetENDPOINT(bEpNum) | EP_KIND) & EPREG_MASK))     // SetEP_KIND / ClearEP_KIND.                                 set & clear EP_KIND bit.                                             Input: @bEpNum: Endpoint Number
#define _ClearEP_KIND(bEpNum)        (_SetENDPOINT(bEpNum, (_GetENDPOINT(bEpNum) & EPKIND_MASK)))

#define _Set_Status_Out(bEpNum)      _SetEP_KIND(bEpNum)                                                       // Set_Status_Out / Clear_Status_Out.                         sets/clears directly STATUS_OUT bit in the endpoint register.        Input: @bEpNum: Endpoint Number
#define _Clear_Status_Out(bEpNum)    _ClearEP_KIND(bEpNum)

#define _SetEPDoubleBuff(bEpNum)     _SetEP_KIND(bEpNum)                                                       // SetEPDoubleBuff / ClearEPDoubleBuff.                       sets/clears directly EP_KIND bit in the endpoint register.           Input: @bEpNum: Endpoint Number
#define _ClearEPDoubleBuff(bEpNum)   _ClearEP_KIND(bEpNum)

#define _ClearEP_CTR_RX(bEpNum)      (_SetENDPOINT(bEpNum, _GetENDPOINT(bEpNum) & 0x7FFF & EPREG_MASK))        // ClearEP_CTR_RX / ClearEP_CTR_TX.                           clears bit CTR_RX / CTR_TX in the endpoint register.                 Input: @bEpNum: Endpoint Number
#define _ClearEP_CTR_TX(bEpNum)      (_SetENDPOINT(bEpNum, _GetENDPOINT(bEpNum) & 0xFF7F & EPREG_MASK))

#define _ToggleDTOG_RX(bEpNum)       (_SetENDPOINT(bEpNum, EP_DTOG_RX | (_GetENDPOINT(bEpNum) & EPREG_MASK)))  // ToggleDTOG_RX / ToggleDTOG_TX .                            toggles DTOG_RX / DTOG_TX bit in the endpoint register.              Input: @bEpNum: Endpoint Number
#define _ToggleDTOG_TX(bEpNum)       (_SetENDPOINT(bEpNum, EP_DTOG_TX | (_GetENDPOINT(bEpNum) & EPREG_MASK)))

#define _ClearDTOG_RX(bEpNum)        if((_GetENDPOINT(bEpNum) & EP_DTOG_RX) != 0) _ToggleDTOG_RX(bEpNum)       // ClearDTOG_RX / ClearDTOG_TX.                               clears DTOG_RX / DTOG_TX bit in the endpoint register.               Input: @bEpNum: Endpoint Number
#define _ClearDTOG_TX(bEpNum)        if((_GetENDPOINT(bEpNum) & EP_DTOG_TX) != 0) _ToggleDTOG_TX(bEpNum)

#define _SetEPAddress(bEpNum,bAddr)  _SetENDPOINT(bEpNum, (_GetENDPOINT(bEpNum) & EPREG_MASK) | bAddr)         // SetEPAddress.                                              sets address in an endpoint register.                                Input: @bEpNum: Endpoint Number @bAddr: Address.
#define _GetEPAddress(bEpNum)        ((u8)(_GetENDPOINT(bEpNum) & EPADDR_FIELD))                               // GetEPAddress.                                              gets address in an endpoint register.                                Input: @bEpNum: Endpoint Number.

#define _pEPTxAddr( bEpNum)          ((u32 *)((_GetBTABLE()+bEpNum*8+0)*2 + PMAAddr))
#define _pEPTxCount(bEpNum)          ((u32 *)((_GetBTABLE()+bEpNum*8+2)*2 + PMAAddr))
#define _pEPRxAddr( bEpNum)          ((u32 *)((_GetBTABLE()+bEpNum*8+4)*2 + PMAAddr))
#define _pEPRxCount(bEpNum)          ((u32 *)((_GetBTABLE()+bEpNum*8+6)*2 + PMAAddr))

#define _SetEPTxAddr(bEpNum,wAddr)   (*_pEPTxAddr(bEpNum) = ((wAddr >> 1) << 1))                               // SetEPTxAddr / SetEPRxAddr.                                 sets address of the tx/rx buffer.                                    Input: @bEpNum: Endpoint Number. @wAddr: address to be set (must be word aligned).
#define _SetEPRxAddr(bEpNum,wAddr)   (*_pEPRxAddr(bEpNum) = ((wAddr >> 1) << 1))

#define _GetEPTxAddr(bEpNum)         ((u16)*_pEPTxAddr(bEpNum))                                                // GetEPTxAddr / GetEPRxAddr.                                 gets address of the tx/rx buffer.                                    Input: bEpNum: Endpoint Number.  // Return: address of the buffer.
#define _GetEPRxAddr(bEpNum)         ((u16)*_pEPRxAddr(bEpNum))

#define _BlocksOf32(dwReg,wCount,wNBlocks){                                                                    /* SetEPCountRxReg.                                           sets counter of rx buffer with no. of blocks.                        Input: @pdwReg: pointer to counter. @wCount: Counter. */ \
	wNBlocks = wCount >> 5;\
	if((wCount & 0x1f) == 0) wNBlocks--;\
	*pdwReg = (u32)((wNBlocks << 10) | 0x8000);\
}/* _BlocksOf32 */
#define _BlocksOf2(dwReg,wCount,wNBlocks){\
	wNBlocks = wCount >> 1;\
	if((wCount & 0x1) != 0) wNBlocks++;\
	*pdwReg = (u32)(wNBlocks << 10);\
}/* _BlocksOf2 */

#define _SetEPCountRxReg(dwReg,wCount)  {                                                                      /* SetEPCountRxReg.                                           set counter of rx buffer with no. of blocks.                         Input: @pdwReg: pointer to counter. @wCount: counter. */  \
	u16 wNBlocks;\
	if(wCount > 62){_BlocksOf32(dwReg,wCount,wNBlocks);}\
	else {_BlocksOf2(dwReg,wCount,wNBlocks);}\
}

#define _SetEPRxDblBuf0Count(bEpNum,wCount){\
	u32 *pdwReg = _pEPTxCount(bEpNum); \
	_SetEPCountRxReg(pdwReg, wCount);\
}

#define _SetEPTxCount(bEpNum,wCount) (*_pEPTxCount(bEpNum) = wCount)                                           // SetEPTxCount / SetEPRxCount.                               sets counter for the tx/rx buffer.                                   Input: @bEpNum: endpoint number. @wCount: Counter value.
#define _SetEPRxCount(bEpNum,wCount){\
	u32 *pdwReg = _pEPRxCount(bEpNum); \
	_SetEPCountRxReg(pdwReg, wCount);\
}

#define _GetEPTxCount(bEpNum)((u16)(*_pEPTxCount(bEpNum)) & 0x3ff)                                             // GetEPTxCount / GetEPRxCount.                               gets counter of the tx/rx buffer.                                    Input: bEpNum: endpoint number.
#define _GetEPRxCount(bEpNum)((u16)(*_pEPRxCount(bEpNum)) & 0x3ff)

#define _SetEPDblBuf0Addr(bEpNum,wBuf0Addr){_SetEPTxAddr(bEpNum, wBuf0Addr);}                                  // SetEPDblBuf0Addr / SetEPDblBuf1Addr.                       sets buffer 0/1 address in a double buffer endpoint.                 Input: @bEpNum: endpoint number. @wBuf0Addr: buffer 0 address.
#define _SetEPDblBuf1Addr(bEpNum,wBuf1Addr){_SetEPRxAddr(bEpNum, wBuf1Addr);}

#define _SetEPDblBuffAddr(bEpNum,wBuf0Addr,wBuf1Addr){                                                         /* SetEPDblBuffAddr.                                          sets addresses in a double buffer endpoint.                          Input: @bEpNum: endpoint number. @wBuf0Addr: buffer 0 address. @wBuf1Addr = buffer 1 address. */ \
	_SetEPDblBuf0Addr(bEpNum, wBuf0Addr);\
	_SetEPDblBuf1Addr(bEpNum, wBuf1Addr);\
}

#define _GetEPDblBuf0Addr(bEpNum) (_GetEPTxAddr(bEpNum))                                                       // GetEPDblBuf0Addr / GetEPDblBuf1Addr.                       gets buffer 0/1 address of a double buffer endpoint.                 Input: @bEpNum: endpoint number.
#define _GetEPDblBuf1Addr(bEpNum) (_GetEPRxAddr(bEpNum))

#define _SetEPDblBuf0Count(bEpNum, bDir, wCount)  {                                                            /* SetEPDblBuffCount / SetEPDblBuf0Count / SetEPDblBuf1Count. gets buffer 0/1 address of a double buffer endpoint.                 Input: @bEpNum: endpoint number. @bDir: endpoint dir  EP_DBUF_OUT = OUT. EP_DBUF_IN  = IN @wCount: Counter value */ \
	if(     bDir == EP_DBUF_OUT){   _SetEPRxDblBuf0Count(bEpNum,wCount);  }  /* OUT endpoint */  \
	else if(bDir == EP_DBUF_IN)    *_pEPTxCount(bEpNum) = (u32)wCount;       /* IN  endpoint */  \
}

#define _SetEPDblBuf1Count(bEpNum, bDir, wCount)  {\
	if(     bDir == EP_DBUF_OUT){  _SetEPRxCount(bEpNum,wCount);}       /* OUT endpoint */  \
	else if(bDir == EP_DBUF_IN)    *_pEPRxCount(bEpNum) = (u32)wCount;  /* IN  endpoint */  \
}

#define _SetEPDblBuffCount(bEpNum, bDir, wCount){\
	_SetEPDblBuf0Count(bEpNum, bDir, wCount); \
	_SetEPDblBuf1Count(bEpNum, bDir, wCount); \
}

#define _GetEPDblBuf0Count(bEpNum)  (_GetEPTxCount(bEpNum))                                                    // GetEPDblBuf0Count / GetEPDblBuf1Count.                     gets buffer 0/1 rx/tx counter for double buffering.                  Input: @bEpNum: endpoint number.
#define _GetEPDblBuf1Count(bEpNum)  (_GetEPRxCount(bEpNum))

// ----------------------------------------------------------------------------------------------------------------------------------------- @blk  usb descriptors
#define U16LO(x16)  ((x16)>>0 & 0xff)
#define U16HI(x16)  ((x16)>>8 & 0xff)

#define USB_DEVICE_DESC_BDIM  0x12
static const u8 USB_DEVICE_DESC[USB_DEVICE_DESC_BDIM] = {  // USB device descriptor
	USB_DEVICE_DESC_BDIM,  // Size of this descriptor (in bytes).
	0x01,                  // Descriptor type.
	0x00,0x02,             // USB 2.0. Use `0x10,0x01` for USB 1.10. USB specification release number in binary-coded decimal.
	0x00,                  // USB-IF class code for the device.
	0x00,                  // USB-IF subclass code for the device.
	0x00,                  // USB-IF protocol code for the device.
	0x08,                  // 8. Maximum packet size for control endpoint 0.
	0x09,0x12,             // 0x1209 (generic). USB-IF vendor ID.
	0x00,0x00,             // 0x0000 (L-sheaf bootloader 0x00, start at 0x3080). USB-IF product ID.
	0x01,0x00,             // 0.01. Device release number in binary-coded decimal.
	0x01,                  // Index of string descriptor describing manufacturer.
	0x02,                  // Index of string descriptor describing product.
	0x00,                  // Index of string descriptor containing device serial number.
	0x01,                  // Number of possible configurations.
};

#define USB_HID_REPORT_DESC0_BDIM  64
static const uint8_t USB_HID_REPORT_DESC0[USB_HID_REPORT_DESC0_BDIM] = {  // "The HID specification states that a host which can only understand the boot protocol must explicitly request the keyboard switch to it by requesting it using a special command."
	0x05, 0x01,        // Usage Page (Generic Desktop)
	0x09, 0x06,        // Usage (Keyboard)
	0xa1, 0x01,        // Collection (Application)
		// modifier mask (1 byte)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x08,        //   Report Count (8)
		0x05, 0x07,        //   Usage Page (Key Codes)
		0x19, 0xe0,        //   Usage Minimum (224)
		0x29, 0xe7,        //   Usage Maximum (231)
		0x15, 0x00,        //   Logical Minimum (0)
		0x25, 0x01,        //   Logical Maximum (1)
		0x81, 0x02,        //   Input (Data, Variable, Absolute)
		// padding (1 byte)
		0x95, 0x01,        //   Report Count (1)
		0x75, 0x08,        //   Report Size (8)
		0x81, 0x01,        //   Input (Constant)
		// 6-key array (6 bytes)
		0x95, 0x06,        //   Report Count (6)
		0x75, 0x08,        //   Report Size (8)
		0x15, 0x00,        //   Logical Minimum (0)
		0x26, 0xff, 0x00,  //   Logical Maximum (255)
		0x05, 0x07,        //   Usage Page (Key Codes)
		0x19, 0x00,        //   Usage Minimum (0)
		0x29, 0xff,        //   Usage Maximum (255)
		0x81, 0x00,        //   Input (Data, Array)
		// LED (note that this is OUTPUT, not INPUT)
		0x95, 0x05,        //   Report Count (5)
		0x75, 0x01,        //   Report Size (1)
		0x05, 0x08,        //   Usage Page (LEDs)
		0x19, 0x01,        //   Usage Minimum (1)
		0x29, 0x05,        //   Usage Maximum (5)
		0x91, 0x02,        //   Output (Data, Variable, Absolute)
		// LED padding (note that this is OUTPUT, not INPUT)
		0x95, 0x01,        //   Report Count (1)
		0x75, 0x03,        //   Report Size (3)
		0x91, 0x01,        //   Output (Constant)
	0xc0,              // End Collection
};

#define USB_HID_REPORT_DESC1_BDIM  182  // 195
static const u8 USB_HID_REPORT_DESC1[USB_HID_REPORT_DESC1_BDIM] = {  // hid report: mouse + consumer/app keys + 240KRO
	// ---------------------------------------------------------------------- mouse report
	0x05,0x01,           // Usage Page:          Generic Desktop Ctrls
	0x09,0x02,           // Usage:               Mouse
	0xa1,0x01,           // Collection:          Application
		0x85,0x02,         //   REPORT ID:         2
		0x09,0x01,         //   Usage:             Pointer
		0xa1,0x00,         //   Collection:        Physical
			// ------------------------
			0x05,0x09,       //     Usage Page:      Button
			0x19,0x01,       //     Usage Minimum:   0x01
			0x29,0x08,       //     Usage Maximum:   0x08
			0x15,0x00,       //     Logical Minimum: 0
			0x25,0x01,       //     Logical Maximum: 1
			0x95,0x08,       //     Report Count:    8
			0x75,0x01,       //     Report Size:     1
			0x81,0x02,       //     Input:           Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
			// ------------------------
			0x05,0x01,       //     Usage Page:      Generic Desktop Ctrls
			0x09,0x30,       //     Usage:           X
			0x09,0x31,       //     Usage:           Y
			0x15,0x81,       //     Logical Minimum: -127
			0x25,0x7f,       //     Logical Maximum: 127
			0x95,0x02,       //     Report Count:    2
			0x75,0x08,       //     Report Size:     8
			0x81,0x06,       //     Input:           Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position

			// ------------------------
			0x09,0x38,       //     Usage:           Wheel
			0x15,0x81,       //     Logical Minimum: -127
			0x25,0x7f,       //     Logical Maximum: 127
			0x95,0x01,       //     Report Count:    1
			0x75,0x08,       //     Report Size:     8
			0x81,0x06,       //     Input:           Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position
			// ------------------------
			0x05,0x0c,       //     Usage Page:      Consumer
			0x0a,0x38,0x02,  //     Usage:           AC Pan
			0x15,0x81,       //     Logical Minimum: -127
			0x25,0x7f,       //     Logical Maximum: 127
			0x95,0x01,       //     Report Count:    1
			0x75,0x08,       //     Report Size:     8
			0x81,0x06,       //     Input:           Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position
		0xc0,              //   End Collection
	0xc0,                // End Collection

	// ---------------------------------------------------------------------- system ctl report, 1KRO
	0x05,0x01,           // Usage Page:          Generic Desktop Ctrls
	0x09,0x80,           // Usage:               Sys Control
	0xa1,0x01,           // Collection:          Application
		0x85,0x03,         //   REPORT ID:         3
		0x19,0x01,         //   Usage Minimum:     Pointer
		0x2a,0xb7,0x00,    //   Usage Maximum:     Sys Display LCD Autoscale
		0x15,0x01,         //   Logical Minimum:   1
		0x26,0xb7,0x00,    //   Logical Maximum:   183
		0x95,0x01,         //   Report Count:      1
		0x75,0x10,         //   Report Size:       16
		0x81,0x00,         //   Input:             Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
	0xc0,                // End Collection

	// ---------------------------------------------------------------------- consumer ctl report, 1KRO
	0x05,0x0c,           // Usage Page:          Consumer
	0x09,0x01,           // Usage:               Consumer Control
	0xa1,0x01,           // Collection:          Application
		0x85,0x04,         //   REPORT ID:         4
		0x19,0x01,         //   Usage Minimum:     Consumer Control
		0x2a,0xa0,0x02,    //   Usage Maximum:     0x02a0
		0x15,0x01,         //   Logical Minimum:   1
		0x26,0xa0,0x02,    //   Logical Maximum:   672
		0x95,0x01,         //   Report Count:      1
		0x75,0x10,         //   Report Size:       16
		0x81,0x00,         //   Input:             Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
	0xc0,                // End Collection

	// ---------------------------------------------------------------------- bitmap keyboard report, 240KRO
	0x05,0x01,           // Usage Page:          Generic Desktop Ctrls
	0x09,0x06,           // Usage:               Keyboard
	0xa1,0x01,           // Collection:          Application
		0x85,0x06,         //   REPORT ID:         6
		// ------------------------
		0x05,0x07,         //   Usage Page:        Kbrd/Keypad
		0x19,0xe0,         //   Usage Minimum:     0xe0
		0x29,0xe7,         //   Usage Maximum:     0xe7
		0x15,0x00,         //   Logical Minimum:   0
		0x25,0x01,         //   Logical Maximum:   1
		0x95,0x08,         //   Report Count:      8
		0x75,0x01,         //   Report Size:       1
		0x81,0x02,         //   Input:             Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
		// ------------------------
		0x05,0x07,         //   Usage Page:        Kbrd/Keypad
		0x19,0x00,         //   Usage Minimum:     0x00
		0x29,0xef,         //   Usage Maximum:     0xef
		0x15,0x00,         //   Logical Minimum:   0
		0x25,0x01,         //   Logical Maximum:   1
		0x95,0xf0,         //   Report Count:      240
		0x75,0x01,         //   Report Size:       1
		0x81,0x02,         //   Input:             Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
		// ------------------------
		0x05,0x08,         //   Usage Page:        LEDs
		0x19,0x01,         //   Usage Minimum:     Num Lock
		0x29,0x05,         //   Usage Maximum:     Kana
		0x95,0x05,         //   Report Count:      5
		0x75,0x01,         //   Report Size:       1
		0x91,0x02,         //   Output:            Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
		0x95,0x01,         //   Report Count:      1
		0x75,0x03,         //   Report Size:       3
		0x91,0x01,         //   Output:            Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
	0xc0,                // End Collection
};

#if 0
#define USB_HID_REPORT_DESC2_BDIM  71
static const u8 USB_HID_REPORT_DESC2[USB_HID_REPORT_DESC2_BDIM] = {  // mouse: 16-bit relative. WORKS
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x02,        // Usage (Mouse)
	0xa1, 0x01,        // Collection (Application)
		0x09, 0x01,        //   Usage (Pointer)
		0xa1, 0x00,        //   Collection (Physical)
			0x05, 0x09,        //     Usage Page (Button)
			0x15, 0x00,        //     Logical Minimum (0)
			0x25, 0x01,        //     Logical Maximum (1)
			0x19, 0x01,        //     Usage Minimum (0x01)
			0x29, 0x05,        //     Usage Maximum (0x05)
			0x75, 0x01,        //     Report Size (1)
			0x95, 0x05,        //     Report Count (5)
			0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
			0x95, 0x03,        //     Report Count (3)
			0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
			0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
			0x16, 0x01, 0x80,  //     Logical Minimum (-32767)
			0x26, 0xff, 0x7f,  //     Logical Maximum (32767)
			0x09, 0x30,        //     Usage (X)
			0x09, 0x31,        //     Usage (Y)
			0x75, 0x10,        //     Report Size (16)
			0x95, 0x02,        //     Report Count (2)
			0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
			0x15, 0x81,        //     Logical Minimum (-127)
			0x25, 0x7f,        //     Logical Maximum (127)
			0x09, 0x38,        //     Usage (Wheel)
			0x75, 0x08,        //     Report Size (8)
			0x95, 0x01,        //     Report Count (1)
			0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
			0x05, 0x0c,        //     Usage Page (Consumer)
			0x0a, 0x38, 0x02,  //     Usage (AC Pan)
			0x95, 0x01,        //     Report Count (1)
			0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
		0xc0,              //   End Collection
	0xc0,              // End Collection
};
#endif

#define USB_HID_REPORT_DESC2_BDIM  (60+15)
static const u8 USB_HID_REPORT_DESC2[USB_HID_REPORT_DESC2_BDIM] = {  // mouse: 16-bit absolute. WORKS! THANK YOU JESUS!!!!!!!
	// ---------------------------------------------------------------------- IN report
	0x05, 0x01,           // usage_page (generic desktop)
	0x09, 0x02,           // usage (mouse)
	0xa1, 0x01,           // collection (application)
		0x09, 0x01,         //   usage (pointer)
		0xa1, 0x00,         //   collection (physical)
			0x05, 0x09,       //     usage_page (button)
			0x19, 0x01,       //     usage_minimum (button 1)
			0x29, 0x03,       //     usage_maximum (button 3)
			0x15, 0x00,       //     logical_minimum (0)
			0x25, 0x01,       //     logical_maximum (1)
			0x95, 0x03,       //     report_count (3)
			0x75, 0x01,       //     report_size (1)
			0x81, 0x02,       //     input (data,var,abs)
			0x95, 0x01,       //     report_count (1)
			0x75, 0x05,       //     report_size (5)
			0x81, 0x03,       //     input (cnst,var,abs)
			0x05, 0x01,       //     usage_page (generic desktop)
			0x09, 0x30,       //     usage (x)
			0x09, 0x31,       //     usage (y)
			0x35, 0x00,       //     physical_minimum (0)
			0x46, 0xff,0x7f,  //     physical_maximum (32767)
			0x15, 0x00,       //     logical_minimum (0)
			0x26, 0xff,0x7f,  //     logical_maximum (32767)
			0x65, 0x11,       //     unit (si lin:distance)
			0x55, 0x0e,       //     unit_exponent (-2)
			0x75, 0x10,       //     report_size (16)
			0x95, 0x02,       //     report_count (2)
			0x81, 0x02,       //     input (data,var,abs)
		0xc0,               //   end_collection
	0xc0,                 // end_collection
	// ---------------------------------------------------------------------- OUT report
	0x09,0x06,    // usage (keyboard)
	0xa1,0x01,    // collection (application)
		0x15,0x00,  //   logical minimum (0)
		0x25,0xff,  //   logical maximum (255)
		0x75,0x08,  //   report size (8)
		0x95,0x40,  //   report count (64)
		0x91,0x02,  //   output (data,var,abs,no wrap,linear,preferred state,no null position,non-volatile)
	0xc0,         // end collection
};

#if 0  // USB configuration descriptor with 1 interface and 1 endpoint
#define USB_DEVICE_CFG_DESC_BDIM  (9+9+9+7)
static const u8 USB_DEVICE_CFG_DESC[USB_DEVICE_CFG_DESC_BDIM] = {
	// Configuration Descriptor?
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x02,                                                                // bDescriptorType:      Configuration Descriptor
	U16LO(USB_DEVICE_CFG_DESC_BDIM), U16HI(USB_DEVICE_CFG_DESC_BDIM),    // wTotalLength:         bdim for the entire @USB_DEVICE_CFG_DESC
	0x01,                                                                // bNumInterfaces:       1
	0x01,                                                                // bConfigurationValue:  1 (?)
	0x00,                                                                // iConfiguration:       index of String Descriptor describing this config
	0xa0,                                                                // bmAttributes:         0xc0: self-powered, 0x80: bus-powered, 0xa0: bus-powered & remote wake-up
	0x32,                                                                // bMaxPower:            100mA (units are 2mA, so 0x32 means 2*50mA, ie. 100mA)

	// ----------------------------------------------------------------------
	// Interface Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x04,                                                                // bDescriptorType:      Interface Descriptor
	0x00,                                                                // bInterfaceNumber:     Interface index?
	0x00,                                                                // bAlternateSetting:    value used to select alternative setting
	0x01,                                                                // bNumEndpoints:        how many Endpoints in this Interface
	0x03,                                                                // bInterfaceClass:      0x03 is HID class. this lets the generic USB driver know which class driver the app needs to be passed on to after enumeration is complete?
	0x00,                                                                // bInterfaceSubClass:   0 (boot)
	0x01,                                                                // bInterfaceProtocol:   1 (keyboard)
	0x00,                                                                // iInterface:           string index

	// ----------------------------------------------------------------------
	// HID Descriptor?
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x21,                                                                // bDescriptorType:      HID Descriptor
	0x11,0x01,                                                           // bcdHID:               1.11
	0x00,                                                                // bCountryCode:         hardware target country
	0x01,                                                                // bNumDescriptors:      number of HID descriptors to follow
	0x22,                                                                // bDescriptorType[0]:   HID
	U16LO(USB_HID_REPORT_DESC1_BDIM), U16HI(USB_HID_REPORT_DESC1_BDIM),  // wDescriptorLength[0]: total bdim of Report Descriptor

	// ----------------------------------------------------------------------
	// Endpoint Descriptor: Endpoint 1
	// 0x07,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	// 0x05,                                                                // bDescriptorType:      Endpoint Descriptor
	// 0x81,                                                                // bEndpointAddress:     IN: device2host
	// 0x03,                                                                // bmAttributes:         Interrupt
	// 0x08,0x00,                                                           // wMaxPacketSize:       8
	// 0x01,                                                                // bInterval

	// Endpoint Descriptor: Endpoint 1
	0x07,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x05,                                                                // bDescriptorType:      Endpoint Descriptor
	EP1_IN,                                                              // bEndpointAddress:     IN: device2host
	0x03,                                                                // bmAttributes:         Interrupt
	0x20,0x00,                                                           // wMaxPacketSize:       32
	0x01,                                                                // bInterval
};
#endif

#if 1  // USB configuration descriptor with 3 interfaces and 3 endpoints
#define USB_DEVICE_CFG_DESC_BDIM  (9 + 9+9+7 + 9+9+7 + 9+9+7+7)
static const u8 USB_DEVICE_CFG_DESC[USB_DEVICE_CFG_DESC_BDIM] = {
	// Configuration Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x02,                                                                // bDescriptorType:      Configuration Descriptor
	U16LO(USB_DEVICE_CFG_DESC_BDIM), U16HI(USB_DEVICE_CFG_DESC_BDIM),    // wTotalLength:         bdim for the entire @USB_DEVICE_CFG_DESC
	0x03,                                                                // bNumInterfaces
	0x01,                                                                // bConfigurationValue:  1 (?)
	0x00,                                                                // iConfiguration:       index of String Descriptor describing this config
	0xa0,                                                                // bmAttributes:         0xc0: self-powered, 0x80: bus-powered, 0xa0: bus-powered & remote wake-up
	0x32,                                                                // bMaxPower:            100mA (units are 2mA, so 0x32 means 2*50mA, ie. 100mA)

	// ----------------------------------------------------------------------
	// Interface Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x04,                                                                // bDescriptorType:      Interface Descriptor
	0x00,                                                                // bInterfaceNumber:     Interface index
	0x00,                                                                // bAlternateSetting:    value used to select alternative setting
	0x01,                                                                // bNumEndpoints:        how many Endpoints in this Interface
	0x03,                                                                // bInterfaceClass:      0x03 is HID class. this lets the generic USB driver know which class driver the app needs to be passed on to after enumeration is complete?
	0x01,                                                                // bInterfaceSubClass:   0: no subclass, 1: boot
	0x01,                                                                // bInterfaceProtocol:   0: none, 1: keyboard, 2: mouse
	0x00,                                                                // iInterface:           string index

	// HID Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x21,                                                                // bDescriptorType:      HID Descriptor
	0x11,0x01,                                                           // bcdHID:               1.11
	0x00,                                                                // bCountryCode:         hardware target country
	0x01,                                                                // bNumDescriptors:      number of HID descriptors to follow
	0x22,                                                                // bDescriptorType[0]:   HID
	U16LO(USB_HID_REPORT_DESC0_BDIM), U16HI(USB_HID_REPORT_DESC0_BDIM),  // wDescriptorLength[0]: total bdim of Report Descriptor

	// Endpoint Descriptor: Endpoint 1 IN
	0x07,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x05,                                                                // bDescriptorType:      Endpoint Descriptor
	EP1_IN,                                                              // bEndpointAddress:     IN: device2host
	0x03,                                                                // bmAttributes:         Interrupt
	EP1_IN_SIZE,0x00,                                                    // wMaxPacketSize:       8
	0x01,                                                                // bInterval

	// ----------------------------------------------------------------------
	// Interface Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x04,                                                                // bDescriptorType:      Interface Descriptor
	0x01,                                                                // bInterfaceNumber:     Interface index
	0x00,                                                                // bAlternateSetting:    value used to select alternative setting
	0x01,                                                                // bNumEndpoints:        how many Endpoints in this Interface
	0x03,                                                                // bInterfaceClass:      0x03 is HID class. this lets the generic USB driver know which class driver the app needs to be passed on to after enumeration is complete?
	0x00,                                                                // bInterfaceSubClass:   0: no subclass, 1: boot
	0x00,                                                                // bInterfaceProtocol:   0: none, 1: keyboard, 2: mouse
	0x00,                                                                // iInterface:           string index

	// HID Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x21,                                                                // bDescriptorType:      HID Descriptor
	0x11,0x01,                                                           // bcdHID:               1.11
	0x00,                                                                // bCountryCode:         hardware target country
	0x01,                                                                // bNumDescriptors:      number of HID descriptors to follow
	0x22,                                                                // bDescriptorType[0]:   HID
	U16LO(USB_HID_REPORT_DESC1_BDIM), U16HI(USB_HID_REPORT_DESC1_BDIM),  // wDescriptorLength[0]: total bdim of Report Descriptor

	// Endpoint Descriptor: Endpoint 2 IN
	0x07,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x05,                                                                // bDescriptorType:      Endpoint Descriptor
	EP2_IN,                                                              // bEndpointAddress:     IN: device2host
	0x03,                                                                // bmAttributes:         Interrupt
	EP2_IN_SIZE,0x00,                                                    // wMaxPacketSize:       32
	0x01,                                                                // bInterval

	// ----------------------------------------------------------------------
	// Interface Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x04,                                                                // bDescriptorType:      Interface Descriptor
	0x02,                                                                // bInterfaceNumber:     Interface index
	0x00,                                                                // bAlternateSetting:    value used to select alternative setting
	0x02,                                                                // bNumEndpoints:        how many Endpoints in this Interface
	0x03,                                                                // bInterfaceClass:      0x03 is HID class. this lets the generic USB driver know which class driver the app needs to be passed on to after enumeration is complete?
	0x00,                                                                // bInterfaceSubClass:   0: no subclass, 1: boot
	0x00,                                                                // bInterfaceProtocol:   0: none, 1: keyboard, 2: mouse
	0x00,                                                                // iInterface:           string index

	// HID Descriptor
	0x09,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x21,                                                                // bDescriptorType:      HID Descriptor
	0x11,0x01,                                                           // bcdHID:               1.11
	0x00,                                                                // bCountryCode:         hardware target country
	0x01,                                                                // bNumDescriptors:      number of HID descriptors to follow
	0x22,                                                                // bDescriptorType[0]:   HID
	U16LO(USB_HID_REPORT_DESC2_BDIM), U16HI(USB_HID_REPORT_DESC2_BDIM),  // wDescriptorLength[0]: total bdim of Report Descriptor

	// Endpoint Descriptor: Endpoint 3 IN
	0x07,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x05,                                                                // bDescriptorType:      Endpoint Descriptor
	EP3_IN,                                                              // bEndpointAddress:     IN: device2host
	0x03,                                                                // bmAttributes:         Interrupt
	EP3_IN_SIZE,0x00,                                                    // wMaxPacketSize:       7
	0x01,                                                                // bInterval

	// Endpoint Descriptor: Endpoint 3 OUT
	0x07,                                                                // bLength:              bdim for this Descriptor (inside @USB_DEVICE_CFG_DESC)
	0x05,                                                                // bDescriptorType:      Endpoint Descriptor
	EP3_OUT,                                                             // bEndpointAddress:     OUT: host2device
	0x03,                                                                // bmAttributes:         Interrupt
	EP3_OUT_SIZE,0x00,                                                   // wMaxPacketSize:       64
	0x01,                                                                // bInterval
};
#endif
