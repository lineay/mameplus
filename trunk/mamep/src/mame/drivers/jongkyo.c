/* Jongkyo */
#include "driver.h"
#include "sound/ay8910.h"
#include "machine/segacrpt.h"

#define JONGKYO_CLOCK 18432000

/*

Jongkyo
(c)1985 Kiwako

834-5558 JONGKYO
C2-00173

CPU: SEGA Custom 315-5084 (Z80)
Sound: AY-3-8910
OSC: 18.432MHz

ROMs:
EPR-6258 (2764)
EPR-6259 (2764)
EPR-6260 (2764)
EPR-6261 (2764)
EPR-6262 (2732)

PR-6263.6J (82S123N)
PR-6264.0H (82S123N)
PR-6265.0M (82S129N)
PR-6266.0B (82S129N)

*/


static int rom_bank;


VIDEO_START(jongkyo)
{

}

VIDEO_UPDATE(jongkyo)
{
	int y;

	for (y = 0; y < 256; ++y)
	{
		int x;

		for (x = 0; x < 256; x += 4)
		{
			int b;
			UINT8 data;

			// bottom layer
			data = videoram[0x4000 + x/4 + y*64];
			for (b = 0; b < 4; ++b)
			{
				*BITMAP_ADDR16(bitmap, 255-y, 255-(x+b)) = (data & 0x01) + ((data & 0x10) >> 3);

				data >>= 1;
			}

			// top layer
			data = videoram[x/4 + y*64];
			for (b = 0; b < 4; ++b)
			{
				if (data & 0x11)
					*BITMAP_ADDR16(bitmap, 255-y, 255-(x+b)) = 4 + (data & 0x01) + ((data & 0x10) >> 3);

				data >>= 1;
			}
		}
	}

	return 0;
}


static WRITE8_HANDLER( bank_select_w )
{
	int mask = 1 << (offset >> 1);

	rom_bank &= ~mask;

	if (offset & 1)
		rom_bank |= mask;

	memory_set_bank(1, rom_bank);
}


static READ8_HANDLER( keyboard1_r )
{
	return 0xff;
}


static READ8_HANDLER( keyboard2_r )
{
	return 0xff;
}


static ADDRESS_MAP_START( jongkyo_memmap, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x6bff) AM_ROM	AM_WRITENOP // fixed
	AM_RANGE(0x6c00, 0x6fff) AM_READ(MRA8_BANK1)	// banked (8 banks)
	AM_RANGE(0x7000, 0x77ff) AM_RAM
	AM_RANGE(0x8000, 0xffff) AM_RAM AM_BASE(&videoram)
ADDRESS_MAP_END


static ADDRESS_MAP_START( jongkyo_portmap, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_FLAGS( AMEF_ABITS(8) )
	// R 01 keyboard
	AM_RANGE(0x01, 0x01) AM_READ(AY8910_read_port_0_r)
	AM_RANGE(0x02, 0x02) AM_WRITE(AY8910_write_port_0_w)
	AM_RANGE(0x03, 0x03) AM_WRITE(AY8910_control_port_0_w)

	AM_RANGE(0x10, 0x10) AM_READ_PORT("DSW")
	AM_RANGE(0x11, 0x11) AM_READ_PORT("IN0")
	// W 11 select keyboard row (fe fd fb f7)
	AM_RANGE(0x40, 0x45) AM_WRITE(bank_select_w)
ADDRESS_MAP_END

/*
-------------------------------------------------------------
Jongkyo ?1985 Kiwako
DIPSW         |      |1    2    3    4   |5   |6   |7   |8
-------------------------------------------------------------
Payout rate   |50%   |on   on   on   on  |    |    |    |
              |53%   |off  on   on   on  |    |    |    |
              |56%   |on   off  on   on  |    |    |    |
              |59%   |off  off  on   on  |    |    |    |
              |62%   |on   on   off  on  |    |    |    |
              |65%   |off  on   off  on  |    |    |    |
              |68%   |on   off  off  on  |    |    |    |
              |71%   |off  off  off  on  |    |    |    |
              |75%   |on   on   on   off |    |    |    |
              |78%   |off  on   on   off |    |    |    |
              |81%   |on   off  on   off |    |    |    |
              |84%   |off  off  on   off |    |    |    |
              |87%   |on   on   off  off |    |    |    |
              |90%   |off  on   off  off |    |    |    |
              |93%   |on   off  off  off |    |    |    |
              |96%   |off  off  off  off |    |    |    |
-------------------------------------------------------------
Start chance  |Yes   |                   |on  |    |    |
              |No    |                   |off |    |    |
-------------------------------------------------------------
Bet up        |Yes   |                   |    |on  |    |
              |No    |                   |    |off |    |
-------------------------------------------------------------
Last chance   |5     |                   |    |    |on  |
              |1     |                   |    |    |off |
-------------------------------------------------------------
Bonus credit  |50    |                   |    |    |    |on
              |10    |                   |    |    |    |off
-------------------------------------------------------------
*/


INPUT_PORTS_START( jongkyo )
	PORT_START_TAG("IN0")
	PORT_DIPNAME( 0x01, 0x00, "Note" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x00, "Memory Reset" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x04, 0x00, "Analizer" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
	PORT_SERVICE( 0x08, IP_ACTIVE_HIGH )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START_TAG("DSW")
	PORT_DIPNAME( 0x0f, 0x0f, "Payout Rate" ) PORT_DIPLOCATION("SW:1,2,3,4")
	PORT_DIPSETTING(    0x00, "50%" )
	PORT_DIPSETTING(    0x01, "53%" )
	PORT_DIPSETTING(    0x02, "56%" )
	PORT_DIPSETTING(    0x03, "59%" )
	PORT_DIPSETTING(    0x04, "62%" )
	PORT_DIPSETTING(    0x05, "65%" )
	PORT_DIPSETTING(    0x06, "68%" )
	PORT_DIPSETTING(    0x07, "71%" )
	PORT_DIPSETTING(    0x08, "75%" )
	PORT_DIPSETTING(    0x09, "78%" )
	PORT_DIPSETTING(    0x0a, "81%" )
	PORT_DIPSETTING(    0x0b, "84$" )
	PORT_DIPSETTING(    0x0c, "87%" )
	PORT_DIPSETTING(    0x0d, "90%" )
	PORT_DIPSETTING(    0x0e, "93%" )
	PORT_DIPSETTING(    0x0f, "96%" )
	PORT_DIPNAME( 0x10, 0x10, "Start Chance" ) PORT_DIPLOCATION("SW:5")
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, "Bet Up" ) PORT_DIPLOCATION("SW:6")
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Last Chance" ) PORT_DIPLOCATION("SW:7")
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, "Bonus Credit" ) PORT_DIPLOCATION("SW:8")
	PORT_DIPSETTING(    0x80, "10" )
	PORT_DIPSETTING(    0x00, "50" )
