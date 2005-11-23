/***************************************************************************

    state.h

    Save state management functions.

***************************************************************************/

#pragma once

#ifndef __STATE_H__
#define __STATE_H__

#include "osd_cpu.h"
#include "fileio.h"

/* Initializes the save state registrations */
void state_save_free(void);
void state_save_allow_registration(int allowed);

/* Registering functions */
int state_save_get_reg_count(void);

void state_save_register_UINT8 (const char *module, int instance,
								const char *name, UINT8 *val, unsigned size);
void state_save_register_INT8  (const char *module, int instance,
								const char *name, INT8 *val, unsigned size);
void state_save_register_UINT16(const char *module, int instance,
								const char *name, UINT16 *val, unsigned size);
void state_save_register_INT16 (const char *module, int instance,
								const char *name, INT16 *val, unsigned size);
void state_save_register_UINT32(const char *module, int instance,
								const char *name, UINT32 *val, unsigned size);
void state_save_register_INT32 (const char *module, int instance,
								const char *name, INT32 *val, unsigned size);
void state_save_register_UINT64(const char *module, int instance,
								const char *name, UINT64 *val, unsigned size);
void state_save_register_INT64 (const char *module, int instance,
								const char *name, INT64 *val, unsigned size);
void state_save_register_double(const char *module, int instance,
								const char *name, double *val, unsigned size);
void state_save_register_float (const char *module, int instance,
								const char *name, float *val, unsigned size);
void state_save_register_int   (const char *module, int instance,
								const char *name, int *val);

void state_save_register_func_presave(void (*func)(void));
void state_save_register_func_postload(void (*func)(void));

void state_save_register_func_presave_int(void (*func)(int), int param);
void state_save_register_func_postload_int(void (*func)(int), int param);

void state_save_register_func_presave_ptr(void (*func)(void *), void *param);
void state_save_register_func_postload_ptr(void (*func)(void *), void *param);

/* Save and load functions */
/* The tags are a hack around the current cpu structures */
int  state_save_save_begin(mame_file *file);
int  state_save_load_begin(mame_file *file);

void state_save_push_tag(int tag);
void state_save_pop_tag(void);

void state_save_save_continue(void);
void state_save_load_continue(void);

void state_save_save_finish(void);
void state_save_load_finish(void);

/* Display function */
void state_save_dump_registry(void);

/* Verification function; can be called from front ends */
int state_save_check_file(mame_file *file, const char *gamename, int validate_signature, void (CLIB_DECL *errormsg)(const char *fmt, ...));

#endif	/* __STATE_H__ */
