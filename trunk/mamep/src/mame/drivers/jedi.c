/***************************************************************************

    Atari Return of the Jedi hardware

    driver by Dan Boris

    Games supported:
        * Return of the Jedi

    Known bugs:
        * smoothing PROMs were dumped at half size

****************************************************************************

    Memory map

****************************************************************************

    ========================================================================
    CPU #1
    ========================================================================
    0000-07FF   R/W   xxxxxxxx    Z-page Working RAM
    0800-08FF   R/W   xxxxxxxx    NVRAM
    0C00        R     xxxx-xxx    Switch inputs #1
                R     x-------       (right coin)
                R     -x------       (left coin)
                R     --x-----       (aux coin)
                R     ---x----       (self test)
                R     -----x--       (left thumb switch)
                R     ------x-       (fire switches)
                R     -------x       (right thumb switch)
    0C01        R     xxx--x--    Communications
                R     x-------       (VBLANK)
                R     -x------       (sound CPU communications latch full flag)
                R     --x-----       (sound CPU acknowledge latch flag)
                R     -----x--       (slam switch)
    1400        R     xxxxxxxx    Sound acknowledge latch
    1800        R     xxxxxxxx    Read A/D conversion
    1C00          W   --------    Enable NVRAM
    1C01          W   --------    Disable NVRAM
    1C80          W   --------    Start A/D conversion (horizontal)
    1C82          W   --------    Start A/D conversion (vertical)
    1D00          W   --------    NVRAM store
    1D80          W   --------    Watchdog clear
    1E00          W   --------    Interrupt acknowledge
    1E80          W   x-------    Left coin counter
    1E81          W   x-------    Right coin counter
    1E82          W   x-------    LED 1 (not used)
    1E83          W   x-------    LED 2 (not used)
    1E84          W   x-------    Alphanumerics bank select
    1E86          W   x-------    Sound CPU reset
    1E87          W   x-------    Video off
    1F00          W   xxxxxxxx    Sound communications latch
    1F80          W   -----xxx    Program ROM bank select
    2000-23FF   R/W   xxxxxxxx    Scrolling playfield (low 8 bits)
    2400-27FF   R/W   ----xxxx    Scrolling playfield (upper 4 bits)
    2800-2BFF   R/W   xxxxxxxx    Color RAM low
                R/W   -----xxx       (blue)
                R/W   --xxx---       (green)
                R/W   xx------       (red LSBs)
    2C00-2FFF   R/W   ----xxxx    Color RAM high
                R/W   -------x       (red MSB)
                R/W   ----xxx-       (intensity)
    3000-37BF   R/W   xxxxxxxx    Alphanumerics RAM
    37C0-37EF   R/W   xxxxxxxx    Motion object picture
    3800-382F   R/W   -xxxxxxx    Motion object flags
                R/W   -x---xx-       (picture bank)
                R/W   --x-----       (vertical flip)
                R/W   ---x----       (horizontal flip)
                R/W   ----x---       (32 pixels tall)
                R/W   -------x       (X position MSB)
    3840-386F   R/W   xxxxxxxx       (Y position)
    38C0-38EF   R/W   xxxxxxxx       (X position LSBs)
    3C00-3C01     W   xxxxxxxx    Scrolling playfield vertical position
    3D00-3D01     W   xxxxxxxx    Scrolling playfield horizontal position
    3E00-3FFF     W   xxxxxxxx    PIXI graphics expander RAM
    4000-7FFF   R     xxxxxxxx    Banked program ROM
    8000-FFFF   R     xxxxxxxx    Fixed program ROM
    ========================================================================
    Interrupts:
        NMI not connected
        IRQ generated by 32V
    ========================================================================


    ========================================================================
    CPU #2
    ========================================================================
    0000-07FF   R/W   xxxxxxxx    Z-page working RAM
    0800-083F   R/W   xxxxxxxx    Custom I/O
    1000          W   --------    Interrupt acknowledge
    1100          W   xxxxxxxx    Speech data
    1200          W   --------    Speech write strobe on
    1300          W   --------    Speech write strobe off
    1400          W   xxxxxxxx    Main CPU acknowledge latch
    1500          W   -------x    Speech chip reset
    1800        R     xxxxxxxx    Main CPU communication latch
    1C00        R     x-------    Speech chip ready
    1C01        R     xx------    Communications
                R     x-------       (sound CPU communication latch full flag)
                R     -x------       (sound CPU acknowledge latch full flag)
    8000-FFFF   R     xxxxxxxx    Program ROM
    ========================================================================
    Interrupts:
        NMI not connected
        IRQ generated by 32V
    ========================================================================

***************************************************************************/

