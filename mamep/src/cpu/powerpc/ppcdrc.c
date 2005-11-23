/*
    PowerPC 4xx/6xx Dynamic Recompiler for x86

    Written by Ville Linde
*/

#include "driver.h"
#include "ppc.h"
#include "mamedbg.h"
#include "x86drc.h"


#define PPC_DRC
#define LOG_CODE			(0)

#define CACHE_SIZE			(16 * 1024 * 1024)
#define MAX_INSTRUCTIONS	512


#if (HAS_PPC603)
static void ppcdrc603_init(void);
static void ppcdrc603_exit(void);
static void ppcdrc603_reset(void *param);
static int ppcdrc603_execute(int cycles);
static void ppcdrc603_set_irq_line(int irqline, int state);
#endif
#if (HAS_PPC602)
static void ppcdrc602_init(void);
static void ppcdrc602_exit(void);
static void ppcdrc602_reset(void *param);
static int ppcdrc602_execute(int cycles);
static void ppcdrc602_set_irq_line(int irqline, int state);
#endif
#if (HAS_PPC403)
UINT8 ppc403_spu_r(UINT32 a);
void ppc403_spu_w(UINT32 a, UINT8 d);
static void ppcdrc403_init(void);
static void ppcdrc403_exit(void);
static void ppcdrc403_reset(void *param);
static int ppcdrc403_execute(int cycles);
static void ppcdrc403_set_irq_line(int irqline, int state);
#endif

static void ppcdrc_init(void);
static void ppcdrc_reset(drc_core *drc);
static void ppcdrc_recompile(drc_core *drc);
static void ppcdrc_entrygen(drc_core *drc);

#define RD				((op >> 21) & 0x1F)
#define RT				((op >> 21) & 0x1f)
#define RS				((op >> 21) & 0x1f)
#define RA				((op >> 16) & 0x1f)
#define RB				((op >> 11) & 0x1f)
#define RC				((op >> 6) & 0x1f)

#define MB				((op >> 6) & 0x1f)
#define ME				((op >> 1) & 0x1f)
#define SH				((op >> 11) & 0x1f)
#define BO				((op >> 21) & 0x1f)
#define BI				((op >> 16) & 0x1f)
#define CRFD			((op >> 23) & 0x7)
#define CRFA			((op >> 18) & 0x7)
#define FXM				((op >> 12) & 0xff)
#define SPR				(((op >> 16) & 0x1f) | ((op >> 6) & 0x3e0))

#define SIMM16			(INT32)(INT16)(op & 0xffff)
#define UIMM16			(UINT32)(op & 0xffff)

#define RCBIT			(op & 0x1)
#define OEBIT			(op & 0x400)
#define AABIT			(op & 0x2)
#define LKBIT			(op & 0x1)

#define REG(x)			(ppc.r[x])
#define LR				(ppc.lr)
#define CTR				(ppc.ctr)
#define XER				(ppc.xer)
#define CR(x)			(ppc.cr[x])
#define MSR				(ppc.msr)
#define SRR0			(ppc.srr0)
#define SRR1			(ppc.srr1)
#define SRR2			(ppc.srr2)
#define SRR3			(ppc.srr3)
#define EVPR			(ppc.evpr)
#define EXIER			(ppc.exier)
#define EXISR			(ppc.exisr)
#define DEC				(ppc.dec)


// Stuff added for the 6xx
#define FPR(x)			(ppc.fpr[x])
#define FM				((op >> 17) & 0xFF)
#define SPRF			(((op >> 6) & 0x3E0) | ((op >> 16) & 0x1F))


#define CHECK_SUPERVISOR()			\
	if((ppc.msr & 0x4000) != 0){	\
	}

#define CHECK_FPU_AVAILABLE()		\
	if((ppc.msr & 0x2000) == 0){	\
	}

static UINT32		ppc_field_xlat[256];



#define FPSCR_FX		0x80000000
#define FPSCR_FEX		0x40000000
#define FPSCR_VX		0x20000000
#define FPSCR_OX		0x10000000
#define FPSCR_UX		0x08000000
#define FPSCR_ZX		0x04000000
#define FPSCR_XX		0x02000000



#define BITMASK_0(n)	(UINT32)(((UINT64)1 << n) - 1)
#define CRBIT(x)		((ppc.cr[x / 4] & (1 << (3 - (x % 4)))) ? 1 : 0)
#define _BIT(n)			(1 << (n))
#define GET_ROTATE_MASK(mb,me)		(ppc_rotate_mask[mb][me])
#define ADD_CA(r,a,b)		((UINT32)r < (UINT32)a)
#define SUB_CA(r,a,b)		(!((UINT32)a < (UINT32)b))
#define ADD_OV(r,a,b)		((~((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)
#define SUB_OV(r,a,b)		(( ((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)

#define XER_SO			0x80000000
#define XER_OV			0x40000000
#define XER_CA			0x20000000

#define MSR_POW			0x00040000	/* Power Management Enable */
#define MSR_WE			0x00040000
#define MSR_CE			0x00020000
#define MSR_ILE			0x00010000	/* Interrupt Little Endian Mode */
#define MSR_EE			0x00008000	/* External Interrupt Enable */
#define MSR_PR			0x00004000	/* Problem State */
#define MSR_FP			0x00002000	/* Floating Point Available */
#define MSR_ME			0x00001000	/* Machine Check Enable */
#define MSR_FE0			0x00000800
#define MSR_SE			0x00000400	/* Single Step Trace Enable */
#define MSR_BE			0x00000200	/* Branch Trace Enable */
#define MSR_DE			0x00000200
#define MSR_FE1			0x00000100
#define MSR_IP			0x00000040	/* Interrupt Prefix */
#define MSR_IR			0x00000020	/* Instruction Relocate */
#define MSR_DR			0x00000010	/* Data Relocate */
#define MSR_PE			0x00000008
#define MSR_PX			0x00000004
#define MSR_RI			0x00000002	/* Recoverable Interrupt Enable */
#define MSR_LE			0x00000001

#define TSR_ENW			0x80000000
#define TSR_WIS			0x40000000

#define BYTE_REVERSE16(x)	((((x) >> 8) & 0xff) | (((x) << 8) & 0xff00))
#define BYTE_REVERSE32(x)	((((x) >> 24) & 0xff) | (((x) >> 8) & 0xff00) | (((x) << 8) & 0xff0000) | (((x) << 24) & 0xff000000))

typedef struct {
	UINT32 cr;
	UINT32 da;
	UINT32 sa;
	UINT32 ct;
	UINT32 cc;
} DMA_REGS;

typedef struct {
	UINT8 spls;
	UINT8 sphs;
	UINT16 brd;
	UINT8 spctl;
	UINT8 sprc;
	UINT8 sptc;
	UINT8 sprb;
	UINT8 sptb;
	void *rx_timer;
	void *tx_timer;
} SPU_REGS;

typedef union {
	UINT64	id;
	double	fd;
} FPR;

typedef union {
	UINT32 i;
	float f;
} FPR32;

typedef struct {
	UINT32 u;
	UINT32 l;
} BATENT;


typedef struct {
	UINT32 r[32];
	UINT32 pc;
	UINT32 npc;

	UINT32 lr;
	UINT32 ctr;
	UINT32 xer;
	UINT32 msr;
	UINT8 cr[8];
	UINT32 pvr;
	UINT32 srr0;
	UINT32 srr1;
	UINT32 srr2;
	UINT32 srr3;
	UINT32 hid0;
	UINT32 hid1;
	UINT32 sdr1;
	UINT32 sprg[4];

	UINT32 dsisr;
	UINT32 dar;
	UINT32 ear;

	BATENT ibat[4];
	BATENT dbat[4];

	UINT32 evpr;
	UINT32 exier;
	UINT32 exisr;
	UINT32 bear;
	UINT32 besr;
	UINT32 iocr;
	UINT32 br[8];
	UINT32 iabr;
	UINT32 esr;
	UINT32 iccr;
	UINT32 dccr;
	UINT32 pit;
	UINT32 tsr;
	UINT32 tcr;
	UINT32 dbsr;
	UINT32 sgr;
	UINT32 pid;
	UINT32 pbl1, pbl2, pbu1, pbu2;
	UINT32 fit_bit;
	UINT32 fit_int_enable;
	UINT32 wdt_bit;
	UINT32 wdt_int_enable;

	SPU_REGS spu;
	DMA_REGS dma[4];
	UINT32 dmasr;

	int reserved;
	UINT32 reserved_address;

	int exception_pending;
	int external_int;

	UINT64 tb;			/* 56-bit timebase register */

	int (*irq_callback)(int irqline);
	drc_core *drc;
	UINT32 drcoptions;

	void *		invoke_exception_handler;
	void *		generate_interrupt_exception;
	void *		generate_syscall_exception;
	void *		generate_decrementer_exception;
	void *		generate_trap_exception;
	void *		generate_dsi_exception;
	void *		generate_isi_exception;

	// PowerPC 60x specific registers */
	UINT32 dec, dec_frac;
	UINT32 fpscr;

	FPR	fpr[32];
	UINT32 sr[16];

	int is603;
	int is602;

	/* PowerPC 602 specific registers */
	UINT32 lt;
	UINT32 sp;
	UINT32 ibr;

	/* PowerPC function pointers for memory accesses/exceptions */
	UINT8 (*read8)(offs_t address);
	UINT16 (*read16)(offs_t address);
	UINT32 (*read32)(offs_t address);
	UINT64 (*read64)(offs_t address);
	void (*write8)(offs_t address, UINT8 data);
	void (*write16)(offs_t address, UINT16 data);
	void (*write32)(offs_t address, UINT32 data);
	void (*write64)(offs_t address, UINT64 data);
	UINT16 (*read16_unaligned)(offs_t address);
	UINT32 (*read32_unaligned)(offs_t address);
	UINT64 (*read64_unaligned)(offs_t address);
	void (*write16_unaligned)(offs_t address, UINT16 data);
	void (*write32_unaligned)(offs_t address, UINT32 data);
	void (*write64_unaligned)(offs_t address, UINT64 data);

	/* saved ESP when entering entry point */
	UINT32 host_esp;
} PPC_REGS;



