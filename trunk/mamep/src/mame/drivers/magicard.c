/******************************************************************************


    MAGIC CARD - IMPERA
    -------------------

    Preliminary driver by Roberto Fresca, David Haywood & Angelo Salese


    Games running on this hardware:

    * Magic Card (set 1),  Impera, 199?
    * Magic Card (set 2),  Impera, 199?

*******************************************************************************


    *** Hardware Notes ***

    These are actually the specs of the Philips CD-i console.

    Identified:

    - CPU:  1x Philips SCC 68070 CCA84 (16 bits Microprocessor, PLCC) @ 15 MHz
    - VSC:  1x Philips SCC 66470 CAB (Video and System Controller, QFP)

    - Protection: 1x Dallas TimeKey DS1207-1 (for book-keeping protection)

    - Crystals:   1x 30.0000 MHz.
                  1x 19.6608 MHz.

    - PLDs:       1x PAL16L8ACN
                  1x PALCE18V8H-25


*******************************************************************************


    *** General Notes ***

    Impera released "Magic Card" in a custom 16-bits PCB.
    The hardware was so expensive and they never have reached the expected sales,
    so... they ported the game to Impera/Funworld 8bits boards, losing part of
    graphics and sound/music quality. The new product was named "Magic Card II".

*******************************************************************************

TODO:

-Proper handling of the 68070 (68k with 32 address lines instead of 24)
 & handle the extra features properly (UART,DMA,Timers etc.)

-Proper emulation of the 66470 Video Chip (still many unhandled features)

-Inputs;

-Unknown sound chip (it's an ADPCM with eight channels);

-Many unknown memory maps;

*******************************************************************************/


#define CLOCK_A	XTAL_30MHz
#define CLOCK_B	XTAL_19_6608MHz

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "sound/2413intf.h"

static UINT16 *magicram;
static UINT16 *pcab_vregs;

/*************************
*     Video Hardware     *
*************************/

/*
66470
video and system controller
19901219/wjvg
*/
/*
TODO: check this register,doesn't seem to be 100% correct.
1fffe0  csr = control and status register
    w 00...... ........ DM = slow timing speed, normal dram mode
    w 01...... ........ DM = fast timing speed, page dram mode
    w 10...... ........ DM = fast timing speed, nibble dram mode
    w 11...... ........ DM = slow timing speed, dual-port vram mode
    w ..1..... ........ TD = 256/64 k dram's
    w ...1.... ........ CG = enable character generator
    w ....1... ........ DD = rom data acknowledge delay
    w .....1.. ........ ED = early dtack
    w ......0. ........ not used
    w .......1 ........ BE  = enable bus error (watchdog timer)
   r  ........ 1.......  DA  = vertical display active
   r  ........ .1......  FG  = set during frame grabbing (if fg in dcr set)
   r  ........ ..xxx...  not used
   r  ........ .....1..  IT2 = intn active
   r  ........ ......1.  IT1 = pixac free and intn active
   r  ........ .......1  BE  = bus error generated by watchdog timer
*/

/*63 at post test,6d all the time.*/
#define SCC_CSR_VREG    (pcab_vregs[0x00/2] & 0xffff)
#define SCC_CG_VREG		((SCC_CSR_VREG & 0x10)>>4)

/*
1fffe2  dcr = display command register
    w 1....... ........  DE = enable display
    w .00..... ........  CF = 20   MHz (or 19.6608 MHz)
    w .01..... ........  CF = 24   MHz
    w .10..... ........  CF = 28.5 MHz
    w .11..... ........  CF = 30   MHz
    w ...1.... ........  FD = 60/50 Hz frame duration
    w ....00.. ........  SM/SS = non-interlaced scan mode
    w ....01.. ........  SM/SS = double frequency scan mode
    w ....10.. ........  SM/SS = interlaced scan mode
    w ....11.. ........  SM/SS = interlaced field repeat scan mode
    w ......1. ........  LS = full screen/border
    w .......1 ........  CM = logical/physical screen
    w ........ 1.......  FG = 4/8 bits per pixel
    w ........ .1......  DF = enable frame grabbing
    w ........ ..00....  IC/DC = ICA and DCA inactive
    w ........ ..01....  IC/DC = ICA active, reduced DCA mode (DCA sz=16 byts)
    w ........ ..10....  IC/DC = ICA active, DCA inactive
    w ........ ..11....  IC/DC = ICA active, DCA active (DCA size=64 bytes)
    w ........ ....aaaa  VSR:H = video start address (MSB's)
*/