#include "driver.h"
#include "deprecat.h"
#include "cpu/m6502/m6502.h"
#include "sound/5220intf.h"
#include "sound/pokey.h"
#include "jedi.h"


/* constants */
#define MAIN_CPU_OSC		10000000
#define SOUND_CPU_OSC		12096000


/* local variables */
static UINT8 control_num;
static UINT8 sound_latch;
static UINT8 sound_ack_latch;
static UINT8 sound_comm_stat;
static UINT8 speech_write_buffer;
static UINT8 speech_strobe_state;
static UINT8 nvram_enabled;

static emu_timer *jedi_timer;


/*************************************
 *
 *  Interrupt handling
 *
 *************************************/

static TIMER_CALLBACK( generate_interrupt )
{
	int scanline = param;

	/* IRQ is set by /32V */
	cpunum_set_input_line(machine, 0, M6502_IRQ_LINE, (scanline & 32) ? CLEAR_LINE : ASSERT_LINE);
	cpunum_set_input_line(machine, 1, M6502_IRQ_LINE, (scanline & 32) ? CLEAR_LINE : ASSERT_LINE);

	/* set up for the next */
	scanline += 32;
	if (scanline > 256)
		scanline = 32;
	timer_adjust_oneshot(jedi_timer,video_screen_get_time_until_pos(0, scanline, 0), scanline);
}


static WRITE8_HANDLER( main_irq_ack_w )
{
	cpunum_set_input_line(Machine, 0, M6502_IRQ_LINE, CLEAR_LINE);
}


static WRITE8_HANDLER( sound_irq_ack_w )
{
	cpunum_set_input_line(Machine, 1, M6502_IRQ_LINE, CLEAR_LINE);
}


static MACHINE_START( jedi )
{
	/* set a timer to run the interrupts */
	jedi_timer = timer_alloc(generate_interrupt, NULL);
	timer_adjust_oneshot(jedi_timer,video_screen_get_time_until_pos(0, 32, 0), 32);

	/* configure the banks */
	memory_configure_bank(1, 0, 3, memory_region(REGION_CPU1) + 0x10000, 0x4000);

	/* set up save state */
	state_save_register_global(control_num);
	state_save_register_global(sound_latch);
	state_save_register_global(sound_ack_latch);
	state_save_register_global(sound_comm_stat);
	state_save_register_global(speech_write_buffer);
	state_save_register_global(speech_strobe_state);
	state_save_register_global(nvram_enabled);
}


static MACHINE_RESET( jedi )
{
	/* init globals */
	control_num = 0;
	sound_latch = 0;
	sound_ack_latch = 0;
	sound_comm_stat = 0;
	speech_write_buffer = 0;
	speech_strobe_state = 0;
	nvram_enabled = 0;
}



/*************************************
 *
 *  Main program ROM banking
 *
 *************************************/

static WRITE8_HANDLER( rom_banksel_w )
{
	if (data & 0x01) memory_set_bank(1, 0);
	if (data & 0x02) memory_set_bank(1, 1);
	if (data & 0x04) memory_set_bank(1, 2);
}



/*************************************
 *
 *  Main CPU -> Sound CPU communications
 *
 *************************************/

static WRITE8_HANDLER( sound_reset_w )
{
	cpunum_set_input_line(Machine, 1, INPUT_LINE_RESET, (data & 1) ? CLEAR_LINE : ASSERT_LINE);
}


static TIMER_CALLBACK( delayed_sound_latch_w )
{
    sound_latch = param;
    sound_comm_stat |= 0x80;
}