INPUT_PORTS_END


static struct AY8910interface ay8910_interface =
{
	keyboard1_r,
	keyboard2_r,
};

static MACHINE_DRIVER_START( jongkyo )
	/* basic machine hardware */
	MDRV_CPU_ADD(Z80,JONGKYO_CLOCK/4)
	MDRV_CPU_PROGRAM_MAP(jongkyo_memmap,0)
	MDRV_CPU_IO_MAP(jongkyo_portmap,0)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER )
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_SCREEN_VISIBLE_AREA(0, 256-1, 8, 256-8-1)
	MDRV_PALETTE_LENGTH(0x100)

	MDRV_VIDEO_START(jongkyo)
	MDRV_VIDEO_UPDATE(jongkyo)

	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD(AY8910, JONGKYO_CLOCK/8)
	MDRV_SOUND_CONFIG(ay8910_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_DRIVER_END


ROM_START( jongkyo )
	ROM_REGION( 0x9000, REGION_CPU1, 0 )
	ROM_LOAD( "epr-6258", 0x00000, 0x02000, CRC(fb8b7bcc) SHA1(8ece7c2c82c237b4b51829d412b2109b96ccd0e7) )
	ROM_LOAD( "epr-6259", 0x02000, 0x02000, CRC(e46cde5d) SHA1(1cbe1677cfb3fa9f76ad90d5b1446ce9cefee6b7) )
	ROM_LOAD( "epr-6260", 0x04000, 0x02000, CRC(369a5365) SHA1(037a2971a59ab339595b333cbdfd4cbb104de2be) )
	ROM_LOAD( "epr-6262", 0x06000, 0x01000, CRC(ecf50f34) SHA1(ecfa1a9360d8fbcbed457d46e53bae77f6d78c1d) )
	ROM_LOAD( "epr-6261", 0x07000, 0x02000, CRC(9c475ae1) SHA1(b993c2636dafed9f80fa87e71921c3c85c039e45) )	// banked at 6c00-6fff

	ROM_REGION( 0x40000, REGION_PROMS, 0 )
	ROM_LOAD( "pr-6263.6j", 0x00000, 0x00020, CRC(468134d9) SHA1(bb633929df17e448882ee80613fc1dfac3c35d7a) )
	ROM_LOAD( "pr-6264.0h", 0x00000, 0x00020, CRC(46014727) SHA1(eec451f292ee319fa6bfbbf223aaa12b231692c1) )
	ROM_LOAD( "pr-6265.0m", 0x00000, 0x00100, CRC(f09d3c4c) SHA1(a9e752d75e7f3ebd05add4ccf2f9f15d8f9a8d15) )
	ROM_LOAD( "pr-6266.0b", 0x00000, 0x00100, CRC(86aeafd1) SHA1(c4e5c56ce5baf2be3962675ae333e28bd8108a00) )
ROM_END


DRIVER_INIT( jongkyo )
{
	int i;
	UINT8 *rom = memory_region(REGION_CPU1);

	/* first of all, do a simple bitswap */
	for (i = 0x6000; i < 0x9000; ++i)
	{
		rom[i] = BITSWAP8(rom[i], 7,6,5,3,4,2,1,0);
	}

	/* then do the standard Sega decryption */
	jongkyo_decode();
}


GAME( 1985, jongkyo,  0,    jongkyo, jongkyo,  jongkyo, ROT0, "Kiwako", "Jongkyo", GAME_NOT_WORKING )