typedef struct {
	int code;
	int subcode;
	UINT32 (* handler)(drc_core *, UINT32);
} PPC_OPCODE;



static int ppc_icount;
static int ppc_tb_base_icount;
static int ppc_dec_base_icount;
static int ppc_dec_trigger_cycle;
static int bus_freq_multiplier = 1;
static PPC_REGS ppc;
static UINT32 ppc_rotate_mask[32][32];

#define ROPCODE(pc)			cpu_readop32(pc)
#define ROPCODE64(pc)		cpu_readop64(DWORD_XOR_BE(pc))

INLINE void SET_CR0(INT32 rd)
{
	if( rd < 0 ) {
		CR(0) = 0x8;
	} else if( rd > 0 ) {
		CR(0) = 0x4;
	} else {
		CR(0) = 0x2;
	}

	if( XER & XER_SO )
		CR(0) |= 0x1;
}

INLINE void SET_CR1(void)
{
	CR(1) = (ppc.fpscr >> 28) & 0xf;
}

INLINE void SET_ADD_OV(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( ADD_OV(rd, ra, rb) )
		XER |= XER_SO | XER_OV;
	else
		XER &= ~XER_OV;
}

INLINE void SET_SUB_OV(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( SUB_OV(rd, ra, rb) )
		XER |= XER_SO | XER_OV;
	else
		XER &= ~XER_OV;
}

INLINE void SET_ADD_CA(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( ADD_CA(rd, ra, rb) )
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

INLINE void SET_SUB_CA(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( SUB_CA(rd, ra, rb) )
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

INLINE UINT32 check_condition_code(UINT32 bo, UINT32 bi)
{
	UINT32 ctr_ok;
	UINT32 condition_ok;
	UINT32 bo0 = (bo & 0x10) ? 1 : 0;
	UINT32 bo1 = (bo & 0x08) ? 1 : 0;
	UINT32 bo2 = (bo & 0x04) ? 1 : 0;
	UINT32 bo3 = (bo & 0x02) ? 1 : 0;

	if( bo2 == 0 )
		--CTR;

	ctr_ok = bo2 | ((CTR != 0) ^ bo3);
	condition_ok = bo0 | (CRBIT(bi) ^ (~bo1 & 0x1));

	return ctr_ok && condition_ok;
}

INLINE UINT64 ppc_read_timebase(void)
{
	int cycles = ppc_tb_base_icount - ppc_icount;

	if (ppc.is603 || ppc.is602)
	{
		// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
		return ppc.tb + (cycles / 4);
	}
	else
	{
		// timebase is incremented once every core cycle on PPC403
		return ppc.tb + cycles;
	}
}

INLINE void ppc_write_timebase_l(UINT32 tbl)
{
	ppc_tb_base_icount = ppc_icount;

	ppc.tb &= ~0xffffffff;
	ppc.tb |= tbl;
}

INLINE void ppc_write_timebase_h(UINT32 tbh)
{
	ppc_tb_base_icount = ppc_icount;

	ppc.tb &= 0xffffffff;
	ppc.tb |= (UINT64)(tbh) << 32;
}

INLINE UINT32 read_decrementer(void)
{
	int cycles = ppc_dec_base_icount - ppc_icount;

	// decrementer is decremented once every four bus clock cycles, so adjust the cycles accordingly
	return DEC - (cycles / (bus_freq_multiplier * 2));
}

INLINE void write_decrementer(UINT32 value)
{
	ppc_dec_base_icount = ppc_icount + (ppc_dec_base_icount - ppc_icount) % (bus_freq_multiplier * 2);

	DEC = value;

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - ppc_icount) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = ppc_icount - DEC;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;
	}
}

/*********************************************************************/

INLINE void ppc_exception(int exception_type)
{
	void *exception_code = NULL;
	void (*invoke_exception_handler)(void *handler);

	switch(exception_type)
	{
		case EXCEPTION_DECREMENTER:
			exception_code = ppc.generate_decrementer_exception;
			break;
		case EXCEPTION_DSI:
			exception_code = ppc.generate_dsi_exception;
			break;
		case EXCEPTION_ISI:
			exception_code = ppc.generate_isi_exception;
			break;
		default:
			osd_die("Unknown exception %d\n", exception_type);
			break;
	}

	memcpy(&invoke_exception_handler, &ppc.invoke_exception_handler, sizeof(invoke_exception_handler));
	invoke_exception_handler(exception_code);
}

/*********************************************************************/