#define SCC_DCR_VREG    (pcab_vregs[0x02/2] & 0xffff)
#define SCC_DE_VREG		((SCC_DCR_VREG & 0x8000)>>15)
#define SCC_VSR_VREG_H  ((SCC_DCR_VREG & 0xf)>>0)

/*
1fffe4  vsr = video start register
    w aaaaaaaa aaaaaaaa  VSR:L = video start address (LSB's)
*/

#define SCC_VSR_VREG_L  (pcab_vregs[0x04/2] & 0xffff)
#define SCC_VSR_VREG    ((SCC_VSR_VREG_H)<<16) | (SCC_VSR_VREG_L)

/*
1fffe6  bcr = border colour register
    w ........ nnnnnnnn  in 8 bit mode
    w ........ nnnn....  in 4 bit mode
*/
/*
(Note: not present on the original vreg listing)
1fffe8 dcr2 = display command register 2
    w x....... ........  not used
    w .nn..... ........  OM = lower port of the video mode (with CM)
    w ...1.... ........  ID = Indipendent DCA bit
    w ....nn.. ........  MF = Mosaic Factor (2,4,8,16)
    w ......nn ........  FT = File Type (0/1 = bitmap, 2 = RLE, 3 = Mosaic)
    w ........ xxxx....  not used
    w ........ ....aaaa  "data" (dunno the purpose...)
*/
#define SCC_DCR2_VREG  (pcab_vregs[0x08/2] & 0xffff)

/*
(Note: not present on the original vreg listing)
1fffea dcp = ???
    w aaaaaaaa aaaaaa--  "data" (dunno the purpose...)
    w -------- ------xx not used
*/

/*
1fffec  swm = selective write mask register
    w nnnnnnnn ........  mask
*/
/*
1fffee  stm = selective mask register
    w ........ nnnnnnnn  mask
*/
/*
1ffff0  a = source register a
    w nnnnnnnn nnnnnnnn  source
*/
#define SCC_SRCA_VREG  (pcab_vregs[0x10/2] & 0xffff)

/*
1ffff2  b = destination register b
   rw nnnnnnnn nnnnnnnn  destination
*/

#define SCC_DSTB_VREG  (pcab_vregs[0x12/2] & 0xffff)

/*
1ffff4  pcr = pixac command register
    w 1....... ........  4N  = 8/4 bits per pixel
    w .1....00 ....x00.  COL = enable colour2 function
    w .1....00 .....01.  COL = enable colour1 function
    w .1...0.. .....10.  COL = enable bcolour2 function
    w .1...0.. .....11.  COL = enable bcolour1 function
    w ..1..000 ....x00.  EXC = enable exchange function
    w ..1..000 .....01.  EXC = enable swap function
    w ..1..000 .....10.  EXC = enable inverted exchange function
    w ..1..000 .....11.  EXC = enable inverted swap function
    w ...1..0. ....x00.  CPY = enable copy type b function
    w ...1...0 ....x10.  CPY = enable copy type a function
    w ...1..0. .....01.  CPY = enable patch type b function
    w ...1...0 .....11.  CPY = enable patch type a function
    w ....1000 .....00.  CMP = enable compare function
    w ....1000 .....10.  CMP = enable compact function
    w .....1.. ........  RTL = manipulate right to left
    w ......1. ........  SHK = shrink picture by factor 2
    w .......1 ........  ZOM = zoom picture by factor 2
    w ........ nnnn....  LGF = logical function
    w ........ 0000....  LGF = d=r
    w ........ 0001....  LGF = d=~r
    w ........ 0010....  LGF = d=0
    w ........ 0011....  LGF = d=1
    w ........ 0100....  LGF = d=~(d^r)
    w ........ 0101....  LGF = d=d^r
    w ........ 0110....  LGF = d=d&r
    w ........ 0111....  LGF = d=~d&r
    w ........ 1000....  LGF = d=~d&~r
    w ........ 1001....  LGF = d=d&~r
    w ........ 1010....  LGF = d=~d|r
    w ........ 1011....  LGF = d=d|r
    w ........ 1100....  LGF = d=d|~r
    w ........ 1101....  LGF = d=~d|~r
    w ........ 1110....  LGF = d=d
    w ........ 1111....  LGF = d=~d
    w ........ ....1...  INV = invert transparancy state of source bits
    w ........ .....1..  BIT = copy:     enable copy type a
    w ........ .....1..  BIT = colour:   enable bcolour/colour
    w ........ .....1..  BIT = compare:  compact/compare
    w ........ ......1.  TT  = perform transparancy test
    w ........ .......0
*/