static WRITE8_HANDLER( sound_latch_w )
{
	timer_call_after_resynch(NULL, data, delayed_sound_latch_w);
}


static READ8_HANDLER( sound_latch_r )
{
    sound_comm_stat &= ~0x80;
    return sound_latch;
}



/*************************************
 *
 *  Sound CPU -> Main CPU communications
 *
 *************************************/

static READ8_HANDLER( sound_ack_latch_r )
{
    sound_comm_stat &= ~0x40;
    return sound_ack_latch;
}


static WRITE8_HANDLER( sound_ack_latch_w )
{
    sound_ack_latch = data;
    sound_comm_stat |= 0x40;
}



/*************************************
 *
 *  I/O ports
 *
 *************************************/

static READ8_HANDLER( a2d_data_r )
{
	switch (control_num)
	{
		case 0:		return readinputport(2);
		case 2:		return readinputport(3);
		default:	return 0;
	}
    return 0;
}


static READ8_HANDLER( special_port1_r )
{
	return readinputport(1) ^ ((sound_comm_stat >> 1) & 0x60);
}


static WRITE8_HANDLER( a2d_select_w )
{
    control_num = offset;
}


static READ8_HANDLER( soundstat_r )
{
    return sound_comm_stat;
}


static WRITE8_HANDLER( jedi_coin_counter_w )
{
	coin_counter_w(offset, data);
}



/*************************************
 *
 *  Speech access
 *
 *************************************/

static WRITE8_HANDLER( speech_data_w )
{
	speech_write_buffer = data;
}


static WRITE8_HANDLER( speech_strobe_w )
{
	int state = (~offset >> 8) & 1;

	if ((state ^ speech_strobe_state) && state)
		tms5220_data_w(0, speech_write_buffer);
	speech_strobe_state = state;
}


static READ8_HANDLER( speech_ready_r )
{
    return (!tms5220_ready_r()) << 7;
}



/*************************************
 *
 *  NVRAM
 *
 *************************************/

static WRITE8_HANDLER( nvram_data_w )
{
	if (nvram_enabled)
		generic_nvram[offset] = data;
}


static WRITE8_HANDLER( nvram_enable_w )
{
	nvram_enabled = ~offset & 1;
}



/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( main_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x07ff) AM_RAM
	AM_RANGE(0x0800, 0x08ff) AM_READWRITE(MRA8_RAM, nvram_data_w) AM_BASE(&generic_nvram) AM_SIZE(&generic_nvram_size)
	AM_RANGE(0x0c00, 0x0c00) AM_READ(input_port_0_r)
	AM_RANGE(0x0c01, 0x0c01) AM_READ(special_port1_r)
	AM_RANGE(0x1400, 0x1400) AM_READ(sound_ack_latch_r)
	AM_RANGE(0x1800, 0x1800) AM_READ(a2d_data_r)
	AM_RANGE(0x1c00, 0x1c01) AM_WRITE(nvram_enable_w)
	AM_RANGE(0x1c80, 0x1c82) AM_WRITE(a2d_select_w)
	AM_RANGE(0x1d00, 0x1d00) AM_WRITE(MWA8_NOP)	/* NVRAM store */
	AM_RANGE(0x1d80, 0x1d80) AM_WRITE(watchdog_reset_w)
	AM_RANGE(0x1e00, 0x1e00) AM_WRITE(main_irq_ack_w)
	AM_RANGE(0x1e80, 0x1e81) AM_WRITE(jedi_coin_counter_w)
	AM_RANGE(0x1e82, 0x1e83) AM_WRITE(MWA8_NOP)	/* LED control; not used */
	AM_RANGE(0x1e84, 0x1e84) AM_WRITE(MWA8_RAM) AM_BASE(&jedi_foreground_bank)
	AM_RANGE(0x1e86, 0x1e86) AM_WRITE(sound_reset_w)
	AM_RANGE(0x1e87, 0x1e87) AM_WRITE(jedi_video_off_w)
	AM_RANGE(0x1f00, 0x1f00) AM_WRITE(sound_latch_w)
	AM_RANGE(0x1f80, 0x1f80) AM_WRITE(rom_banksel_w)
	AM_RANGE(0x2000, 0x27ff) AM_RAM AM_BASE(&jedi_backgroundram)
	AM_RANGE(0x2800, 0x2fff) AM_RAM AM_BASE(&jedi_paletteram)
	AM_RANGE(0x3000, 0x37bf) AM_RAM AM_BASE(&jedi_foregroundram)
	AM_RANGE(0x37c0, 0x3bff) AM_RAM AM_BASE(&jedi_spriteram)
	AM_RANGE(0x3c00, 0x3c01) AM_WRITE(jedi_vscroll_w)
	AM_RANGE(0x3d00, 0x3d01) AM_WRITE(jedi_hscroll_w)
	AM_RANGE(0x3e00, 0x3fff) AM_WRITE(jedi_PIXIRAM_w)
	AM_RANGE(0x4000, 0x7fff) AM_ROMBANK(1)
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END



