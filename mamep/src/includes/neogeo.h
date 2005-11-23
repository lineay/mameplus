/*************************************************************************

    SNK NeoGeo hardware

*************************************************************************/
#ifndef __NEOGEO_H__
#define __NEOGEO_H__

//#define USE_NEOGEO_HACKS


#ifdef USE_NEOGEO_HACKS
#define NEOGEO_BIOS_EURO	0	// Europe MVS (Ver. 2)
#define NEOGEO_BIOS_DEBUG	14	// Debug MVS (Hack?)
#endif /* USE_NEOGEO_HACKS */


/*----------- defined in drivers/neogeo.c -----------*/

extern UINT32 neogeo_frame_counter;
extern UINT32 neogeo_frame_counter_speed;
extern int neogeo_has_trackball;

void neogeo_set_cpu1_second_bank(UINT32 bankaddress);
void neogeo_init_cpu2_setbank(void);
void neogeo_register_main_savestate(void);

/*----------- defined in machine/neogeo.c -----------*/

extern UINT16 *neogeo_ram16;
extern UINT16 *neogeo_sram16;

extern int memcard_status;
extern UINT8 *neogeo_memcard;

extern UINT8 *neogeo_game_vectors;

MACHINE_INIT( neogeo );
DRIVER_INIT( neogeo );

WRITE16_HANDLER( neogeo_sram16_lock_w );
WRITE16_HANDLER( neogeo_sram16_unlock_w );
READ16_HANDLER( neogeo_sram16_r );
WRITE16_HANDLER( neogeo_sram16_w );

NVRAM_HANDLER( neogeo );

READ16_HANDLER( neogeo_memcard16_r );
WRITE16_HANDLER( neogeo_memcard16_w );
int neogeo_memcard_load(int);
void neogeo_memcard_save(void);
void neogeo_memcard_eject(void);
int neogeo_memcard_create(int);


/*----------- defined in machine/neocrypt.c -----------*/

extern int neogeo_fix_bank_type;

void kof99_neogeo_gfx_decrypt(int extra_xor);
void kof2000_neogeo_gfx_decrypt(int extra_xor);
#ifdef EXTRA_GAMES
void neogeo_sfix_decrypt(void);
#endif /* EXTRA_GAMES */

/*----------- defined in vidhrdw/neogeo.c -----------*/

VIDEO_START( neogeo_mvs );

WRITE16_HANDLER( neogeo_setpalbank0_16_w );
WRITE16_HANDLER( neogeo_setpalbank1_16_w );
READ16_HANDLER( neogeo_paletteram16_r );
WRITE16_HANDLER( neogeo_paletteram16_w );

WRITE16_HANDLER( neogeo_vidram16_offset_w );
READ16_HANDLER( neogeo_vidram16_data_r );
WRITE16_HANDLER( neogeo_vidram16_data_w );
WRITE16_HANDLER( neogeo_vidram16_modulo_w );
READ16_HANDLER( neogeo_vidram16_modulo_r );
WRITE16_HANDLER( neo_board_fix_16_w );
WRITE16_HANDLER( neo_game_fix_16_w );
WRITE16_HANDLER (neogeo_select_bios_vectors);
WRITE16_HANDLER (neogeo_select_game_vectors);

VIDEO_UPDATE( neogeo );
VIDEO_UPDATE( neogeo_raster );
void neogeo_vh_raster_partial_refresh(mame_bitmap *bitmap,int current_line);

#endif /* __NEOGEO_H__ */
