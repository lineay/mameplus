/***************************************************************************

    Centuri Aztarac hardware

    driver by Mathis Rosenhauer
    Thanks to David Fish for additional hardware information.

    Games supported:
        * Aztarac

    Known bugs:
        * none at this time

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "cpu/m68000/m68000.h"
#include "video/vector.h"
#include "aztarac.h"
#include "sound/ay8910.h"



/*************************************
 *
 *  Machine init
 *
 *************************************/

static IRQ_CALLBACK(aztarac_irq_callback)
{
	return 0xc;
}


static MACHINE_RESET( aztarac )
{
	cpu_set_irq_callback(machine->cpu[0], aztarac_irq_callback);
}



/*************************************
 *
 *  NVRAM handler
 *
 *************************************/

static READ16_HANDLER( nvram_r )
{
	return generic_nvram16[offset] | 0xfff0;
}



/*************************************
 *
 *  Input ports
 *
 *************************************/

static READ16_HANDLER( joystick_r )
{
    return (((input_port_read(space->machine, "STICKZ") - 0xf) << 8) |
            ((input_port_read(space->machine, "STICKY") - 0xf) & 0xff));
}



/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( main_map, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x00bfff) AM_ROM
	AM_RANGE(0x022000, 0x0220ff) AM_READWRITE(nvram_r, SMH_RAM) AM_BASE(&generic_nvram16) AM_SIZE(&generic_nvram_size)
	AM_RANGE(0x027000, 0x027001) AM_READ(joystick_r)
	AM_RANGE(0x027004, 0x027005) AM_READ_PORT("INPUTS")
	AM_RANGE(0x027008, 0x027009) AM_READWRITE(aztarac_sound_r, aztarac_sound_w)
	AM_RANGE(0x02700c, 0x02700d) AM_READ_PORT("DIAL")
	AM_RANGE(0x02700e, 0x02700f) AM_READ(watchdog_reset16_r)
	AM_RANGE(0xff8000, 0xffafff) AM_RAM AM_BASE(&aztarac_vectorram)
	AM_RANGE(0xffb000, 0xffb001) AM_WRITE(aztarac_ubr_w)
	AM_RANGE(0xffe000, 0xffffff) AM_RAM
ADDRESS_MAP_END



/*************************************
 *
 *  Sound CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x1fff) AM_ROM
	AM_RANGE(0x8000, 0x87ff) AM_RAM
	AM_RANGE(0x8800, 0x8800) AM_READ(aztarac_snd_command_r)
	AM_RANGE(0x8c00, 0x8c01) AM_DEVREADWRITE(SOUND, "ay1", ay8910_r, ay8910_data_address_w)
	AM_RANGE(0x8c02, 0x8c03) AM_DEVREADWRITE(SOUND, "ay2", ay8910_r, ay8910_data_address_w)
	AM_RANGE(0x8c04, 0x8c05) AM_DEVREADWRITE(SOUND, "ay3", ay8910_r, ay8910_data_address_w)
	AM_RANGE(0x8c06, 0x8c07) AM_DEVREADWRITE(SOUND, "ay4", ay8910_r, ay8910_data_address_w)
	AM_RANGE(0x9000, 0x9000) AM_READWRITE(aztarac_snd_status_r, aztarac_snd_status_w)
ADDRESS_MAP_END



/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( aztarac )
	PORT_START("STICKZ")
	PORT_BIT( 0x1f, 0xf, IPT_AD_STICK_Z ) PORT_MINMAX(0,0x1e) PORT_SENSITIVITY(100) PORT_KEYDELTA(1)

	PORT_START("STICKY")
	PORT_BIT( 0x1f, 0xf, IPT_AD_STICK_Y ) PORT_MINMAX(0,0x1e) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_REVERSE

	PORT_START("DIAL")
	PORT_BIT( 0xff, 0x00, IPT_DIAL ) PORT_SENSITIVITY(25) PORT_KEYDELTA(10) PORT_CODE_DEC(KEYCODE_Z) PORT_CODE_INC(KEYCODE_X) PORT_REVERSE

	PORT_START("INPUTS")
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE_NO_TOGGLE(0x80, IP_ACTIVE_LOW)
INPUT_PORTS_END



/*************************************
 *
 *  Machine drivers
 *
 *************************************/