/*************************************
 *
 *  Sound CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x07ff) AM_RAM
	AM_RANGE(0x0800, 0x080f) AM_READWRITE(pokey1_r, pokey1_w)
	AM_RANGE(0x0810, 0x081f) AM_READWRITE(pokey2_r, pokey2_w)
	AM_RANGE(0x0820, 0x082f) AM_READWRITE(pokey3_r, pokey3_w)
	AM_RANGE(0x0830, 0x083f) AM_READWRITE(pokey4_r, pokey4_w)
	AM_RANGE(0x1000, 0x1000) AM_WRITE(sound_irq_ack_w)
	AM_RANGE(0x1100, 0x11ff) AM_WRITE(speech_data_w)
	AM_RANGE(0x1200, 0x13ff) AM_WRITE(speech_strobe_w)
	AM_RANGE(0x1400, 0x1400) AM_WRITE(sound_ack_latch_w)
	AM_RANGE(0x1800, 0x1800) AM_READ(sound_latch_r)
	AM_RANGE(0x1c00, 0x1c00) AM_READ(speech_ready_r)
	AM_RANGE(0x1c01, 0x1c01) AM_READ(soundstat_r)
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END



/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( jedi )
	PORT_START	/* 0C00 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_SERVICE1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_COIN1 )

	PORT_START	/* 0C01 */
	PORT_BIT( 0x03, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x18, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* sound comm */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* analog Y */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Y ) PORT_SENSITIVITY(100) PORT_KEYDELTA(10)

	PORT_START	/* analog X */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_X ) PORT_SENSITIVITY(100) PORT_KEYDELTA(10)
INPUT_PORTS_END



/*************************************
 *
 *  Machine driver
 *
 *************************************/

static MACHINE_DRIVER_START( jedi )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6502,MAIN_CPU_OSC/2/2)		/* 2.5MHz */
	MDRV_CPU_PROGRAM_MAP(main_map,0)

	MDRV_CPU_ADD(M6502,SOUND_CPU_OSC/2/4)		/* 1.5MHz */
	MDRV_CPU_PROGRAM_MAP(sound_map,0)

	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_INTERLEAVE(4)

	MDRV_MACHINE_START(jedi)
	MDRV_MACHINE_RESET(jedi)
	MDRV_NVRAM_HANDLER(generic_0fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(64*8, 262) /* verify vert size */
	MDRV_SCREEN_VISIBLE_AREA(0*8, 37*8-1, 0*8, 30*8-1)

	MDRV_VIDEO_START(jedi)
	MDRV_VIDEO_UPDATE(jedi)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("left", "right")

	MDRV_SOUND_ADD(POKEY, SOUND_CPU_OSC/2/4)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "left", 0.30)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "right", 0.30)

	MDRV_SOUND_ADD(POKEY, SOUND_CPU_OSC/2/4)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "left", 0.30)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "right", 0.30)

	MDRV_SOUND_ADD(POKEY, SOUND_CPU_OSC/2/4)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "left", 0.30)

	MDRV_SOUND_ADD(POKEY, SOUND_CPU_OSC/2/4)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "right", 0.30)

	MDRV_SOUND_ADD(TMS5220, SOUND_CPU_OSC/2/9)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "left", 1.0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "right", 1.0)