INLINE void ppc_set_spr(int spr, UINT32 value)
{
	switch (spr)
	{
		case SPR_LR:		LR = value; return;
		case SPR_CTR:		CTR = value; return;
		case SPR_XER:		XER = value; return;
		case SPR_SRR0:		ppc.srr0 = value; return;
		case SPR_SRR1:		ppc.srr1 = value; return;
		case SPR_SPRG0:		ppc.sprg[0] = value; return;
		case SPR_SPRG1:		ppc.sprg[1] = value; return;
		case SPR_SPRG2:		ppc.sprg[2] = value; return;
		case SPR_SPRG3:		ppc.sprg[3] = value; return;
		case SPR_PVR:		return;
	}

#if (HAS_PPC603 || HAS_PPC602)
	if(ppc.is603 || ppc.is602) {
		switch(spr)
		{
			case SPR603E_DEC:
				if((value & 0x80000000) && !(DEC & 0x80000000))
				{
					/* trigger interrupt */
					if( MSR & MSR_EE )
						ppc_exception(EXCEPTION_DECREMENTER);
				}
				write_decrementer(value);
				return;

			case SPR603E_TBL_W:
			case SPR603E_TBL_R: // special 603e case
				ppc_write_timebase_l(value);
				return;

			case SPR603E_TBU_R:
			case SPR603E_TBU_W: // special 603e case
				ppc_write_timebase_h(value);
				return;

			case SPR603E_HID0:
				ppc.hid0 = value;
				return;

			case SPR603E_HID1:
				ppc.hid1 = value;
				return;

			case SPR603E_IBAT0L:		ppc.ibat[0].l = value; return;
			case SPR603E_IBAT0U:		ppc.ibat[0].u = value; return;
			case SPR603E_IBAT1L:		ppc.ibat[1].l = value; return;
			case SPR603E_IBAT1U:		ppc.ibat[1].u = value; return;
			case SPR603E_IBAT2L:		ppc.ibat[2].l = value; return;
			case SPR603E_IBAT2U:		ppc.ibat[2].u = value; return;
			case SPR603E_IBAT3L:		ppc.ibat[3].l = value; return;
			case SPR603E_IBAT3U:		ppc.ibat[3].u = value; return;
			case SPR603E_DBAT0L:		ppc.dbat[0].l = value; return;
			case SPR603E_DBAT0U:		ppc.dbat[0].u = value; return;
			case SPR603E_DBAT1L:		ppc.dbat[1].l = value; return;
			case SPR603E_DBAT1U:		ppc.dbat[1].u = value; return;
			case SPR603E_DBAT2L:		ppc.dbat[2].l = value; return;
			case SPR603E_DBAT2U:		ppc.dbat[2].u = value; return;
			case SPR603E_DBAT3L:		ppc.dbat[3].l = value; return;
			case SPR603E_DBAT3U:		ppc.dbat[3].u = value; return;

			case SPR603E_SDR1:
				ppc.sdr1 = value;
				return;

			case SPR603E_IABR:			ppc.iabr = value; return;
		}
	}
#endif

#if (HAS_PPC602)
	if (ppc.is602) {
		switch(spr)
		{
			case SPR602_LT:			printf("ppc: LT = %08X\n", value); ppc.lt = value; return;
			case SPR602_IBR:		printf("ppc: IBR = %08X\n", value); ppc.ibr = value; return;
			case SPR602_SP:			printf("ppc: SP = %08X\n", value); ppc.sp = value; return;
			case SPR602_TCR:		printf("ppc: TCR = %08X\n", value); ppc.tcr = value; return;
		}
	}
#endif

#if (HAS_PPC403)
	if (!ppc.is603 && !ppc.is602) {
		switch(spr)
		{
			case SPR403_TBHI:		ppc_write_timebase_h(value); return;
			case SPR403_TBLO:		ppc_write_timebase_l(value); return;

			case SPR403_TSR:
				ppc.tsr &= ~value; // 1 clears, 0 does nothing
				return;

			case SPR403_TCR:
				switch((value >> 24) & 0x3)
				{
					case 0: ppc.fit_bit = 1 << 8; break;
					case 1: ppc.fit_bit = 1 << 12; break;
					case 2: ppc.fit_bit = 1 << 16; break;
					case 3: ppc.fit_bit = 1 << 20; break;
				}
				switch((value >> 30) & 0x3)
				{
					case 0: ppc.wdt_bit = 1 << 16; break;
					case 1: ppc.wdt_bit = 1 << 20; break;
					case 2: ppc.wdt_bit = 1 << 24; break;
					case 3: ppc.wdt_bit = 1 << 28; break;
				}
				ppc.fit_int_enable = (value >> 23) & 0x1;
				ppc.wdt_int_enable = (value >> 27) & 0x1;
				ppc.tcr = value;
				return;

			case SPR403_ESR:		ppc.esr = value; return;
			case SPR403_ICCR:		ppc.iccr = value; return;
			case SPR403_DCCR:		ppc.dccr = value; return;
			case SPR403_EVPR:		EVPR = value & 0xffff0000; return;
			case SPR403_PIT:		ppc.pit = value; return;
			case SPR403_SGR:		ppc.sgr = value; return;
			case SPR403_DBSR:		ppc.dbsr = value; return;
			case SPR403_DCWR:		return;
			case SPR403_PID:		ppc.pid = value; return;
			case SPR403_PBL1:		ppc.pbl1 = value; printf("PPC: PBL1 = %08X\n", ppc.pbl1); return;
			case SPR403_PBU1:		ppc.pbu1 = value; printf("PPC: PBU1 = %08X\n", ppc.pbu1); return;
			case SPR403_PBL2:		ppc.pbl2 = value; printf("PPC: PBL2 = %08X\n", ppc.pbl2); return;
			case SPR403_PBU2:		ppc.pbu2 = value; printf("PPC: PBU2 = %08X\n", ppc.pbu2); return;
		}
	}
#endif

	osd_die("ppc: set_spr: unknown spr %d (%03X) !\n", spr, spr);
}

INLINE UINT32 ppc_get_spr(int spr)
{
	switch(spr)
	{
		case SPR_LR:		return LR;
		case SPR_CTR:		return CTR;
		case SPR_XER:		return XER;
		case SPR_SRR0:		return ppc.srr0;
		case SPR_SRR1:		return ppc.srr1;
		case SPR_SPRG0:		return ppc.sprg[0];
		case SPR_SPRG1:		return ppc.sprg[1];
		case SPR_SPRG2:		return ppc.sprg[2];
		case SPR_SPRG3:		return ppc.sprg[3];
		case SPR_PVR:		return ppc.pvr;
	}

#if (HAS_PPC403)
	if (!ppc.is603 && !ppc.is602)
	{
		switch (spr)
		{
			case SPR403_TBLU:
			case SPR403_TBLO:		return (UINT32)(ppc_read_timebase());
			case SPR403_TBHU:
			case SPR403_TBHI:		return (UINT32)(ppc_read_timebase() >> 32);

			case SPR403_EVPR:		return EVPR;
			case SPR403_ESR:		return ppc.esr;
			case SPR403_TCR:		return ppc.tcr;
			case SPR403_ICCR:		return ppc.iccr;
			case SPR403_DCCR:		return ppc.dccr;
			case SPR403_PIT:		return ppc.pit;
			case SPR403_DBSR:		return ppc.dbsr;
			case SPR403_SGR:		return ppc.sgr;
			case SPR403_TSR:		return ppc.tsr;
			case SPR403_PBL1:		return ppc.pbl1;
			case SPR403_PBU1:		return ppc.pbu1;
			case SPR403_PBL2:		return ppc.pbl2;
			case SPR403_PBU2:		return ppc.pbu2;
		}
	}
#endif

#if (HAS_PPC602)
	if (ppc.is602) {
		switch(spr)
		{
			case SPR602_LT:			return ppc.lt;
			case SPR602_IBR:		return ppc.ibr;
			case SPR602_SP:			return ppc.sp;
			case SPR602_TCR:		return ppc.tcr;
		}
	}
#endif

#if (HAS_PPC603 || HAS_PPC602)
	if (ppc.is603 || ppc.is602)
	{
		switch (spr)
		{
			case SPR603E_TBL_R:
				osd_die("ppc: get_spr: TBL_R \n");
				break;

			case SPR603E_TBU_R:
				osd_die("ppc: get_spr: TBU_R \n");
				break;

			case SPR603E_TBL_W:		return (UINT32)(ppc_read_timebase());
			case SPR603E_TBU_W:		return (UINT32)(ppc_read_timebase() >> 32);
			case SPR603E_HID0:		return ppc.hid0;
			case SPR603E_HID1:		return ppc.hid1;
			case SPR603E_DEC:		return read_decrementer();
			case SPR603E_SDR1:		return ppc.sdr1;
			case SPR603E_DSISR:		return ppc.dsisr;
			case SPR603E_DAR:		return ppc.dar;
			case SPR603E_EAR:		return ppc.ear;
			case SPR603E_IBAT0L:	return ppc.ibat[0].l;
			case SPR603E_IBAT0U:	return ppc.ibat[0].u;
			case SPR603E_IBAT1L:	return ppc.ibat[1].l;
			case SPR603E_IBAT1U:	return ppc.ibat[1].u;
			case SPR603E_IBAT2L:	return ppc.ibat[2].l;
			case SPR603E_IBAT2U:	return ppc.ibat[2].u;
			case SPR603E_IBAT3L:	return ppc.ibat[3].l;
			case SPR603E_IBAT3U:	return ppc.ibat[3].u;
			case SPR603E_DBAT0L:	return ppc.dbat[0].l;
			case SPR603E_DBAT0U:	return ppc.dbat[0].u;
			case SPR603E_DBAT1L:	return ppc.dbat[1].l;
			case SPR603E_DBAT1U:	return ppc.dbat[1].u;
			case SPR603E_DBAT2L:	return ppc.dbat[2].l;
			case SPR603E_DBAT2U:	return ppc.dbat[2].u;
			case SPR603E_DBAT3L:	return ppc.dbat[3].l;
			case SPR603E_DBAT3U:	return ppc.dbat[3].u;
		}
	}
#endif

	osd_die("ppc: get_spr: unknown spr %d (%03X) !\n", spr, spr);
	return 0;
}

static UINT8 ppc_read8_translated(offs_t address);
static UINT16 ppc_read16_translated(offs_t address);
static UINT32 ppc_read32_translated(offs_t address);
static UINT64 ppc_read64_translated(offs_t address);
static void ppc_write8_translated(offs_t address, UINT8 data);
static void ppc_write16_translated(offs_t address, UINT16 data);
static void ppc_write32_translated(offs_t address, UINT32 data);
static void ppc_write64_translated(offs_t address, UINT64 data);

INLINE void ppc_set_msr(UINT32 value)
{
	if( value & (MSR_ILE | MSR_LE) )
		osd_die("ppc: set_msr: little_endian mode not supported !\n");

	MSR = value;

	if (ppc.is603 || ppc.is602)
	{
		if (!(MSR & MSR_DR))
		{
			ppc.read8 = program_read_byte_64be;
			ppc.read16 = program_read_word_64be;
			ppc.read32 = program_read_dword_64be;
			ppc.read64 = program_read_qword_64be;
			ppc.write8 = program_write_byte_64be;
			ppc.write16 = program_write_word_64be;
			ppc.write32 = program_write_dword_64be;
			ppc.write64 = program_write_qword_64be;
		}
		else
		{
			ppc.read8 = ppc_read8_translated;
			ppc.read16 = ppc_read16_translated;
			ppc.read32 = ppc_read32_translated;
			ppc.read64 = ppc_read64_translated;
			ppc.write8 = ppc_write8_translated;
			ppc.write16 = ppc_write16_translated;
			ppc.write32 = ppc_write32_translated;
			ppc.write64 = ppc_write64_translated;
		}
	}
}