#define SCC_PCR_VREG  (pcab_vregs[0x14/2] & 0xffff)

/*
1ffff6  mask = mask register
    w ........ ....nnnn  mask nibbles/0
*/
/*
1ffff8  shift = shift register
    w ......nn ........  shift by .. during source alignment
*/
/*
1ffffa  index = index register
    w ........ ......nn  bcolour: use bit .. in the source word
    w ........ ......nn  compact: nibble .. will hold the result
*/
/*
1ffffc  fc/bc = foreground/background colour register
    w nnnnnnnn ........  FC = foreground colour
    w ........ nnnnnnnn  BC = background colour
*/
/*
1ffffe  tc = transparent colour register
    w nnnnnnnn ........  transparent colour
*/



static VIDEO_START(magicard)
{

}

static VIDEO_UPDATE(magicard)
{
	int x,y;
	UINT32 count;

	bitmap_fill(bitmap, cliprect, get_black_pen(screen->machine)); //TODO

	if(!(SCC_DE_VREG)) //display enable
		return 0;

	count = ((SCC_VSR_VREG)/2);

	for(y=0;y<300;y++)
	{
		for(x=0;x<168;x++)
		{
			UINT32 color;

			color = ((magicram[count]) & 0x00ff)>>0;

			if((x*2)<video_screen_get_visible_area(screen)->max_x && ((y)+0)<video_screen_get_visible_area(screen)->max_y)
				*BITMAP_ADDR32(bitmap, y, (x*2)+1) = screen->machine->pens[color];

			color = ((magicram[count]) & 0xff00)>>8;

			if(((x*2)+1)<video_screen_get_visible_area(screen)->max_x && ((y)+0)<video_screen_get_visible_area(screen)->max_y)
				*BITMAP_ADDR32(bitmap, y, (x*2)+0) = screen->machine->pens[color];

			count++;
		}
	}

	return 0;
}


/*************************
* Memory map information *
*************************/

static READ16_HANDLER( test_r )
{
	return mame_rand(space->machine);
}

static WRITE16_HANDLER( paletteram_io_w )
{
	static int pal_offs,r,g,b,internal_pal_offs;

	switch(offset*2)
	{
		case 0:
			pal_offs = data;
			break;
		case 4:
			internal_pal_offs = 0;
			break;
		case 2:
			switch(internal_pal_offs)
			{
				case 0:
					r = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
					internal_pal_offs++;
					break;
				case 1:
					g = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
					internal_pal_offs++;
					break;
				case 2:
					b = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
					palette_set_color(space->machine, pal_offs, MAKE_RGB(r, g, b));
					internal_pal_offs = 0;
					pal_offs++;
					break;
			}

			break;
	}
}

static READ16_HANDLER( philips_66470_r )
{
	switch(offset)
	{
//      case 0/2:
//          return mame_rand(space->machine); //TODO
	}

	printf("[%04x]\n",offset*2);


	return pcab_vregs[offset];
}

static WRITE16_HANDLER( philips_66470_w )
{
	COMBINE_DATA(&pcab_vregs[offset]);

//  if(offset == 0x10/2)
//  {
		//printf("%04x %04x %04x\n",data,pcab_vregs[0x12/2],pcab_vregs[0x14/2]);
		//pcab_vregs[0x12/2] = pcab_vregs[0x10/2];
//  }
}

