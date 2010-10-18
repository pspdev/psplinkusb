#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <psptypes.h>

/* Define maximum number of thread exception context */
#define PSPLINK_MAX_CONTEXT 8

#define PSPLINK_EXTYPE_NORMAL 0
#define PSPLINK_EXTYPE_DEBUG  1

/* Define continue flags */
#define PSP_EXCEPTION_EXIT        0
#define PSP_EXCEPTION_CONTINUE    1
#define PSP_EXCEPTION_STEP        2
#define PSP_EXCEPTION_SKIP        3
#define PSP_EXCEPTION_NOT_HANDLED 4

/** Structure to hold the register data associated with an exception */
typedef struct _PsplinkRegBlock
{
	unsigned int frame[6];
	/** Array of the 32 GPRs */
	unsigned int r[32];
	/** The status register */
	unsigned int status;
	/** lo */
	unsigned int lo;
	unsigned int hi;
	unsigned int badvaddr;
	unsigned int cause;
	unsigned int epc;
	float fpr[32];
	unsigned int fsr;
	unsigned int fir;
	unsigned int frame_ptr;
	unsigned int unused;
	/* Unused on PSP */
	unsigned int index;
	unsigned int random;
	unsigned int entrylo0;
	unsigned int entrylo1;
	unsigned int context;
	unsigned int pagemask;
	unsigned int wired;
	unsigned int cop0_7;
	unsigned int cop0_8;
	unsigned int cop0_9;
	unsigned int entryhi;
	unsigned int cop0_11;
	unsigned int cop0_12;
	unsigned int cop0_13;
	unsigned int cop0_14;
	/* PRId should still be okay */
	unsigned int prid;
	/* Type of exception (normal or debug) */
	unsigned int type;
	/* Pad vfpu to 128bit boundary */
	int pad;
	float vfpu[128];
} PsplinkRegBlock;

/* A thread context during an exception */
struct PsplinkContext
{
	int valid;
	struct PsplinkContext *pNext;
	PsplinkRegBlock regs;
	SceUID thid;
	unsigned int drcntl;
	/* Continue type */
	int cont;
	/* Indicates whether this was an error or planned */
	int error;
};

#define VFPU_PRINT_SINGLE 0
#define VFPU_PRINT_COL    1
#define VFPU_PRINT_ROW    2
#define VFPU_PRINT_MATRIX 3
#define VFPU_PRINT_TRANS  4

extern struct PsplinkContext *g_currex;

void exceptionInit(void);
void exceptionPrint(struct PsplinkContext *ctx);
void exceptionFpuPrint(struct PsplinkContext *ctx);
void exceptionVfpuPrint(struct PsplinkContext *ctx, int mode);
unsigned int *exceptionGetReg(const char *reg);
void exceptionResume(struct PsplinkContext *ctx, int cont);
void exceptionPrintFPURegs(float *pFpu, unsigned int fsr, unsigned int fir);
void exceptionPrintCPURegs(unsigned int *pRegs);
void exceptionList(void);
void exceptionSetCtx(int ex);

#endif