INLINE UINT32 ppc_get_msr(void)
{
	return MSR;
}

INLINE void ppc_set_cr(UINT32 value)
{
	CR(0) = (value >> 28) & 0xf;
	CR(1) = (value >> 24) & 0xf;
	CR(2) = (value >> 20) & 0xf;
	CR(3) = (value >> 16) & 0xf;
	CR(4) = (value >> 12) & 0xf;
	CR(5) = (value >> 8) & 0xf;
	CR(6) = (value >> 4) & 0xf;
	CR(7) = (value >> 0) & 0xf;
}

INLINE UINT32 ppc_get_cr(void)
{
	return CR(0) << 28 | CR(1) << 24 | CR(2) << 20 | CR(3) << 16 | CR(4) << 12 | CR(5) << 8 | CR(6) << 4 | CR(7);
}

static void log_code(drc_core *drc)
{
#if LOG_CODE
	FILE *temp;
	temp = fopen("code.bin", "wb");
	fwrite(drc->cache_base, 1, drc->cache_top - drc->cache_base, temp);
	fclose(temp);
#endif
}
/***********************************************************************/

static UINT32 (* optable19[1024])(drc_core *, UINT32);
static UINT32 (* optable31[1024])(drc_core *, UINT32);
static UINT32 (* optable59[1024])(drc_core *, UINT32);
static UINT32 (* optable63[1024])(drc_core *, UINT32);
static UINT32 (* optable[64])(drc_core *, UINT32);

#include "ppc_mem.c"

#if (HAS_PPC403)
#include "ppc403.c"
#endif

/********************************************************************/

#include "ppc_ops.c"
#include "drc_ops.c"
#include "drc_ops.h"

/* Initialization and shutdown */

void ppc_init(void)
{
	int i,j;

	for( i=0; i < 64; i++ ) {
		optable[i] = recompile_invalid;
	}
	for( i=0; i < 1024; i++ ) {
		optable19[i] = recompile_invalid;
		optable31[i] = recompile_invalid;
		optable59[i] = recompile_invalid;
		optable63[i] = recompile_invalid;
	}

	/* Fill the opcode tables */
	for( i=0; i < (sizeof(ppcdrc_opcode_common) / sizeof(PPC_OPCODE)); i++ ) {

		switch(ppcdrc_opcode_common[i].code)
		{
			case 19:
				optable19[ppcdrc_opcode_common[i].subcode] = ppcdrc_opcode_common[i].handler;
				break;

			case 31:
				optable31[ppcdrc_opcode_common[i].subcode] = ppcdrc_opcode_common[i].handler;
				break;

			case 59:
			case 63:
				break;

			default:
				optable[ppcdrc_opcode_common[i].code] = ppcdrc_opcode_common[i].handler;
		}

	}

	/* Calculate rotate mask table */
	for( i=0; i < 32; i++ ) {
		for( j=0; j < 32; j++ ) {
			UINT32 mask;
			int mb = i;
			int me = j;
			mask = ((UINT32)0xFFFFFFFF >> mb) ^ ((me >= 31) ? 0 : ((UINT32)0xFFFFFFFF >> (me + 1)));
			if( mb > me )
				mask = ~mask;

			ppc_rotate_mask[i][j] = mask;
		}
	}
}

#if (HAS_PPC403)
static void ppcdrc403_init(void)
{
	ppc_init();
	ppcdrc_init();

	/* PPC403 specific opcodes */
	optable31[454] = recompile_dccci;
	optable31[486] = recompile_dcread;
	optable31[262] = recompile_icbt;
	optable31[966] = recompile_iccci;
	optable31[998] = recompile_icread;
	optable31[323] = recompile_mfdcr;
	optable31[451] = recompile_mtdcr;
	optable31[131] = recompile_wrtee;
	optable31[163] = recompile_wrteei;
	optable19[51] = recompile_rfci;

	ppc.spu.rx_timer = timer_alloc(ppc403_spu_rx_callback);
	ppc.spu.tx_timer = timer_alloc(ppc403_spu_tx_callback);

	ppc.is603 = 0;
	ppc.is602 = 0;

	ppc.read8 = ppc403_read8;
	ppc.read16 = ppc403_read16;
	ppc.read32 = ppc403_read32;
	ppc.write8 = ppc403_write8;
	ppc.write16 = ppc403_write16;
	ppc.write32 = ppc403_write32;
	ppc.read16_unaligned = ppc403_read16_unaligned;
	ppc.read32_unaligned = ppc403_read32_unaligned;
	ppc.write16_unaligned = ppc403_write16_unaligned;
	ppc.write32_unaligned = ppc403_write32_unaligned;
}

static void ppcdrc403_exit(void)
{
#if LOG_CODE
	//if (symfile) fclose(symfile);
#endif
	drc_exit(ppc.drc);
}

static void ppcdrc403_reset(void *param)
{
	ppc_config *config = param;
	ppc.pc = ppc.npc = 0xfffffffc;
	ppc.pvr = config->pvr;

	ppc_set_msr(0);
	change_pc(ppc.pc);

	/* reset the DRC */
	drc_cache_reset(ppc.drc);
}

static int ppcdrc403_execute(int cycles)
{
	/* count cycles and interrupt cycles */
	ppc_icount = cycles;
	ppc_tb_base_icount = cycles;

	drc_execute(ppc.drc);

	// update timebase
	ppc.tb += (ppc_tb_base_icount - ppc_icount);

	return cycles - ppc_icount;
}

static void ppcdrc403_set_irq_line(int irqline, int state)
{
	if (irqline >= INPUT_LINE_IRQ0 && irqline <= INPUT_LINE_IRQ4)
	{
		UINT32 mask = (1 << (4 - irqline));
		if( state == ASSERT_LINE) {
			if( EXIER & mask ) {
				ppc.external_int = mask;
				ppc.exception_pending |= 0x1;

				if (ppc.irq_callback)
				{
					ppc.irq_callback(irqline);
				}
			}
		}
		// clear line is used to clear the interrupt when the interrupts are level-sensitive
		else if (state == CLEAR_LINE)
		{
			ppc.exisr &= ~mask;
		}
	}
	else if (irqline == PPC403_SPU_RX)
	{
		UINT32 mask = 0x08000000;
		if (state) {
			if( EXIER & mask ) {
				ppc.external_int = mask;
				ppc.exception_pending |= 0x1;
			}
		}
	}
	else if (irqline == PPC403_SPU_TX)
	{
		UINT32 mask = 0x04000000;
		if (state) {
			if( EXIER & mask ) {
				ppc.external_int = mask;
				ppc.exception_pending |= 0x1;
			}
		}
	}
	else
	{
		osd_die("PPC: Unknown IRQ line %d\n", irqline);
	}
}
#endif

