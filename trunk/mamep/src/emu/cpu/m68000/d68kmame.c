#include <stdio.h>
#include <stdlib.h>
#include "d68k.h"
#include "m68000.h"

/* global access */

extern m68k_memory_interface m68k_memory_intf;
extern offs_t m68k_encrypted_opcode_start[MAX_CPU];
extern offs_t m68k_encrypted_opcode_end[MAX_CPU];


#if 0 //ks hcmame s switch m68k core
void m68k_set_encrypted_opcode_range(int cpunum, offs_t start, offs_t end)
{
	m68k_encrypted_opcode_start[cpunum] = start;
	m68k_encrypted_opcode_end[cpunum] = end;
}
#endif //ks hcmame e switch m68k core

/****************************************************************************
 * 8-bit data memory interface
 ****************************************************************************/

static UINT16 m68008_read_immediate_16(offs_t address)
{
	offs_t addr = (address) ^ m68k_memory_intf.opcode_xor;
	return (cpu_readop(addr) << 8) | (cpu_readop(addr + 1));
}

/* interface for 20/22-bit address bus, 8-bit data bus (68008) */
static const m68k_memory_interface interface_d8 =
{
	0,
	m68008_read_immediate_16,
	program_read_byte_8be,
	program_read_word_8be,
	program_read_dword_8be,
	program_write_byte_8be,
	program_write_word_8be,
	program_write_dword_8be
};

/****************************************************************************
 * 16-bit data memory interface
 ****************************************************************************/

static UINT16 read_immediate_16(offs_t address)
{
	return cpu_readop16((address) ^ m68k_memory_intf.opcode_xor);
}

/* interface for 24-bit address bus, 16-bit data bus (68000, 68010) */
static const m68k_memory_interface interface_d16 =
{
	0,
	read_immediate_16,
	program_read_byte_16be,
	program_read_word_16be,
	program_read_dword_16be,
	program_write_byte_16be,
	program_write_word_16be,
	program_write_dword_16be
};

/****************************************************************************
 * 32-bit data memory interface
 ****************************************************************************/

/* potentially misaligned 16-bit reads with a 32-bit data bus (and 24-bit address bus) */
static UINT16 readword_d32(offs_t address)
{
	UINT16 result;

	if (!(address & 1))
		return program_read_word_32be(address);
	result = program_read_byte_32be(address) << 8;
	return result | program_read_byte_32be(address + 1);
}

/* potentially misaligned 16-bit writes with a 32-bit data bus (and 24-bit address bus) */
static void writeword_d32(offs_t address, UINT16 data)
{
	if (!(address & 1))
	{
		program_write_word_32be(address, data);
		return;
	}
	program_write_byte_32be(address, data >> 8);
	program_write_byte_32be(address + 1, data);
}

/* potentially misaligned 32-bit reads with a 32-bit data bus (and 24-bit address bus) */
static UINT32 readlong_d32(offs_t address)
{
	UINT32 result;

	if (!(address & 3))
		return program_read_dword_32be(address);
	else if (!(address & 1))
	{
		result = program_read_word_32be(address) << 16;
		return result | program_read_word_32be(address + 2);
	}
	result = program_read_byte_32be(address) << 24;
	result |= program_read_word_32be(address + 1) << 8;
	return result | program_read_byte_32be(address + 3);
}

/* potentially misaligned 32-bit writes with a 32-bit data bus (and 24-bit address bus) */
static void writelong_d32(offs_t address, UINT32 data)
{
	if (!(address & 3))
	{
		program_write_dword_32be(address, data);
		return;
	}
	else if (!(address & 1))
	{
		program_write_word_32be(address, data >> 16);
		program_write_word_32be(address + 2, data);
		return;
	}
	program_write_byte_32be(address, data >> 24);
	program_write_word_32be(address + 1, data >> 8);
	program_write_byte_32be(address + 3, data);
}

/* interface for 32-bit data bus (68EC020, 68020) */
static const m68k_memory_interface interface_d32 =
{
	WORD_XOR_BE(0),
	read_immediate_16,
	program_read_byte_32be,
	readword_d32,
	readlong_d32,
	program_write_byte_32be,
	writeword_d32,
	writelong_d32
};



/****************************************************************************
 * 68000 section
 ****************************************************************************/

static void m68000_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	m68kdrc_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_memory_intf = interface_d16;
	m68k_state_register("m68000", index);
	m68k_set_int_ack_callback(irqcallback);
}

static void m68000_reset(void)
{
	m68kdrc_pulse_reset();
}

static void m68000_exit(void)
{
	m68kdrc_exit();
}

static int m68000_execute(int cycles)
{
	return m68kdrc_execute(cycles);
}