MACHINE_DRIVER_END



/*************************************
 *
 *  ROM definitions
 *
 *************************************/

ROM_START( jedi )
	ROM_REGION( 0x1C000, REGION_CPU1, 0 )	/* 64k for code + 48k for banked ROMs */
	ROM_LOAD( "136030-221.14f",  0x08000, 0x4000, CRC(414d05e3) SHA1(e5f5f8d85433467a13d6ca9e3889e07a62b00e52) )
	ROM_LOAD( "136030-222.13f",  0x0c000, 0x4000, CRC(7b3f21be) SHA1(8fe62401f9b78c7a3e62b544c4b705b1bfa9b8f3) )
	ROM_LOAD( "136030-123.13d",  0x10000, 0x4000, CRC(877f554a) SHA1(8b51109cabd84741b024052f892b3172fbe83223) ) /* Page 0 */
	ROM_LOAD( "136030-124.13b",  0x14000, 0x4000, CRC(e72d41db) SHA1(1b3fcdc435f1e470e8d5b7241856e398a4c3910e) ) /* Page 1 */
	ROM_LOAD( "136030-122.13a",  0x18000, 0x4000, CRC(cce7ced5) SHA1(bff031a637aefca713355dbf251dcb5c2cea0885) ) /* Page 2 */

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* space for the sound ROMs */
	ROM_LOAD( "136030-133.01c",  0x8000, 0x4000, CRC(6c601c69) SHA1(618b77800bbbb4db34a53ca974a71bdaf89b5930) )
	ROM_LOAD( "136030-134.01a",  0xC000, 0x4000, CRC(5e36c564) SHA1(4b0afceb9a1d912f1d5c1f26928d244d5b14ea4a) )

	ROM_REGION( 0x02000, REGION_GFX1,0 )
	ROM_LOAD( "136030-215.11t",  0x00000, 0x2000, CRC(3e49491f) SHA1(ade5e846069c2fa6edf667469d13ce5a6a45c06d) ) /* Alphanumeric */

	ROM_REGION( 0x10000, REGION_GFX2,0 )
	ROM_LOAD( "136030-126.06r",  0x00000, 0x8000, CRC(9c55ece8) SHA1(b8faa23314bb0d199ef46199bfabd9cb17510dd3) ) /* Playfield */
	ROM_LOAD( "136030-127.06n",  0x08000, 0x8000, CRC(4b09dcc5) SHA1(d46b5f4fb69c4b8d823dd9c4d92f8713badfa44a) )

	ROM_REGION( 0x20000, REGION_GFX3, 0 )
	ROM_LOAD( "136030-130.01h",  0x00000, 0x8000, CRC(2646a793) SHA1(dcb5fd50eafbb27565bce099a884be83a9d82285) ) /* Sprites */
	ROM_LOAD( "136030-131.01f",  0x08000, 0x8000, CRC(60107350) SHA1(ded03a46996d3f2349df7f59fd435a7ad6ed465e) )
	ROM_LOAD( "136030-128.01m",  0x10000, 0x8000, CRC(24663184) SHA1(5eba142ed926671ee131430944e59f21a55a5c57) )
	ROM_LOAD( "136030-129.01k",  0x18000, 0x8000, CRC(ac86b98c) SHA1(9f86c8801a7293fa46e9432f1651dd85bf00f4b9) )

	ROM_REGION( 0x1000, REGION_PROMS, 0 )	/* background smoothing */
	ROM_LOAD( "136030-117.bin",   0x0000, 0x0800, BAD_DUMP CRC(9831bd55) SHA1(12945ef2d1582914125b9ee591567034d71d6573) )
	ROM_LOAD( "136030-118.bin",   0x0800, 0x0800, BAD_DUMP CRC(261fbfe7) SHA1(efc65a74a3718563a07b718e34d8a7aa23339a69) )
ROM_END



/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1984, jedi, 0, jedi, jedi, 0, ROT0, "Atari", "Return of the Jedi", GAME_IMPERFECT_GRAPHICS | GAME_SUPPORTS_SAVE )