#if (HAS_PPC603)
static void ppcdrc603_init(void)
{
	int i;
	ppc_init();
	ppcdrc_init();

	optable[48] = recompile_lfs;
	optable[49] = recompile_lfsu;
	optable[50] = recompile_lfd;
	optable[51] = recompile_lfdu;
	optable[52] = recompile_stfs;
	optable[53] = recompile_stfsu;
	optable[54] = recompile_stfd;
	optable[55] = recompile_stfdu;
	optable31[631] = recompile_lfdux;
	optable31[599] = recompile_lfdx;
	optable31[567] = recompile_lfsux;
	optable31[535] = recompile_lfsx;
	optable31[595] = recompile_mfsr;
	optable31[659] = recompile_mfsrin;
	optable31[371] = recompile_mftb;
	optable31[210] = recompile_mtsr;
	optable31[242] = recompile_mtsrin;
	optable31[758] = recompile_dcba;
	optable31[759] = recompile_stfdux;
	optable31[727] = recompile_stfdx;
	optable31[983] = recompile_stfiwx;
	optable31[695] = recompile_stfsux;
	optable31[663] = recompile_stfsx;
	optable31[370] = recompile_tlbia;
	optable31[306] = recompile_tlbie;
	optable31[566] = recompile_tlbsync;
	optable31[310] = recompile_eciwx;
	optable31[438] = recompile_ecowx;

	optable63[264] = recompile_fabsx;
	optable63[21] = recompile_faddx;
	optable63[32] = recompile_fcmpo;
	optable63[0] = recompile_fcmpu;
	optable63[14] = recompile_fctiwx;
	optable63[15] = recompile_fctiwzx;
	optable63[18] = recompile_fdivx;
	optable63[72] = recompile_fmrx;
	optable63[136] = recompile_fnabsx;
	optable63[40] = recompile_fnegx;
	optable63[12] = recompile_frspx;
	optable63[26] = recompile_frsqrtex;
	optable63[22] = recompile_fsqrtx;
	optable63[20] = recompile_fsubx;
	optable63[583] = recompile_mffsx;
	optable63[70] = recompile_mtfsb0x;
	optable63[38] = recompile_mtfsb1x;
	optable63[711] = recompile_mtfsfx;
	optable63[134] = recompile_mtfsfix;
	optable63[64] = recompile_mcrfs;

	optable59[21] = recompile_faddsx;
	optable59[18] = recompile_fdivsx;
	optable59[24] = recompile_fresx;
	optable59[22] = recompile_fsqrtsx;
	optable59[20] = recompile_fsubsx;

	for(i = 0; i < 32; i++)
	{
		optable63[i * 32 | 29] = recompile_fmaddx;
		optable63[i * 32 | 28] = recompile_fmsubx;
		optable63[i * 32 | 25] = recompile_fmulx;
		optable63[i * 32 | 31] = recompile_fnmaddx;
		optable63[i * 32 | 30] = recompile_fnmsubx;
		optable63[i * 32 | 23] = recompile_fselx;

		optable59[i * 32 | 29] = recompile_fmaddsx;
		optable59[i * 32 | 28] = recompile_fmsubsx;
		optable59[i * 32 | 25] = recompile_fmulsx;
		optable59[i * 32 | 31] = recompile_fnmaddsx;
		optable59[i * 32 | 30] = recompile_fnmsubsx;
	}

	for(i = 0; i < 256; i++)
	{
		ppc_field_xlat[i] =
			((i & 0x80) ? 0xF0000000 : 0) |
			((i & 0x40) ? 0x0F000000 : 0) |
			((i & 0x20) ? 0x00F00000 : 0) |
			((i & 0x10) ? 0x000F0000 : 0) |
			((i & 0x08) ? 0x0000F000 : 0) |
			((i & 0x04) ? 0x00000F00 : 0) |
			((i & 0x02) ? 0x000000F0 : 0) |
			((i & 0x01) ? 0x0000000F : 0);
	}

	ppc.is603 = 1;

	ppc.read8 = program_read_byte_64be;
	ppc.read16 = program_read_word_64be;
	ppc.read32 = program_read_dword_64be;
	ppc.read64 = program_read_qword_64be;
	ppc.write8 = program_write_byte_64be;
	ppc.write16 = program_write_word_64be;
	ppc.write32 = program_write_dword_64be;
	ppc.write64 = program_write_qword_64be;
	ppc.read16_unaligned = ppc_read16_unaligned;
	ppc.read32_unaligned = ppc_read32_unaligned;
	ppc.read64_unaligned = ppc_read64_unaligned;
	ppc.write16_unaligned = ppc_write16_unaligned;
	ppc.write32_unaligned = ppc_write32_unaligned;
	ppc.write64_unaligned = ppc_write64_unaligned;
}

static void ppcdrc603_exit(void)
{
#if LOG_CODE
	//if (symfile) fclose(symfile);
#endif
	drc_exit(ppc.drc);
}

static void ppcdrc603_reset(void *param)
{
	int pll_config = 0;
	float multiplier;
	ppc_config *config = param;
	ppc.pc = ppc.npc = 0xfff00100;
	ppc.pvr = config->pvr;

	multiplier = (float)((config->bus_frequency_multiplier >> 4) & 0xf) +
				 (float)(config->bus_frequency_multiplier & 0xf) / 10.0f;
	bus_freq_multiplier = (int)(multiplier * 2);

	switch(config->pvr)
	{
		case PPC_MODEL_603E:	pll_config = mpc603e_pll_config[bus_freq_multiplier-1][config->bus_frequency]; break;
		case PPC_MODEL_603EV:	pll_config = mpc603ev_pll_config[bus_freq_multiplier-1][config->bus_frequency]; break;
		case PPC_MODEL_603R:	pll_config = mpc603r_pll_config[bus_freq_multiplier-1][config->bus_frequency]; break;
		default: break;
	}

	if (pll_config == -1)
	{
		osd_die("PPC: Invalid bus/multiplier combination (bus frequency = %d, multiplier = %1.1f)", config->bus_frequency, multiplier);
	}

	ppc.hid1 = pll_config << 28;

	ppc_set_msr(0x40);
	change_pc(ppc.pc);

	/* reset the DRC */
	drc_cache_reset(ppc.drc);
}

static int ppcdrc603_execute(int cycles)
{
	/* count cycles and interrupt cycles */
	ppc_icount = cycles;
	ppc_tb_base_icount = cycles;
	ppc_dec_base_icount = cycles + ppc.dec_frac;

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - ppc_icount) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = ppc_icount - DEC;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;
	}

	drc_execute(ppc.drc);

	// update timebase
	// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
	ppc.tb += ((ppc_tb_base_icount - ppc_icount) / 4);

	// update decrementer
	ppc.dec_frac = ((ppc_dec_base_icount - ppc_icount) % (bus_freq_multiplier * 2));
	DEC -= ((ppc_dec_base_icount - ppc_icount) / (bus_freq_multiplier * 2));

	return cycles - ppc_icount;
}

static void ppcdrc603_set_irq_line(int irqline, int state)
{
	if( state ) {
		ppc.exception_pending |= 0x1;
		if (ppc.irq_callback)
		{
			ppc.irq_callback(irqline);
		}
	}
}
#endif

#if (HAS_PPC602)
static void ppcdrc602_init(void)
{
	int i;
	ppc_init();
	ppcdrc_init();

	optable[48] = recompile_lfs;
	optable[49] = recompile_lfsu;
	optable[50] = recompile_lfd;
	optable[51] = recompile_lfdu;
	optable[52] = recompile_stfs;
	optable[53] = recompile_stfsu;
	optable[54] = recompile_stfd;
	optable[55] = recompile_stfdu;
	optable31[631] = recompile_lfdux;
	optable31[599] = recompile_lfdx;
	optable31[567] = recompile_lfsux;
	optable31[535] = recompile_lfsx;
	optable31[595] = recompile_mfsr;
	optable31[659] = recompile_mfsrin;
	optable31[371] = recompile_mftb;
	optable31[210] = recompile_mtsr;
	optable31[242] = recompile_mtsrin;
	optable31[758] = recompile_dcba;
	optable31[759] = recompile_stfdux;
	optable31[727] = recompile_stfdx;
	optable31[983] = recompile_stfiwx;
	optable31[695] = recompile_stfsux;
	optable31[663] = recompile_stfsx;
	optable31[370] = recompile_tlbia;
	optable31[306] = recompile_tlbie;
	optable31[566] = recompile_tlbsync;
	optable31[310] = recompile_eciwx;
	optable31[438] = recompile_ecowx;

	optable63[264] = recompile_fabsx;
	optable63[21] = recompile_faddx;
	optable63[32] = recompile_fcmpo;
	optable63[0] = recompile_fcmpu;
	optable63[14] = recompile_fctiwx;
	optable63[15] = recompile_fctiwzx;
	optable63[18] = recompile_fdivx;
	optable63[72] = recompile_fmrx;
	optable63[136] = recompile_fnabsx;
	optable63[40] = recompile_fnegx;
	optable63[12] = recompile_frspx;
	optable63[26] = recompile_frsqrtex;
	optable63[22] = recompile_fsqrtx;
	optable63[20] = recompile_fsubx;
	optable63[583] = recompile_mffsx;
	optable63[70] = recompile_mtfsb0x;
	optable63[38] = recompile_mtfsb1x;
	optable63[711] = recompile_mtfsfx;
	optable63[134] = recompile_mtfsfix;
	optable63[64] = recompile_mcrfs;

	optable59[21] = recompile_faddsx;
	optable59[18] = recompile_fdivsx;
	optable59[24] = recompile_fresx;
	optable59[22] = recompile_fsqrtsx;
	optable59[20] = recompile_fsubsx;

	for(i = 0; i < 32; i++)
	{
		optable63[i * 32 | 29] = recompile_fmaddx;
		optable63[i * 32 | 28] = recompile_fmsubx;
		optable63[i * 32 | 25] = recompile_fmulx;
		optable63[i * 32 | 31] = recompile_fnmaddx;
		optable63[i * 32 | 30] = recompile_fnmsubx;
		optable63[i * 32 | 23] = recompile_fselx;

		optable59[i * 32 | 29] = recompile_fmaddsx;
		optable59[i * 32 | 28] = recompile_fmsubsx;
		optable59[i * 32 | 25] = recompile_fmulsx;
		optable59[i * 32 | 31] = recompile_fnmaddsx;
		optable59[i * 32 | 30] = recompile_fnmsubsx;
	}

	for(i = 0; i < 256; i++)
	{
		ppc_field_xlat[i] =
			((i & 0x80) ? 0xF0000000 : 0) |
			((i & 0x40) ? 0x0F000000 : 0) |
			((i & 0x20) ? 0x00F00000 : 0) |
			((i & 0x10) ? 0x000F0000 : 0) |
			((i & 0x08) ? 0x0000F000 : 0) |
			((i & 0x04) ? 0x00000F00 : 0) |
			((i & 0x02) ? 0x000000F0 : 0) |
			((i & 0x01) ? 0x0000000F : 0);
	}

	ppc.is603 = 0;
	ppc.is602 = 1;

	ppc.read8 = program_read_byte_64be;
	ppc.read16 = program_read_word_64be;
	ppc.read32 = program_read_dword_64be;
	ppc.read64 = program_read_qword_64be;
	ppc.write8 = program_write_byte_64be;
	ppc.write16 = program_write_word_64be;
	ppc.write32 = program_write_dword_64be;
	ppc.write64 = program_write_qword_64be;
	ppc.read16_unaligned = ppc_read16_unaligned;
	ppc.read32_unaligned = ppc_read32_unaligned;
	ppc.read64_unaligned = ppc_read64_unaligned;
	ppc.write16_unaligned = ppc_write16_unaligned;
	ppc.write32_unaligned = ppc_write32_unaligned;
	ppc.write64_unaligned = ppc_write64_unaligned;
}

