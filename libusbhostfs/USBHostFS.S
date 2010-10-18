	.set noreorder

#include "pspimport.s"

#ifdef F_USBHostFS_0000
	IMPORT_START "USBHostFS",0x40090000
#endif
#ifdef F_USBHostFS_0001
	IMPORT_FUNC  "USBHostFS",0x75246D41,usbAsyncRegister
#endif
#ifdef F_USBHostFS_0002
	IMPORT_FUNC  "USBHostFS",0x587DDEDA,usbAsyncUnregister
#endif
#ifdef F_USBHostFS_0003
	IMPORT_FUNC  "USBHostFS",0xBA9F1145,usbAsyncRead
#endif
#ifdef F_USBHostFS_0004
	IMPORT_FUNC  "USBHostFS",0xE4C00162,usbAsyncReadWithTimeout
#endif
#ifdef F_USBHostFS_0005
	IMPORT_FUNC  "USBHostFS",0x5D1F19A0,usbAsyncWrite
#endif
#ifdef F_USBHostFS_0006
	IMPORT_FUNC  "USBHostFS",0x89D91015,usbAsyncFlush
#endif
#ifdef F_USBHostFS_0007
	IMPORT_FUNC  "USBHostFS",0xFF18E862,usbWaitForConnect
#endif
#ifdef F_USBHostFS_0008
	IMPORT_FUNC  "USBHostFS",0x4ABA9C2B,usbWriteBulkData
#endif
#ifdef F_USBHostFS_0009
	IMPORT_FUNC  "USBHostFS",0x642EF71E,usbLockBus
#endif
#ifdef F_USBHostFS_0010
	IMPORT_FUNC  "USBHostFS",0x439E6C6C,usbUnlockBus
#endif
