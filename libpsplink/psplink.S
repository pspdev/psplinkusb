	.set noreorder

#include "pspimport.s"

// Build files
// psplink_0000.o psplink_0001.o psplink_0002.o psplink_0003.o psplink_0004.o psplink_0005.o psplink_0006.o psplink_0007.o psplink_0008.o psplink_0009.o psplink_0010.o 

#ifdef F_psplink_0000
	IMPORT_START "psplink",0x40090000
#endif
#ifdef F_psplink_0001
	IMPORT_FUNC  "psplink",0x670C6041,psplinkPresent
#endif
#ifdef F_psplink_0002
	IMPORT_FUNC  "psplink",0xB03642D9,psplinkExitShell
#endif
#ifdef F_psplink_0003
	IMPORT_FUNC  "psplink",0x8F31ACE7,psplinkReferModule
#endif
#ifdef F_psplink_0004
	IMPORT_FUNC  "psplink",0x754F9BB8,psplinkReferModuleByName
#endif
#ifdef F_psplink_0005
	IMPORT_FUNC  "psplink",0x4A45386D,psplinkReset
#endif
#ifdef F_psplink_0006
	IMPORT_FUNC  "psplink",0xCBA8647D,apiHookByName
#endif
#ifdef F_psplink_0007
	IMPORT_FUNC  "psplink",0x4ED59445,apiHookByNid
#endif
#ifdef F_psplink_0008
	IMPORT_FUNC  "psplink",0x3E371C7A,_apiHookHandle
#endif
#ifdef F_psplink_0009
	IMPORT_FUNC  "psplink",0x4285B6A4,_apiHookReturn
#endif
#ifdef F_psplink_0010
	IMPORT_FUNC  "psplink",0x0A72C44E,apiHookRegisterUserDispatch
#endif