static void ppcdrc602_exit(void)
{
#if LOG_CODE
	//if (symfile) fclose(symfile);
#endif
	drc_exit(ppc.drc);
}

static void ppcdrc602_reset(void *param)
{
	float multiplier;
	ppc_config *config = param;
	ppc.pc = ppc.npc = 0xfff00100;
	ppc.pvr = config->pvr;

	multiplier = (float)((config->bus_frequency_multiplier >> 4) & 0xf) +
				 (float)(config->bus_frequency_multiplier & 0xf) / 10.0f;
	bus_freq_multiplier = (int)(multiplier * 2);

	ppc_set_msr(0);
	change_pc(ppc.pc);

	ppc.hid0 = 1;

	/* reset the DRC */
	drc_cache_reset(ppc.drc);
}

static int ppcdrc602_execute(int cycles)
{
	/* count cycles and interrupt cycles */
	ppc_icount = cycles;
	ppc_tb_base_icount = cycles;
	ppc_dec_base_icount = cycles;

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - ppc_icount) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = ppc_icount - DEC;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;
	}

	drc_execute(ppc.drc);

	// update timebase
	// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
	ppc.tb += ((ppc_tb_base_icount - ppc_icount) / 4);

	// update decrementer
	DEC -= ((ppc_dec_base_icount - ppc_icount) / (bus_freq_multiplier * 2));

	return cycles - ppc_icount;
}

static void ppcdrc602_set_irq_line(int irqline, int state)
{
	if( state ) {
		ppc.exception_pending |= 0x1;
		if (ppc.irq_callback)
		{
			ppc.irq_callback(irqline);
		}
	}
}
#endif

static void ppc_get_context(void *dst)
{
	/* copy the context */
	if (dst)
		*(PPC_REGS *)dst = ppc;
}


static void ppc_set_context(void *src)
{
	/* copy the context */
	if (src)
		ppc = *(PPC_REGS *)src;

	change_pc(ppc.pc);
}

/*******************************************************************/

/* Debugger definitions */

static UINT8 ppc_reg_layout[] =
{
	PPC_PC,			PPC_MSR,		-1,
	PPC_CR,			PPC_LR,			-1,
	PPC_CTR,		PPC_XER,		-1,
	PPC_SRR0,		PPC_SRR1,		-1,
	PPC_R0,		 	PPC_R16,		-1,
	PPC_R1, 		PPC_R17,		-1,
	PPC_R2, 		PPC_R18,		-1,
	PPC_R3, 		PPC_R19,		-1,
	PPC_R4, 		PPC_R20,		-1,
	PPC_R5, 		PPC_R21,		-1,
	PPC_R6, 		PPC_R22,		-1,
	PPC_R7, 		PPC_R23,		-1,
	PPC_R8,			PPC_R24,		-1,
	PPC_R9,			PPC_R25,		-1,
	PPC_R10,		PPC_R26,		-1,
	PPC_R11,		PPC_R27,		-1,
	PPC_R12,		PPC_R28,		-1,
	PPC_R13,		PPC_R29,		-1,
	PPC_R14,		PPC_R30,		-1,
	PPC_R15,		PPC_R31,		0
};

static UINT8 ppc_win_layout[] =
{
	 0, 0,34,33,	/* register window (top rows) */
	35, 0,45,14,	/* disassembler window (left colums) */
	35,15,45, 3,	/* memory #2 window (right, lower middle) */
	35,19,45, 3,	/* memory #1 window (right, upper middle) */
	 0,23,80, 1,	/* command line window (bottom rows) */
};

static UINT8 ppc603_reg_layout[] =
{
	PPC_PC,			PPC_MSR,		-1,
	PPC_CR,			PPC_LR,			-1,
	PPC_CTR,		PPC_XER,		-1,
	PPC_SRR0,		PPC_SRR1,		-1,
	PPC_DEC,						-1,
	PPC_R0,		 	PPC_R16,		-1,
	PPC_R1, 		PPC_R17,		-1,
	PPC_R2, 		PPC_R18,		-1,
	PPC_R3, 		PPC_R19,		-1,
	PPC_R4, 		PPC_R20,		-1,
	PPC_R5, 		PPC_R21,		-1,
	PPC_R6, 		PPC_R22,		-1,
	PPC_R7, 		PPC_R23,		-1,
	PPC_R8,			PPC_R24,		-1,
	PPC_R9,			PPC_R25,		-1,
	PPC_R10,		PPC_R26,		-1,
	PPC_R11,		PPC_R27,		-1,
	PPC_R12,		PPC_R28,		-1,
	PPC_R13,		PPC_R29,		-1,
	PPC_R14,		PPC_R30,		-1,
	PPC_R15,		PPC_R31,		0
};

/**************************************************************************
 * Generic set_info
 **************************************************************************/

static void ppc_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		case CPUINFO_INT_PC:
		case CPUINFO_INT_REGISTER + PPC_PC:				ppc.pc = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_MSR:			ppc_set_msr(info->i);				 	break;
		case CPUINFO_INT_REGISTER + PPC_CR:				ppc_set_cr(info->i);			 		break;
		case CPUINFO_INT_REGISTER + PPC_LR:				LR = info->i;							break;
		case CPUINFO_INT_REGISTER + PPC_CTR:			CTR = info->i;							break;
		case CPUINFO_INT_REGISTER + PPC_XER:			XER = info->i;						 	break;
		case CPUINFO_INT_REGISTER + PPC_SRR0:			SRR0 = info->i;							break;
		case CPUINFO_INT_REGISTER + PPC_SRR1:			SRR1 = info->i;							break;

		case CPUINFO_INT_REGISTER + PPC_R0:				ppc.r[0] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R1:				ppc.r[1] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R2:				ppc.r[2] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R3:				ppc.r[3] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R4:				ppc.r[4] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R5:				ppc.r[5] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R6:				ppc.r[6] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R7:				ppc.r[7] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R8:				ppc.r[8] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R9:				ppc.r[9] = info->i;						break;
		case CPUINFO_INT_REGISTER + PPC_R10:			ppc.r[10] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R11:			ppc.r[11] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R12:			ppc.r[12] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R13:			ppc.r[13] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R14:			ppc.r[14] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R15:			ppc.r[15] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R16:			ppc.r[16] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R17:			ppc.r[17] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R18:			ppc.r[18] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R19:			ppc.r[19] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R20:			ppc.r[20] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R21:			ppc.r[21] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R22:			ppc.r[22] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R23:			ppc.r[23] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R24:			ppc.r[24] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R25:			ppc.r[25] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R26:			ppc.r[26] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R27:			ppc.r[27] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R28:			ppc.r[28] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R29:			ppc.r[29] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R30:			ppc.r[30] = info->i;					break;
		case CPUINFO_INT_REGISTER + PPC_R31:			ppc.r[31] = info->i;					break;

		/* --- the following bits of info are set as pointers to data or functions --- */
		case CPUINFO_PTR_IRQ_CALLBACK:					ppc.irq_callback = info->irqcallback;	break;
	}
}