/****************************************************************************
 * M68008 section
 ****************************************************************************/
#if HAS_M68008

static void m68008_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	m68kdrc_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68008);
	m68k_memory_intf = interface_d8;
	m68k_state_register("m68008", index);
	m68k_set_int_ack_callback(irqcallback);
}

#endif /* HAS_M68008 */


/****************************************************************************
 * M68010 section
 ****************************************************************************/
#if HAS_M68010

static void m68010_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	m68kdrc_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68010);
	m68k_memory_intf = interface_d16;
	m68k_state_register("m68008", index);
	m68k_set_int_ack_callback(irqcallback);
}

#endif /* HAS_M68010 */

/****************************************************************************
 * M68020 section
 ****************************************************************************/

static void m68020_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	m68kdrc_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_memory_intf = interface_d32;
	m68k_state_register("m68020", index);
	m68k_set_int_ack_callback(irqcallback);
}


/****************************************************************************
 * M680EC20 section
 ****************************************************************************/
#if HAS_M68EC020

static void m68ec020_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	m68kdrc_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68EC020);
	m68k_memory_intf = interface_d32;
	m68k_state_register("m68ec020", index);
	m68k_set_int_ack_callback(irqcallback);
}

#endif /* HAS_M68EC020 */

/****************************************************************************
 * M68040 section
 ****************************************************************************/
#if HAS_M68040

static void m68040_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	m68kdrc_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68040);
	m68k_memory_intf = interface_d32;
	m68k_state_register("m68040", index);
	m68kfpu_state_register("m68040", index);
	m68k_set_int_ack_callback(irqcallback);
}
#endif


/**************************************************************************
 * Generic get_info
 **************************************************************************/

#ifdef ENABLE_DEBUGGER
extern void m68kdrc_flag_str_mark_dirty(char *str);
#endif

extern void m68000c_get_info(UINT32 state, cpuinfo *info);

void m68000drc_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = m68000_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef ENABLE_DEBUGGER
		case CPUINFO_STR_FLAGS:
			m68000c_get_info(state, info);
			m68kdrc_flag_str_mark_dirty(info->s);
			break;
#endif
		default:
			m68000c_get_info(state, info);
			break;
	}
}

#if (HAS_M68008)
extern void m68008c_get_info(UINT32 state, cpuinfo *info);

void m68008drc_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = m68008_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef ENABLE_DEBUGGER
		case CPUINFO_STR_FLAGS:
			m68008c_get_info(state, info);
			m68kdrc_flag_str_mark_dirty(info->s);
			break;
#endif
		default:
			m68008c_get_info(state, info);
			break;
	}
}

#endif


#if (HAS_M68010)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

extern void m68010c_get_info(UINT32 state, cpuinfo *info);

void m68010drc_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		case CPUINFO_PTR_INIT:							info->init = m68010_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef ENABLE_DEBUGGER
		case CPUINFO_STR_FLAGS:
			m68010c_get_info(state, info);
			m68kdrc_flag_str_mark_dirty(info->s);
			break;
#endif
		default:
			m68010c_get_info(state, info);
			break;
	}
}
#endif

/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

extern void m68020c_get_info(UINT32 state, cpuinfo *info);

void m68020drc_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		case CPUINFO_PTR_INIT:							info->init = m68020_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef ENABLE_DEBUGGER
		case CPUINFO_STR_FLAGS:
			m68020c_get_info(state, info);
			m68kdrc_flag_str_mark_dirty(info->s);
			break;
#endif
		default:
			m68020c_get_info(state, info);
			break;
	}
}


#if (HAS_M68EC020)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

extern void m68ec020c_get_info(UINT32 state, cpuinfo *info);

void m68ec020drc_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		case CPUINFO_PTR_INIT:							info->init = m68ec020_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef ENABLE_DEBUGGER
		case CPUINFO_STR_FLAGS:
			m68ec020c_get_info(state, info);
			m68kdrc_flag_str_mark_dirty(info->s);
			break;
#endif
		default:
			m68ec020c_get_info(state, info);
			break;
	}
}
#endif


#if (HAS_M68040)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

extern void m68040c_get_info(UINT32 state, cpuinfo *info);

void m68040drc_get_info(UINT32 state, cpuinfo *info)
{
	switch (state)
	{
		case CPUINFO_PTR_INIT:							info->init = m68040_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef ENABLE_DEBUGGER
		case CPUINFO_STR_FLAGS:
			m68040c_get_info(state, info);
			m68kdrc_flag_str_mark_dirty(info->s);
			break;
#endif
		default:
			m68040c_get_info(state, info);
			break;
	}
}
#endif
