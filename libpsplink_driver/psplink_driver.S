	.set noreorder

#include "pspimport.s"

#ifdef F_psplink_driver_0000
	IMPORT_START "psplink_driver",0x00090000
#endif
#ifdef F_psplink_driver_0001
	IMPORT_FUNC  "psplink_driver",0x670C6041,psplinkPresent
#endif
#ifdef F_psplink_driver_0002
	IMPORT_FUNC  "psplink_driver",0xB03642D9,psplinkExitShell
#endif
#ifdef F_psplink_driver_0003
	IMPORT_FUNC  "psplink_driver",0x8F31ACE7,psplinkReferModule
#endif
#ifdef F_psplink_driver_0004
	IMPORT_FUNC  "psplink_driver",0x0D4438D3,psplinkGetFullThreadContext
#endif
#ifdef F_psplink_driver_0005
	IMPORT_FUNC  "psplink_driver",0x754F9BB8,psplinkReferModuleByName
#endif
#ifdef F_psplink_driver_0006
	IMPORT_FUNC  "psplink_driver",0x2FF7228E,psplinkReferThreadsByModule
#endif
#ifdef F_psplink_driver_0007
	IMPORT_FUNC  "psplink_driver",0x4A45386D,psplinkReset
#endif
#ifdef F_psplink_driver_0008
	IMPORT_FUNC  "psplink_driver",0xCBA8647D,apiHookByName
#endif
#ifdef F_psplink_driver_0009
	IMPORT_FUNC  "psplink_driver",0x4ED59445,apiHookByNid
#endif
#ifdef F_psplink_driver_0010
	IMPORT_FUNC  "psplink_driver",0xA79585AA,debugRegisterEventHandler
#endif
#ifdef F_psplink_driver_0011
	IMPORT_FUNC  "psplink_driver",0x4FD9B166,debugUnregisterEventHandler
#endif
#ifdef F_psplink_driver_0012
	IMPORT_FUNC  "psplink_driver",0xDA0CE45E,debugWaitDebugEvent
#endif
#ifdef F_psplink_driver_0013
	IMPORT_FUNC  "psplink_driver",0x0B7159E4,debugDeleteBP
#endif
#ifdef F_psplink_driver_0014
	IMPORT_FUNC  "psplink_driver",0x90440E35,debugDisableBP
#endif
#ifdef F_psplink_driver_0015
	IMPORT_FUNC  "psplink_driver",0x4821391E,debugEnableBP
#endif
#ifdef F_psplink_driver_0016
	IMPORT_FUNC  "psplink_driver",0x8BE7EA9B,debugFindBPByIndex
#endif
#ifdef F_psplink_driver_0017
	IMPORT_FUNC  "psplink_driver",0xFCF4D9D3,debugSetBP
#endif
#ifdef F_psplink_driver_0018
	IMPORT_FUNC  "psplink_driver",0x45EAC8C0,debugBreakThread
#endif