void ppc_get_info(UINT32 state, union cpuinfo *info)
{
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_CONTEXT_SIZE:					info->i = sizeof(ppc);					break;
		case CPUINFO_INT_INPUT_LINES:					info->i = 1;							break;
		case CPUINFO_INT_DEFAULT_IRQ_VECTOR:			info->i = 0;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;					break;
		case CPUINFO_INT_CLOCK_DIVIDER:					info->i = 1;							break;
		case CPUINFO_INT_MIN_INSTRUCTION_BYTES:			info->i = 4;							break;
		case CPUINFO_INT_MAX_INSTRUCTION_BYTES:			info->i = 4;							break;
		case CPUINFO_INT_MIN_CYCLES:					info->i = 1;							break;
		case CPUINFO_INT_MAX_CYCLES:					info->i = 40;							break;

		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_DATA:	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_IO: 		info->i = 0;					break;

		case CPUINFO_INT_INPUT_STATE:			info->i = CLEAR_LINE;	break;

		case CPUINFO_INT_PREVIOUSPC:					/* not implemented */					break;

		case CPUINFO_INT_PC:	/* intentional fallthrough */
		case CPUINFO_INT_REGISTER + PPC_PC:				info->i = ppc.pc;						break;
		case CPUINFO_INT_REGISTER + PPC_MSR:			info->i = ppc_get_msr();				break;
		case CPUINFO_INT_REGISTER + PPC_CR:				info->i = ppc_get_cr();					break;
		case CPUINFO_INT_REGISTER + PPC_LR:				info->i = LR;							break;
		case CPUINFO_INT_REGISTER + PPC_CTR:			info->i = CTR;							break;
		case CPUINFO_INT_REGISTER + PPC_XER:			info->i = XER;							break;
		case CPUINFO_INT_REGISTER + PPC_SRR0:			info->i = SRR0;							break;
		case CPUINFO_INT_REGISTER + PPC_SRR1:			info->i = SRR1;							break;

		case CPUINFO_INT_REGISTER + PPC_R0:				info->i = ppc.r[0];						break;
		case CPUINFO_INT_REGISTER + PPC_R1:				info->i = ppc.r[1];						break;
		case CPUINFO_INT_REGISTER + PPC_R2:				info->i = ppc.r[2];						break;
		case CPUINFO_INT_REGISTER + PPC_R3:				info->i = ppc.r[3];						break;
		case CPUINFO_INT_REGISTER + PPC_R4:				info->i = ppc.r[4];						break;
		case CPUINFO_INT_REGISTER + PPC_R5:				info->i = ppc.r[5];						break;
		case CPUINFO_INT_REGISTER + PPC_R6:				info->i = ppc.r[6];						break;
		case CPUINFO_INT_REGISTER + PPC_R7:				info->i = ppc.r[7];						break;
		case CPUINFO_INT_REGISTER + PPC_R8:				info->i = ppc.r[8];						break;
		case CPUINFO_INT_REGISTER + PPC_R9:				info->i = ppc.r[9];						break;
		case CPUINFO_INT_REGISTER + PPC_R10:			info->i = ppc.r[10];					break;
		case CPUINFO_INT_REGISTER + PPC_R11:			info->i = ppc.r[11];					break;
		case CPUINFO_INT_REGISTER + PPC_R12:			info->i = ppc.r[12];					break;
		case CPUINFO_INT_REGISTER + PPC_R13:			info->i = ppc.r[13];					break;
		case CPUINFO_INT_REGISTER + PPC_R14:			info->i = ppc.r[14];					break;
		case CPUINFO_INT_REGISTER + PPC_R15:			info->i = ppc.r[15];					break;
		case CPUINFO_INT_REGISTER + PPC_R16:			info->i = ppc.r[16];					break;
		case CPUINFO_INT_REGISTER + PPC_R17:			info->i = ppc.r[17];					break;
		case CPUINFO_INT_REGISTER + PPC_R18:			info->i = ppc.r[18];					break;
		case CPUINFO_INT_REGISTER + PPC_R19:			info->i = ppc.r[19];					break;
		case CPUINFO_INT_REGISTER + PPC_R20:			info->i = ppc.r[20];					break;
		case CPUINFO_INT_REGISTER + PPC_R21:			info->i = ppc.r[21];					break;
		case CPUINFO_INT_REGISTER + PPC_R22:			info->i = ppc.r[22];					break;
		case CPUINFO_INT_REGISTER + PPC_R23:			info->i = ppc.r[23];					break;
		case CPUINFO_INT_REGISTER + PPC_R24:			info->i = ppc.r[24];					break;
		case CPUINFO_INT_REGISTER + PPC_R25:			info->i = ppc.r[25];					break;
		case CPUINFO_INT_REGISTER + PPC_R26:			info->i = ppc.r[26];					break;
		case CPUINFO_INT_REGISTER + PPC_R27:			info->i = ppc.r[27];					break;
		case CPUINFO_INT_REGISTER + PPC_R28:			info->i = ppc.r[28];					break;
		case CPUINFO_INT_REGISTER + PPC_R29:			info->i = ppc.r[29];					break;
		case CPUINFO_INT_REGISTER + PPC_R30:			info->i = ppc.r[30];					break;
		case CPUINFO_INT_REGISTER + PPC_R31:			info->i = ppc.r[31];					break;



		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_GET_CONTEXT:					info->getcontext = ppc_get_context;		break;
		case CPUINFO_PTR_SET_CONTEXT:					info->setcontext = ppc_set_context;		break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = ppc_dasm;			break;
		case CPUINFO_PTR_IRQ_CALLBACK:					info->irqcallback = ppc.irq_callback;	break;
		case CPUINFO_PTR_INSTRUCTION_COUNTER:			info->icount = &ppc_icount;				break;
		case CPUINFO_PTR_REGISTER_LAYOUT:				info->p = ppc_reg_layout;				break;
		case CPUINFO_PTR_WINDOW_LAYOUT:					info->p = ppc_win_layout;				break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "PPC403"); break;
		case CPUINFO_STR_CORE_FAMILY:					strcpy(info->s = cpuintrf_temp_str(), "PowerPC"); break;
		case CPUINFO_STR_CORE_VERSION:					strcpy(info->s = cpuintrf_temp_str(), "1.0"); break;
		case CPUINFO_STR_CORE_FILE:						strcpy(info->s = cpuintrf_temp_str(), __FILE__); break;
		case CPUINFO_STR_CORE_CREDITS:					strcpy(info->s = cpuintrf_temp_str(), "Copyright (C) 2004"); break;

		case CPUINFO_STR_FLAGS:							strcpy(info->s = cpuintrf_temp_str(), " "); break;

		case CPUINFO_STR_REGISTER + PPC_PC:				sprintf(info->s = cpuintrf_temp_str(), "PC: %08X", ppc.pc); break;
		case CPUINFO_STR_REGISTER + PPC_MSR:			sprintf(info->s = cpuintrf_temp_str(), "MSR: %08X", ppc_get_msr()); break;
		case CPUINFO_STR_REGISTER + PPC_CR:				sprintf(info->s = cpuintrf_temp_str(), "CR: %08X", ppc_get_cr()); break;
		case CPUINFO_STR_REGISTER + PPC_LR:				sprintf(info->s = cpuintrf_temp_str(), "LR: %08X", LR); break;
		case CPUINFO_STR_REGISTER + PPC_CTR:			sprintf(info->s = cpuintrf_temp_str(), "CTR: %08X", CTR); break;
		case CPUINFO_STR_REGISTER + PPC_XER:			sprintf(info->s = cpuintrf_temp_str(), "XER: %08X", XER); break;
		case CPUINFO_STR_REGISTER + PPC_SRR0:			sprintf(info->s = cpuintrf_temp_str(), "SRR0: %08X", SRR0); break;
		case CPUINFO_STR_REGISTER + PPC_SRR1:			sprintf(info->s = cpuintrf_temp_str(), "SRR1: %08X", SRR1); break;

		case CPUINFO_STR_REGISTER + PPC_R0:				sprintf(info->s = cpuintrf_temp_str(), "R0: %08X", ppc.r[0]); break;
		case CPUINFO_STR_REGISTER + PPC_R1:				sprintf(info->s = cpuintrf_temp_str(), "R1: %08X", ppc.r[1]); break;
		case CPUINFO_STR_REGISTER + PPC_R2:				sprintf(info->s = cpuintrf_temp_str(), "R2: %08X", ppc.r[2]); break;
		case CPUINFO_STR_REGISTER + PPC_R3:				sprintf(info->s = cpuintrf_temp_str(), "R3: %08X", ppc.r[3]); break;
		case CPUINFO_STR_REGISTER + PPC_R4:				sprintf(info->s = cpuintrf_temp_str(), "R4: %08X", ppc.r[4]); break;
		case CPUINFO_STR_REGISTER + PPC_R5:				sprintf(info->s = cpuintrf_temp_str(), "R5: %08X", ppc.r[5]); break;
		case CPUINFO_STR_REGISTER + PPC_R6:				sprintf(info->s = cpuintrf_temp_str(), "R6: %08X", ppc.r[6]); break;
		case CPUINFO_STR_REGISTER + PPC_R7:				sprintf(info->s = cpuintrf_temp_str(), "R7: %08X", ppc.r[7]); break;
		case CPUINFO_STR_REGISTER + PPC_R8:				sprintf(info->s = cpuintrf_temp_str(), "R8: %08X", ppc.r[8]); break;
		case CPUINFO_STR_REGISTER + PPC_R9:				sprintf(info->s = cpuintrf_temp_str(), "R9: %08X", ppc.r[9]); break;
		case CPUINFO_STR_REGISTER + PPC_R10:			sprintf(info->s = cpuintrf_temp_str(), "R10: %08X", ppc.r[10]); break;
		case CPUINFO_STR_REGISTER + PPC_R11:			sprintf(info->s = cpuintrf_temp_str(), "R11: %08X", ppc.r[11]); break;
		case CPUINFO_STR_REGISTER + PPC_R12:			sprintf(info->s = cpuintrf_temp_str(), "R12: %08X", ppc.r[12]); break;
		case CPUINFO_STR_REGISTER + PPC_R13:			sprintf(info->s = cpuintrf_temp_str(), "R13: %08X", ppc.r[13]); break;
		case CPUINFO_STR_REGISTER + PPC_R14:			sprintf(info->s = cpuintrf_temp_str(), "R14: %08X", ppc.r[14]); break;
		case CPUINFO_STR_REGISTER + PPC_R15:			sprintf(info->s = cpuintrf_temp_str(), "R15: %08X", ppc.r[15]); break;
		case CPUINFO_STR_REGISTER + PPC_R16:			sprintf(info->s = cpuintrf_temp_str(), "R16: %08X", ppc.r[16]); break;
		case CPUINFO_STR_REGISTER + PPC_R17:			sprintf(info->s = cpuintrf_temp_str(), "R17: %08X", ppc.r[17]); break;
		case CPUINFO_STR_REGISTER + PPC_R18:			sprintf(info->s = cpuintrf_temp_str(), "R18: %08X", ppc.r[18]); break;
		case CPUINFO_STR_REGISTER + PPC_R19:			sprintf(info->s = cpuintrf_temp_str(), "R19: %08X", ppc.r[19]); break;
		case CPUINFO_STR_REGISTER + PPC_R20:			sprintf(info->s = cpuintrf_temp_str(), "R20: %08X", ppc.r[20]); break;
		case CPUINFO_STR_REGISTER + PPC_R21:			sprintf(info->s = cpuintrf_temp_str(), "R21: %08X", ppc.r[21]); break;
		case CPUINFO_STR_REGISTER + PPC_R22:			sprintf(info->s = cpuintrf_temp_str(), "R22: %08X", ppc.r[22]); break;
		case CPUINFO_STR_REGISTER + PPC_R23:			sprintf(info->s = cpuintrf_temp_str(), "R23: %08X", ppc.r[23]); break;
		case CPUINFO_STR_REGISTER + PPC_R24:			sprintf(info->s = cpuintrf_temp_str(), "R24: %08X", ppc.r[24]); break;
		case CPUINFO_STR_REGISTER + PPC_R25:			sprintf(info->s = cpuintrf_temp_str(), "R25: %08X", ppc.r[25]); break;
		case CPUINFO_STR_REGISTER + PPC_R26:			sprintf(info->s = cpuintrf_temp_str(), "R26: %08X", ppc.r[26]); break;
		case CPUINFO_STR_REGISTER + PPC_R27:			sprintf(info->s = cpuintrf_temp_str(), "R27: %08X", ppc.r[27]); break;
		case CPUINFO_STR_REGISTER + PPC_R28:			sprintf(info->s = cpuintrf_temp_str(), "R28: %08X", ppc.r[28]); break;
		case CPUINFO_STR_REGISTER + PPC_R29:			sprintf(info->s = cpuintrf_temp_str(), "R29: %08X", ppc.r[29]); break;
		case CPUINFO_STR_REGISTER + PPC_R30:			sprintf(info->s = cpuintrf_temp_str(), "R30: %08X", ppc.r[30]); break;
		case CPUINFO_STR_REGISTER + PPC_R31:			sprintf(info->s = cpuintrf_temp_str(), "R31: %08X", ppc.r[31]); break;
	}
}