static ADDRESS_MAP_START( magicard_mem, ADDRESS_SPACE_PROGRAM, 16 )
//  ADDRESS_MAP_GLOBAL_MASK(0x1fffff)
	AM_RANGE(0x000000, 0x0fffff) AM_RAM AM_BASE(&magicram) /*only 0-7ffff accessed in Magic Card*/
//  AM_RANGE(0x100000, 0x17ffff) AM_RAM AM_REGION("main", 0)
	AM_RANGE(0x180000, 0x1ffbff) AM_ROM AM_REGION("main", 0)
	/* 1ffc00-1ffdff System I/O */
	AM_RANGE(0x1ffc00, 0x1ffc01) AM_READ(test_r)
	AM_RANGE(0x1ffc40, 0x1ffc41) AM_READ(test_r)
	AM_RANGE(0x1ffd00, 0x1ffd05) AM_WRITE(paletteram_io_w) //RAMDAC
	/*not the right sound chip,unknown type,it should be an ADPCM with 8 channels.*/
	AM_RANGE(0x1ffd40, 0x1ffd43) AM_DEVWRITE8(SOUND, "ym", ym2413_w, 0x00ff)
	AM_RANGE(0x1ffd80, 0x1ffd81) AM_READ(test_r)
	AM_RANGE(0x1ffd80, 0x1ffd81) AM_WRITENOP //?
	AM_RANGE(0x1fff80, 0x1fffbf) AM_RAM //DRAM I/O, not accessed by this game, CD buffer?
	AM_RANGE(0x1fffe0, 0x1fffff) AM_READWRITE(philips_66470_r,philips_66470_w) AM_BASE(&pcab_vregs) //video registers
	AM_RANGE(0x200000, 0x2fffff) AM_RAM
ADDRESS_MAP_END


/*************************
*      Input ports       *
*************************/

static INPUT_PORTS_START( magicard )
INPUT_PORTS_END


MACHINE_RESET( magicard )
{
	UINT16 *src    = (UINT16*)memory_region( machine, "main" );
	UINT16 *dst    = magicram;
	memcpy (dst, src, 0x80000);
	device_reset(cputag_get_cpu(machine, "main"));
}

/*************************
*    Machine Drivers     *
*************************/

/*Probably there's a mask somewhere if it REALLY uses irqs at all...irq vectors dynamically changes after some time.*/
static INTERRUPT_GEN( magicard_irq )
{
//  if(input_code_pressed(KEYCODE_Z))
//      cpu_set_input_line(device->machine->cpu[0], 1, HOLD_LINE);
//  magicram[0x2004/2]^=0xffff;
}

static MACHINE_DRIVER_START( magicard )
	MDRV_CPU_ADD("main", M68000, CLOCK_A/2)	/* SCC-68070 CCA84 datasheet */
	MDRV_CPU_PROGRAM_MAP(magicard_mem,0)
 	MDRV_CPU_VBLANK_INT("main", magicard_irq) /* no interrupts? (it erases the vectors..) */

	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(400, 300)
	MDRV_SCREEN_VISIBLE_AREA(0, 320-1, 0, 256-1) //dynamic resolution,TODO

	MDRV_PALETTE_LENGTH(0x100)

	MDRV_VIDEO_START(magicard)
	MDRV_VIDEO_UPDATE(magicard)

	MDRV_MACHINE_RESET(magicard)

	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("ym", YM2413, CLOCK_A/12)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_DRIVER_END

/*************************
*        Rom Load        *
*************************/

ROM_START( magicard )
	ROM_REGION( 0x80000, "main", 0 ) /* 68000 Code & GFX */
	ROM_LOAD16_WORD_SWAP( "magicorg.bin", 0x000000, 0x80000, CRC(810edf9f) SHA1(0f1638a789a4be7413aa019b4e198353ba9c12d9) )

	ROM_REGION( 0x0100, "proms", 0 ) /* Color PROM?? */
	ROM_LOAD16_WORD_SWAP("mgorigee.bin",	0x0000,	0x0100, CRC(73522889) SHA1(3e10d6c1585c3a63cff717a0b950528d5373c781) )
ROM_END