static MACHINE_DRIVER_START( aztarac )

	/* basic machine hardware */
	MDRV_CPU_ADD("main", M68000, 8000000)
	MDRV_CPU_PROGRAM_MAP(main_map,0)
	MDRV_CPU_VBLANK_INT("main", irq4_line_hold)

	MDRV_CPU_ADD("audio", Z80, 2000000)
	MDRV_CPU_PROGRAM_MAP(sound_map,0)
	MDRV_CPU_PERIODIC_INT(aztarac_snd_timed_irq, 100)

	MDRV_MACHINE_RESET(aztarac)
	MDRV_NVRAM_HANDLER(generic_1fill)

	/* video hardware */
	MDRV_SCREEN_ADD("main", VECTOR)
	MDRV_SCREEN_REFRESH_RATE(40)
	MDRV_SCREEN_SIZE(400, 300)
	MDRV_SCREEN_VISIBLE_AREA(0, 1024-1, 0, 768-1)

	MDRV_VIDEO_START(aztarac)
	MDRV_VIDEO_UPDATE(vector)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("ay1", AY8910, 2000000)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.15)

	MDRV_SOUND_ADD("ay2", AY8910, 2000000)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.15)

	MDRV_SOUND_ADD("ay3", AY8910, 2000000)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.15)

	MDRV_SOUND_ADD("ay4", AY8910, 2000000)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.15)
MACHINE_DRIVER_END



/*************************************
 *
 *  ROM definitions
 *
 *************************************/

ROM_START( aztarac )
	ROM_REGION( 0xc000, "main", 0 )
	ROM_LOAD16_BYTE( "l8_6.bin", 0x000000, 0x001000, CRC(25f8da18) SHA1(e8179ba3683e39c8225b549ead74c8af2d0a0b3e) )
	ROM_LOAD16_BYTE( "n8_0.bin", 0x000001, 0x001000, CRC(04e20626) SHA1(2b6a04992037257830df2c01a6da748fb4449f79) )
	ROM_LOAD16_BYTE( "l7_7.bin", 0x002000, 0x001000, CRC(230e244c) SHA1(42283a368144acf2aad2ef390e312e0951c3ea64) )
	ROM_LOAD16_BYTE( "n7_1.bin", 0x002001, 0x001000, CRC(37b12697) SHA1(da288b077902e3205600a021c3fac5730f9fb832) )
	ROM_LOAD16_BYTE( "l6_8.bin", 0x004000, 0x001000, CRC(1293fb9d) SHA1(5a8d512372fd38f1a55f990f5c3eb51833c463d8) )
	ROM_LOAD16_BYTE( "n6_2.bin", 0x004001, 0x001000, CRC(712c206a) SHA1(eb29f161189c14d84896502940e3ab6cc3bd1cd0) )
	ROM_LOAD16_BYTE( "l5_9.bin", 0x006000, 0x001000, CRC(743a6501) SHA1(da83a8f756466bcd94d4b0cc28a1a1858e9532f3) )
	ROM_LOAD16_BYTE( "n5_3.bin", 0x006001, 0x001000, CRC(a65cbf99) SHA1(dd06f98c0989604bd4ac6317e545e1fcf6722e75) )
	ROM_LOAD16_BYTE( "l4_a.bin", 0x008000, 0x001000, CRC(9cf1b0a1) SHA1(dd644026f49d8430c0b4cf4c750dc33c013c19fc) )
	ROM_LOAD16_BYTE( "n4_4.bin", 0x008001, 0x001000, CRC(5f0080d5) SHA1(fb1303f9a02067faea2ac4d523051c416de9cf35) )
	ROM_LOAD16_BYTE( "l3_b.bin", 0x00a000, 0x001000, CRC(8cc7f7fa) SHA1(fefb9a4fdd63878bc5d8138e3e8456cb6638425a) )
	ROM_LOAD16_BYTE( "n3_5.bin", 0x00a001, 0x001000, CRC(40452376) SHA1(1d058b7ecd2bbff3393950aab9215b262908475b) )

	ROM_REGION( 0x10000, "audio", 0 )
	ROM_LOAD( "j4_c.bin", 0x0000, 0x1000, CRC(e897dfcd) SHA1(750df3d08512d8098a13ec62677831efa164c126) )
	ROM_LOAD( "j3_d.bin", 0x1000, 0x1000, CRC(4016de77) SHA1(7232ec003f1b9d3623d762f3270108a1d1837846) )
ROM_END



/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1983, aztarac, 0, aztarac, aztarac, 0, ROT0, "Centuri", "Aztarac", 0 )