/* PowerPC 403 */
#if (HAS_PPC403)
void ppc403_set_info(UINT32 state, union cpuinfo *info)
{
	if (state >= CPUINFO_INT_INPUT_STATE && state <= CPUINFO_INT_INPUT_STATE + 5)
	{
		ppcdrc403_set_irq_line(state-CPUINFO_INT_INPUT_STATE, info->i);
		return;
	}
	switch(state)
	{
		default:	ppc_set_info(state, info);		break;
	}
}

void ppc403_get_info(UINT32 state, union cpuinfo *info)
{
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_INPUT_LINES:					info->i = 7;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = ppc403_set_info;		break;
		case CPUINFO_PTR_INIT:							info->init = ppcdrc403_init;			break;
		case CPUINFO_PTR_RESET:							info->reset = ppcdrc403_reset;			break;
		case CPUINFO_PTR_EXIT:							info->exit = ppcdrc403_exit;			break;
		case CPUINFO_PTR_EXECUTE:						info->execute = ppcdrc403_execute;		break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "PPC403"); break;

		default:	ppc_get_info(state, info);		break;
	}
}
#endif

/* PowerPC 603 */
#if (HAS_PPC603)
void ppc603_set_info(UINT32 state, union cpuinfo *info)
{
	if (state >= CPUINFO_INT_INPUT_STATE && state <= CPUINFO_INT_INPUT_STATE + 5)
	{
		ppcdrc603_set_irq_line(state-CPUINFO_INT_INPUT_STATE, info->i);
		return;
	}
	switch(state)
	{
		case CPUINFO_INT_REGISTER + PPC_DEC:				write_decrementer(info->i);		break;
		default:	ppc_set_info(state, info);		break;
	}
}

void ppc603_get_info(UINT32 state, union cpuinfo *info)
{
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_INPUT_LINES:					info->i = 5;				break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;			break;
		case CPUINFO_INT_REGISTER + PPC_DEC:			info->i = read_decrementer(); break;

		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 64;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 32;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:					info->setinfo = ppc603_set_info;		break;
		case CPUINFO_PTR_INIT:						info->init = ppcdrc603_init;				break;
		case CPUINFO_PTR_RESET:						info->reset = ppcdrc603_reset;				break;
		case CPUINFO_PTR_EXIT:						info->exit = ppcdrc603_exit;				break;
		case CPUINFO_PTR_EXECUTE:					info->execute = ppcdrc603_execute;			break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = ppc_dasm64;			break;
		case CPUINFO_PTR_READ:							info->read = ppc_read;					break;
		case CPUINFO_PTR_WRITE:							info->write = ppc_write;				break;
		case CPUINFO_PTR_READOP:						info->readop = ppc_readop;				break;
		case CPUINFO_PTR_REGISTER_LAYOUT:				info->p = ppc603_reg_layout;				break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "PPC603"); break;
		case CPUINFO_STR_REGISTER + PPC_DEC:			sprintf(info->s = cpuintrf_temp_str(), "DEC: %08X", read_decrementer()); break;

		default:	ppc_get_info(state, info);		break;
	}
}
#endif

/* PowerPC 602 */
#if (HAS_PPC602)
void ppc602_set_info(UINT32 state, union cpuinfo *info)
{
	if (state >= CPUINFO_INT_INPUT_STATE && state <= CPUINFO_INT_INPUT_STATE + 5)
	{
		ppcdrc602_set_irq_line(state-CPUINFO_INT_INPUT_STATE, info->i);
		return;
	}
	switch(state)
	{
		default:	ppc_set_info(state, info);		break;
	}
}

void ppc602_get_info(UINT32 state, union cpuinfo *info)
{
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_INPUT_LINES:					info->i = 5;				break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;			break;

		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 64;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 32;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:					info->setinfo = ppc602_set_info;		break;
		case CPUINFO_PTR_INIT:						info->init = ppcdrc602_init;				break;
		case CPUINFO_PTR_RESET:						info->reset = ppcdrc602_reset;				break;
		case CPUINFO_PTR_EXIT:						info->exit = ppcdrc602_exit;				break;
		case CPUINFO_PTR_EXECUTE:					info->execute = ppcdrc602_execute;			break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = ppc_dasm64;			break;
		case CPUINFO_PTR_READ:							info->read = ppc_read;					break;
		case CPUINFO_PTR_WRITE:							info->write = ppc_write;				break;
		case CPUINFO_PTR_READOP:						info->readop = ppc_readop;				break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "PPC602"); break;

		default:	ppc_get_info(state, info);		break;
	}
}
#endif