ROM_START( magicrda )
	ROM_REGION( 0x80000, "main", 0 ) /* 68000 Code & GFX */
	ROM_LOAD16_WORD_SWAP( "mcorigg2.bin", 0x00000, 0x20000, CRC(48546aa9) SHA1(23099a5e4c9f2c3386496f6d7f5bb7d435a6fb16) )
	ROM_RELOAD(                           0x40000, 0x20000 )
	ROM_LOAD16_WORD_SWAP( "mcorigg1.bin", 0x20000, 0x20000, CRC(c9e4a38d) SHA1(812e5826b27c7ad98142a0f52fbdb6b61a2e31d7) )
	ROM_RELOAD(                           0x40001, 0x20000 )

	ROM_REGION( 0x0100, "proms", 0 ) /* Color PROM?? */
	ROM_LOAD("mgorigee.bin",	0x0000,	0x0100, CRC(73522889) SHA1(3e10d6c1585c3a63cff717a0b950528d5373c781) )
ROM_END

ROM_START( magicrdb )
	ROM_REGION( 0x80000, "main", 0 ) /* 68000 Code & GFX */
	ROM_LOAD16_WORD_SWAP( "mg_8.bin", 0x00000, 0x80000, CRC(f5499765) SHA1(63bcf40b91b43b218c1f9ec1d126a856f35d0844) )

	/*bigger than the other sets?*/
	ROM_REGION( 0x20000, "proms", 0 ) /* Color PROM?? */
	ROM_LOAD16_WORD_SWAP("mg_u3.bin",	0x00000,	0x20000, CRC(2116de31) SHA1(fb9c21ca936532e7c342db4bcaaac31c478b1a35) )
ROM_END

/*************************
*      Game Drivers      *
*************************/

static DRIVER_INIT( magicard )
{
	//...
}

/*    YEAR  NAME      PARENT MACHINE   INPUT  INIT  ROT    COMPANY   FULLNAME             FLAGS... */

GAME( 199?, magicard, 0,     magicard, 0,     magicard,    ROT0, "Impera", "Magic Card (set 1)", GAME_NO_SOUND | GAME_NOT_WORKING )
GAME( 199?, magicrda, 0,     magicard, 0,     magicard,    ROT0, "Impera", "Magic Card (set 2)", GAME_NO_SOUND | GAME_NOT_WORKING )
GAME( 199?, magicrdb, 0,     magicard, 0,     magicard,    ROT0, "Impera", "Magic Card (set 3)", GAME_NO_SOUND | GAME_NOT_WORKING )

/*Below here there are CD-I bios defines,to be removed in the end*/
/*
ROM_START( mcdi200 )
    ROM_REGION( 0x80000, "main", 0 )
    ROM_LOAD16_WORD( "mgvx200.rom", 0x000000, 0x80000, CRC(40c4e6b9) SHA1(d961de803c89b3d1902d656ceb9ce7c02dccb40a) )
ROM_END

ROM_START( pcdi490 )
    ROM_REGION( 0x80000, "main", 0 )
    ROM_LOAD16_WORD( "phlp490.rom", 0x000000, 0x80000, CRC(e115f45b) SHA1(f71be031a5dfa837de225081b2ddc8dcb74a0552) )
ROM_END

ROM_START( pcdi910m )
    ROM_REGION( 0x80000, "main", 0 )
    ROM_LOAD16_WORD( "cdi910.rom", 0x000000, 0x80000,  CRC(8ee44ed6) SHA1(3fcdfa96f862b0cb7603fb6c2af84cac59527b05) )
ROM_END

GAME( 199?, mcdi200, 0,     magicard, 0,     magicard,    ROT0, "Philips", "Magnavox CD-I 200 BIOS", GAME_NO_SOUND | GAME_NOT_WORKING )
GAME( 199?, pcdi490, 0,     magicard, 0,     magicard,    ROT0, "Philips", "Philips CD-I 490 BIOS", GAME_NO_SOUND | GAME_NOT_WORKING )
GAME( 199?, pcdi910m,0,     magicard, 0,     magicard,    ROT0, "Philips", "Philips CD-I 910 (Memorex-Tandy) BIOS", GAME_NO_SOUND | GAME_NOT_WORKING )
*/
