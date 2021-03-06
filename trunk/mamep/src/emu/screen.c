// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    screen.c

    Core MAME screen device.

***************************************************************************/

#include "emu.h"
#include "emuopts.h"
#include "png.h"
#include "rendutil.h"
#ifdef USE_SCALE_EFFECTS
#include "osdscale.h"
#endif /* USE_SCALE_EFFECTS */



//**************************************************************************
//  DEBUGGING
//**************************************************************************

#define VERBOSE                     (0)
#define LOG_PARTIAL_UPDATES(x)      do { if (VERBOSE) logerror x; } while (0)



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
const device_type SCREEN = &device_creator<screen_device>;

const attotime screen_device::DEFAULT_FRAME_PERIOD(attotime::from_hz(DEFAULT_FRAME_RATE));

UINT32 screen_device::m_id_counter = 0;

#ifdef USE_SCALE_EFFECTS
//**************************************************************************
//  scaler dimensions
//**************************************************************************
int     use_work_bitmap;
int     scale_depth;
int     scale_xsize;
int     scale_ysize;
int     scale_bank_offset;
#endif /* USE_SCALE_EFFECTS */

//**************************************************************************
//  SCREEN DEVICE
//**************************************************************************

#ifdef USE_SCALE_EFFECTS
void screen_device::video_init_scale_effect()
{
	use_work_bitmap = (m_texformat != TEXFORMAT_RGB32);
	scale_depth = 32;

	if (scale_init())
	{
		logerror("WARNING: scale effect is disabled\n");
		scale_effect.effect = 0;
		return;
	}

	if (scale_check(scale_depth))
	{
		int old_depth = scale_depth;

		use_work_bitmap = 1;
		scale_depth = (scale_depth == 15) ? 32 : 15;
		if (scale_check(scale_depth))
		{
			popmessage(_("scale_effect \"%s\" does not support both depth 15 and 32. scale effect is disabled."),
				scale_desc(scale_effect.effect));

			scale_exit();
			scale_effect.effect = 0;
			scale_init();
			return;
		}
		else
			logerror("WARNING: scale_effect \"%s\" does not support depth %d, use depth %d\n", scale_desc(scale_effect.effect), old_depth, scale_depth);
	}

	logerror("scale effect: %s (depth:%d)\n", scale_effect.name, scale_depth);

	realloc_scale_bitmaps();
}


void screen_device::video_exit_scale_effect()
{
	free_scale_bitmap();
	scale_exit();
}


void screen_device::free_scale_bitmap()
{
	int bank;
	m_changed &= ~UPDATE_HAS_NOT_CHANGED;

	for (bank = 0; bank < 2; bank++)
	{
		// restore mame screen
		if ((m_texture[bank]) && (m_bitmap[bank].valid()))
			m_texture[bank]->set_bitmap(m_bitmap[bank], m_visarea, m_bitmap[bank].texformat());

		if (m_scale_bitmap[bank] != NULL)
		{
			auto_free(machine(), m_scale_bitmap[bank]);
			m_scale_bitmap[bank] = NULL;
		}

		if (m_work_bitmap[0][bank] != NULL)
		{
			auto_free(machine(), m_work_bitmap[0][bank]);
			m_work_bitmap[0][bank] = NULL;
		}

		if (m_work_bitmap[1][bank] != NULL)
		{
			auto_free(machine(), m_work_bitmap[1][bank]);
			m_work_bitmap[1][bank] = NULL;
		}
	}

	scale_xsize = 0;
	scale_ysize = 0;
}


void screen_device::convert_palette_to_32(const bitmap_t &src, bitmap_t &dst, const rectangle &visarea, UINT32 palettebase)
{
	const rgb_t *palette =  m_palette->palette()->entry_list_adjusted() + palettebase;
	int x, y;

	for (y = visarea.min_y; y < visarea.max_y; y++)
	{
		UINT32 *dst32 = &dst.pixt<UINT32>(y, visarea.min_x);
		UINT16 *src16 = &src.pixt<UINT16>(y, visarea.min_x);

		for (x = visarea.min_x; x < visarea.max_x; x++)
			*dst32++ = palette[*src16++];
	}
}

void screen_device::convert_palette_to_15(const bitmap_t &src, bitmap_t &dst, const rectangle &visarea, UINT32 palettebase)
{
	const rgb_t *palette =  m_palette->palette()->entry_list_adjusted() + palettebase;
	int x, y;

	for (y = visarea.min_y; y < visarea.max_y; y++)
	{
		UINT16 *dst16 = &dst.pixt<UINT16>(y, visarea.min_x);
		UINT16 *src16 = &src.pixt<UINT16>(y, visarea.min_x);

		for (x = visarea.min_x; x < visarea.max_x; x++)
			*dst16++ = (palette[*src16++]).as_rgb15();
	}
}


static void convert_15_to_32(const bitmap_t &src, bitmap_t &dst, const rectangle &visarea)
{
	int x, y;

	for (y = visarea.min_y; y < visarea.max_y; y++)
	{
		UINT32 *dst32 = &dst.pixt<UINT32>(y, visarea.min_x);
		UINT16 *src16 = &src.pixt<UINT16>(y, visarea.min_x);

		for (x = visarea.min_x; x < visarea.max_x; x++)
		{
			UINT16 pix = *src16++;
			UINT32 color = ((pix & 0x7c00) << 9) | ((pix & 0x03e0) << 6) | ((pix & 0x001f) << 3);
			*dst32++ = color | ((color >> 5) & 0x070707);
		}
	}
}


static void convert_32_to_15(bitmap_t &src, bitmap_t &dst, const rectangle &visarea)
{
	int x, y;

	for (y = visarea.min_y; y < visarea.max_y; y++)
	{
		UINT16 *dst16 = &dst.pixt<UINT16>(y, visarea.min_x);
		UINT32 *src32 = &src.pixt<UINT32>(y, visarea.min_x);

		for (x = visarea.min_x; x < visarea.max_x; x++)
			*dst16++ = rgb_t(*src32++).as_rgb15();
	}
}

void screen_device::texture_set_scale_bitmap(const rectangle &visarea, UINT32 palettebase)
{
	int curbank = m_curbitmap;
	int scalebank = /* scale_bank_offset + */curbank;
	bitmap_t *target = &(bitmap_t &)m_bitmap[curbank];
	bitmap_t *dst;
	rectangle fixedvis;
	int width, height;

	width = visarea.max_x - visarea.min_x;
	height = visarea.max_y - visarea.min_y;

	fixedvis.min_x = 0;
	fixedvis.min_y = 0;
	fixedvis.max_x = width * scale_xsize;
	fixedvis.max_y = height * scale_ysize;

	// convert texture to 15 or 32 bit which scaler is capable of rendering
	switch (target->format())
	{
	case BITMAP_FORMAT_IND16:
		target = m_work_bitmap[0][curbank];

		if (scale_depth == 32)
			convert_palette_to_32((bitmap_t &)m_bitmap[curbank], *target, visarea, palettebase);
		else
			convert_palette_to_15((bitmap_t &)m_bitmap[curbank], *target, visarea, palettebase);

		break;

	case BITMAP_FORMAT_RGB32:
		if (scale_depth == 32)
			break;

		target = m_work_bitmap[0][curbank];
		convert_32_to_15((bitmap_t &)m_bitmap[curbank], *target, visarea);
		break;

	default:
		logerror("unknown texture format\n");
		return;
	}

	dst = m_scale_bitmap[curbank];
	if (scale_depth == 32)
	{
		UINT32 *src32 = &target->pixt<UINT32>(visarea.min_y, visarea.min_x);
		UINT32 *dst32 = &dst->pixt<UINT32>(0, 0);
		scale_perform_scale((UINT8 *)src32, (UINT8 *)dst32, target->rowpixels() * 4, dst->rowpixels() * 4, width, height, 32, m_scale_dirty[curbank], scalebank);
	}
	else
	{
		UINT16 *src16 = &target->pixt<UINT16>(visarea.min_y, visarea.min_x);
		UINT16 *dst16 = &m_work_bitmap[1][curbank]->pixt<UINT16>(0, 0);
		scale_perform_scale((UINT8 *)src16, (UINT8 *)dst16, target->rowpixels() * 2, dst->rowpixels() * 2, width, height, 15, m_scale_dirty[curbank], scalebank);
		convert_15_to_32(*m_work_bitmap[1][curbank], *dst, dst->cliprect());
	}
	m_scale_dirty[curbank] = 0;

	m_texture[curbank]->set_bitmap(*dst, fixedvis, TEXFORMAT_RGB32);
}
#endif /* USE_SCALE_EFFECTS */

//-------------------------------------------------
//  screen_device - constructor
//-------------------------------------------------

screen_device::screen_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, SCREEN, "Video Screen", tag, owner, clock, "screen", __FILE__),
		m_type(SCREEN_TYPE_RASTER),
		m_oldstyle_vblank_supplied(false),
		m_refresh(0),
		m_vblank(0),
		m_xoffset(0.0f),
		m_yoffset(0.0f),
		m_xscale(1.0f),
		m_yscale(1.0f),
		m_palette(*this),
		m_video_attributes(0),
		m_container(NULL),
		m_width(100),
		m_height(100),
		m_visarea(0, 99, 0, 99),
		m_curbitmap(0),
		m_curtexture(0),
		m_changed(true),
		m_last_partial_scan(0),
		m_frame_period(DEFAULT_FRAME_PERIOD.as_attoseconds()),
		m_scantime(1),
		m_pixeltime(1),
		m_vblank_period(0),
		m_vblank_start_time(attotime::zero),
		m_vblank_end_time(attotime::zero),
		m_vblank_begin_timer(NULL),
		m_vblank_end_timer(NULL),
		m_scanline0_timer(NULL),
		m_scanline_timer(NULL),
		m_frame_number(0),
		m_partial_updates_this_frame(0)
{
	m_unique_id = m_id_counter;
	m_id_counter++;
	memset(m_texture, 0, sizeof(m_texture));

#ifdef USE_SCALE_EFFECTS
	memset(m_scale_bitmap, 0, sizeof(m_scale_bitmap));
	memset(m_work_bitmap, 0, sizeof(m_work_bitmap));
	memset(m_scale_dirty, 0, sizeof(m_scale_dirty));
#endif /* USE_SCALE_EFFECTS */
}


//-------------------------------------------------
//  ~screen_device - destructor
//-------------------------------------------------

screen_device::~screen_device()
{
}


//-------------------------------------------------
//  static_set_type - configuration helper
//  to set the screen type
//-------------------------------------------------

void screen_device::static_set_type(device_t &device, screen_type_enum type)
{
	downcast<screen_device &>(device).m_type = type;
}


//-------------------------------------------------
//  static_set_raw - configuration helper
//  to set the raw screen parameters
//-------------------------------------------------

void screen_device::static_set_raw(device_t &device, UINT32 pixclock, UINT16 htotal, UINT16 hbend, UINT16 hbstart, UINT16 vtotal, UINT16 vbend, UINT16 vbstart)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_clock = pixclock;
	screen.m_refresh = HZ_TO_ATTOSECONDS(pixclock) * htotal * vtotal;
	screen.m_vblank = screen.m_refresh / vtotal * (vtotal - (vbstart - vbend));
	screen.m_width = htotal;
	screen.m_height = vtotal;
	screen.m_visarea.set(hbend, hbstart - 1, vbend, vbstart - 1);
}


//-------------------------------------------------
//  static_set_refresh - configuration helper
//  to set the refresh rate
//-------------------------------------------------

void screen_device::static_set_refresh(device_t &device, attoseconds_t rate)
{
	downcast<screen_device &>(device).m_refresh = rate;
}


//-------------------------------------------------
//  static_set_vblank_time - configuration helper
//  to set the VBLANK duration
//-------------------------------------------------

void screen_device::static_set_vblank_time(device_t &device, attoseconds_t time)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_vblank = time;
	screen.m_oldstyle_vblank_supplied = true;
}


//-------------------------------------------------
//  static_set_size - configuration helper to set
//  the width/height of the screen
//-------------------------------------------------

void screen_device::static_set_size(device_t &device, UINT16 width, UINT16 height)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_width = width;
	screen.m_height = height;
}


//-------------------------------------------------
//  static_set_visarea - configuration helper to
//  set the visible area of the screen
//-------------------------------------------------

void screen_device::static_set_visarea(device_t &device, INT16 minx, INT16 maxx, INT16 miny, INT16 maxy)
{
	downcast<screen_device &>(device).m_visarea.set(minx, maxx, miny, maxy);
}


//-------------------------------------------------
//  static_set_default_position - configuration
//  helper to set the default position and scale
//  factors for the screen
//-------------------------------------------------

void screen_device::static_set_default_position(device_t &device, double xscale, double xoffs, double yscale, double yoffs)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_xscale = xscale;
	screen.m_xoffset = xoffs;
	screen.m_yscale = yscale;
	screen.m_yoffset = yoffs;
}


//-------------------------------------------------
//  static_set_screen_update - set the legacy(?)
//  screen update callback in the device
//  configuration
//-------------------------------------------------

void screen_device::static_set_screen_update(device_t &device, screen_update_ind16_delegate callback)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_screen_update_ind16 = callback;
	screen.m_screen_update_rgb32 = screen_update_rgb32_delegate();
}

void screen_device::static_set_screen_update(device_t &device, screen_update_rgb32_delegate callback)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_screen_update_ind16 = screen_update_ind16_delegate();
	screen.m_screen_update_rgb32 = callback;
}


//-------------------------------------------------
//  static_set_screen_vblank - set the screen
//  VBLANK callback in the device configuration
//-------------------------------------------------

void screen_device::static_set_screen_vblank(device_t &device, screen_vblank_delegate callback)
{
	downcast<screen_device &>(device).m_screen_vblank = callback;
}


//-------------------------------------------------
//  static_set_palette - set the screen palette
//  configuration
//-------------------------------------------------

void screen_device::static_set_palette(device_t &device, const char *tag)
{
	downcast<screen_device &>(device).m_palette.set_tag(tag);
}


//-------------------------------------------------
//  static_set_video_attributes - set the screen
//  video attributes
//-------------------------------------------------

void screen_device::static_set_video_attributes(device_t &device, UINT32 flags)
{
	screen_device &screen = downcast<screen_device &>(device);
	screen.m_video_attributes = flags;
}
//-------------------------------------------------
//  device_validity_check - verify device
//  configuration
//-------------------------------------------------

void screen_device::device_validity_check(validity_checker &valid) const
{
	// sanity check dimensions
	if (m_width <= 0 || m_height <= 0)
		osd_printf_error(_("Invalid display dimensions\n"));

	// sanity check display area
	if (m_type != SCREEN_TYPE_VECTOR)
	{
		if (m_visarea.empty() || m_visarea.max_x >= m_width || m_visarea.max_y >= m_height)
			osd_printf_error(_("Invalid display area\n"));

		// sanity check screen formats
		if (m_screen_update_ind16.isnull() && m_screen_update_rgb32.isnull())
			osd_printf_error(_("Missing SCREEN_UPDATE function\n"));
	}

	// check for zero frame rate
	if (m_refresh == 0)
		osd_printf_error(_("Invalid (zero) refresh rate\n"));

	texture_format texformat = !m_screen_update_ind16.isnull() ? TEXFORMAT_PALETTE16 : TEXFORMAT_RGB32;
	if (m_palette == NULL && texformat == TEXFORMAT_PALETTE16)
		osd_printf_error(_("Screen does not have palette defined\n"));
	if (m_palette != NULL && texformat == TEXFORMAT_RGB32)
		osd_printf_warning(_("Screen does not need palette defined\n"));
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void screen_device::device_start()
{
	// bind our handlers
	m_screen_update_ind16.bind_relative_to(*owner());
	m_screen_update_rgb32.bind_relative_to(*owner());
	m_screen_vblank.bind_relative_to(*owner());

	// if we have a palette and it's not started, wait for it
	if (m_palette != NULL && !m_palette->started())
		throw device_missing_dependencies();

	// configure bitmap formats and allocate screen bitmaps
	texture_format texformat = !m_screen_update_ind16.isnull() ? TEXFORMAT_PALETTE16 : TEXFORMAT_RGB32;

	for (int index = 0; index < ARRAY_LENGTH(m_bitmap); index++)
	{
		m_bitmap[index].set_format(format(), texformat);
		register_screen_bitmap(m_bitmap[index]);
	}
	register_screen_bitmap(m_priority);

	// allocate raw textures
	m_texture[0] = machine().render().texture_alloc();
	m_texture[0]->set_osd_data((UINT64)((m_unique_id << 1) | 0));
	m_texture[1] = machine().render().texture_alloc();
	m_texture[1]->set_osd_data((UINT64)((m_unique_id << 1) | 1));

	// configure the default cliparea
	render_container::user_settings settings;
	m_container->get_user_settings(settings);
	settings.m_xoffset = m_xoffset;
	settings.m_yoffset = m_yoffset;
	settings.m_xscale = m_xscale;
	settings.m_yscale = m_yscale;
	m_container->set_user_settings(settings);

	// allocate the VBLANK timers
	m_vblank_begin_timer = timer_alloc(TID_VBLANK_START);
	m_vblank_end_timer = timer_alloc(TID_VBLANK_END);

	// allocate a timer to reset partial updates
	m_scanline0_timer = timer_alloc(TID_SCANLINE0);

	// allocate a timer to generate per-scanline updates
	if ((m_video_attributes & VIDEO_UPDATE_SCANLINE) != 0)
		m_scanline_timer = timer_alloc(TID_SCANLINE);

	// configure the screen with the default parameters
	configure(m_width, m_height, m_visarea, m_refresh);

	// reset VBLANK timing
	m_vblank_start_time = attotime::zero;
	m_vblank_end_time = attotime(0, m_vblank_period);

	// start the timer to generate per-scanline updates
	if ((m_video_attributes & VIDEO_UPDATE_SCANLINE) != 0)
		m_scanline_timer->adjust(time_until_pos(0));

	// create burn-in bitmap
	if (machine().options().burnin())
	{
		int width, height;
		if (sscanf(machine().options().snap_size(), "%dx%d", &width, &height) != 2 || width == 0 || height == 0)
			width = height = 300;
		m_burnin.allocate(width, height);
		m_burnin.fill(0);
	}

	// load the effect overlay
	const char *overname = machine().options().effect();
	if (overname != NULL && strcmp(overname, "none") != 0)
		load_effect_overlay(overname);

	// register items for saving
	save_item(NAME(m_width));
	save_item(NAME(m_height));
	save_item(NAME(m_visarea.min_x));
	save_item(NAME(m_visarea.min_y));
	save_item(NAME(m_visarea.max_x));
	save_item(NAME(m_visarea.max_y));
	save_item(NAME(m_last_partial_scan));
	save_item(NAME(m_frame_period));
	save_item(NAME(m_scantime));
	save_item(NAME(m_pixeltime));
	save_item(NAME(m_vblank_period));
	save_item(NAME(m_vblank_start_time));
	save_item(NAME(m_vblank_end_time));
	save_item(NAME(m_frame_number));
}


//-------------------------------------------------
//  device_stop - clean up before the machine goes
//  away
//-------------------------------------------------

void screen_device::device_stop()
{
	machine().render().texture_free(m_texture[0]);
	machine().render().texture_free(m_texture[1]);
	if (m_burnin.valid())
		finalize_burnin();
}


//-------------------------------------------------
//  device_post_load - device-specific update
//  after a save state is loaded
//-------------------------------------------------

void screen_device::device_post_load()
{
	realloc_screen_bitmaps();
#ifdef USE_SCALE_EFFECTS
	video_init_scale_effect();
#endif /* USE_SCALE_EFFECTS */
}


//-------------------------------------------------
//  device_timer - called whenever a device timer
//  fires
//-------------------------------------------------

void screen_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch (id)
	{
		// signal VBLANK start
		case TID_VBLANK_START:
			vblank_begin();
			break;

		// signal VBLANK end
		case TID_VBLANK_END:
			vblank_end();
			break;

		// first scanline
		case TID_SCANLINE0:
			reset_partial_updates();
			break;

		// subsequent scanlines when scanline updates are enabled
		case TID_SCANLINE:

			// force a partial update to the current scanline
			update_partial(param);

			// compute the next visible scanline
			param++;
			if (param > m_visarea.max_y)
				param = m_visarea.min_y;
			m_scanline_timer->adjust(time_until_pos(param), param);
			break;
	}
}


//-------------------------------------------------
//  configure - configure screen parameters
//-------------------------------------------------

void screen_device::configure(int width, int height, const rectangle &visarea, attoseconds_t frame_period)
{
	// validate arguments
	assert(width > 0);
	assert(height > 0);
	assert(visarea.min_x >= 0);
	assert(visarea.min_y >= 0);
//  assert(visarea.max_x < width);
//  assert(visarea.max_y < height);
	assert(m_type == SCREEN_TYPE_VECTOR || visarea.min_x < width);
	assert(m_type == SCREEN_TYPE_VECTOR || visarea.min_y < height);
	assert(frame_period > 0);

	// fill in the new parameters
	m_width = width;
	m_height = height;
	m_visarea = visarea;

	// reallocate bitmap if necessary
	realloc_screen_bitmaps();

#ifdef USE_SCALE_EFFECTS
	// init scale
	video_init_scale_effect();
#endif /* USE_SCALE_EFFECTS */

	// compute timing parameters
	m_frame_period = frame_period;
	m_scantime = frame_period / height;
	m_pixeltime = frame_period / (height * width);

	// if an old style VBLANK_TIME was specified in the MACHINE_CONFIG,
	// use it; otherwise calculate the VBLANK period from the visible area
	if (m_oldstyle_vblank_supplied)
		m_vblank_period = m_vblank;
	else
		m_vblank_period = m_scantime * (height - visarea.height());

	// we are now fully configured with the new parameters
	// and can safely call time_until_pos(), etc.

	// if the frame period was reduced so that we are now past the end of the frame,
	// call the VBLANK start timer now; otherwise, adjust it for the future
	attoseconds_t delta = (machine().time() - m_vblank_start_time).as_attoseconds();
	if (delta >= m_frame_period)
		vblank_begin();
	else
		m_vblank_begin_timer->adjust(time_until_vblank_start());

	// if we are on scanline 0 already, call the scanline 0 timer
	// by hand now; otherwise, adjust it for the future
	if (vpos() == 0)
		reset_partial_updates();
	else
		m_scanline0_timer->adjust(time_until_pos(0));

	// adjust speed if necessary
	machine().video().update_refresh_speed();
}


//-------------------------------------------------
//  reset_origin - reset the timing such that the
//  given (x,y) occurs at the current time
//-------------------------------------------------

void screen_device::reset_origin(int beamy, int beamx)
{
	// compute the effective VBLANK start/end times
	attotime curtime = machine().time();
	m_vblank_end_time = curtime - attotime(0, beamy * m_scantime + beamx * m_pixeltime);
	m_vblank_start_time = m_vblank_end_time - attotime(0, m_vblank_period);

	// if we are resetting relative to (0,0) == VBLANK end, call the
	// scanline 0 timer by hand now; otherwise, adjust it for the future
	if (beamy == 0 && beamx == 0)
		reset_partial_updates();
	else
		m_scanline0_timer->adjust(time_until_pos(0));

	// if we are resetting relative to (visarea.max_y + 1, 0) == VBLANK start,
	// call the VBLANK start timer now; otherwise, adjust it for the future
	if (beamy == ((m_visarea.max_y + 1) % m_height) && beamx == 0)
		vblank_begin();
	else
		m_vblank_begin_timer->adjust(time_until_vblank_start());
}


//-------------------------------------------------
//  realloc_screen_bitmaps - reallocate bitmaps
//  and textures as necessary
//-------------------------------------------------

void screen_device::realloc_screen_bitmaps()
{
	// doesn't apply for vector games
	if (m_type == SCREEN_TYPE_VECTOR)
		return;

	// determine effective size to allocate
	INT32 effwidth = MAX(m_width, m_visarea.max_x + 1);
	INT32 effheight = MAX(m_height, m_visarea.max_y + 1);

	// reize all registered screen bitmaps
	for (auto_bitmap_item *item = m_auto_bitmap_list.first(); item != NULL; item = item->next())
		item->m_bitmap.resize(effwidth, effheight);

	// re-set up textures
	if (m_palette != NULL)
	{
		m_bitmap[0].set_palette(m_palette->palette());
		m_bitmap[1].set_palette(m_palette->palette());
	}
	m_texture[0]->set_bitmap(m_bitmap[0], m_visarea, m_bitmap[0].texformat());
	m_texture[1]->set_bitmap(m_bitmap[1], m_visarea, m_bitmap[1].texformat());
}

#ifdef USE_SCALE_EFFECTS
//-------------------------------------------------
//  realloc_scale_bitmaps - reallocate scale
//  bitmaps as necessary
//-------------------------------------------------

void screen_device::realloc_scale_bitmaps()
{
	osd_printf_verbose("realloc_scale_bitmaps()\n");

	if (m_type == SCREEN_TYPE_VECTOR)
		return;

	int curwidth = 0, curheight = 0;
	int cur_scalewidth = 0, cur_scaleheight = 0, cur_xsize = 0, cur_ysize = 0;

	// bitmap has been alloc'd
	curwidth = m_bitmap[0].width();
	curheight = m_bitmap[0].height();

	// extract the current width/height from the scale_bitmap
	if (m_scale_bitmap[0] != NULL)
	{
		cur_scalewidth = m_scale_bitmap[0]->width();
		cur_scaleheight = m_scale_bitmap[0]->height();
	}

	// assign new x/y size
	scale_xsize = scale_effect.xsize;
	scale_ysize = scale_effect.ysize;

	scale_bank_offset = 0;

	// reallocate our bitmaps and textures
	if (cur_scalewidth != curwidth * scale_xsize || cur_scaleheight != curheight * scale_ysize)
	{
		int bank;

		for (bank = 0; bank < 2; bank++)
		{
			// free what we have currently
			if (m_scale_bitmap[bank] != NULL)
			{
				auto_free(machine(), m_scale_bitmap[bank]);
				m_scale_bitmap[bank] = NULL;
			}
			if (m_work_bitmap[0][bank] != NULL)
			{
				auto_free(machine(), m_work_bitmap[0][bank]);
				m_work_bitmap[0][bank] = NULL;
			}
			if (m_work_bitmap[1][bank] != NULL)
			{
				auto_free(machine(), m_work_bitmap[1][bank]);
				m_work_bitmap[1][bank] = NULL;
			}

			m_scale_dirty[bank] = 1;

			// compute new width/height
			cur_xsize = MAX(scale_xsize, cur_xsize);
			cur_ysize = MAX(scale_ysize, cur_ysize);

			// allocate scale_bitmaps
			m_scale_bitmap[bank] = auto_bitmap_rgb32_alloc(machine(), curwidth * scale_xsize, curheight * scale_ysize);
			if (use_work_bitmap)
			{
				m_work_bitmap[0][bank] = auto_bitmap_rgb32_alloc(machine(), curwidth, curheight);
				m_work_bitmap[1][bank] = auto_bitmap_rgb32_alloc(machine(), curwidth * scale_xsize, curheight * scale_ysize);
			}

			osd_printf_verbose("realloc_scale_bitmaps: %dx%d@%dbpp, workerbmp: %d \n", 
								curwidth * scale_xsize, 
								curheight * scale_ysize,
								scale_depth,
								use_work_bitmap
								);
		}
	}
	scale_bank_offset = 1;
}
#endif /* USE_SCALE_EFFECTS */


//-------------------------------------------------
//  set_visible_area - just set the visible area
//-------------------------------------------------

void screen_device::set_visible_area(int min_x, int max_x, int min_y, int max_y)
{
	rectangle visarea(min_x, max_x, min_y, max_y);
	assert(!visarea.empty());
	configure(m_width, m_height, visarea, m_frame_period);
}


//-------------------------------------------------
//  update_partial - perform a partial update from
//  the last scanline up to and including the
//  specified scanline
//-----------------------------------------------*/

bool screen_device::update_partial(int scanline)
{
	// validate arguments
	assert(scanline >= 0);

	LOG_PARTIAL_UPDATES(("Partial: update_partial(%s, %d): ", tag(), scanline));

	// these two checks only apply if we're allowed to skip frames
	if (!(m_video_attributes & VIDEO_ALWAYS_UPDATE))
	{
		// if skipping this frame, bail
		if (machine().video().skip_this_frame())
		{
			LOG_PARTIAL_UPDATES(("skipped due to frameskipping\n"));
			return FALSE;
		}

		// skip if this screen is not visible anywhere
		if (!machine().render().is_live(*this))
		{
			LOG_PARTIAL_UPDATES(("skipped because screen not live\n"));
			return FALSE;
		}
	}

	// skip if we already rendered this line
	if (scanline < m_last_partial_scan)
	{
		LOG_PARTIAL_UPDATES(("skipped because line was already rendered\n"));
		return false;
	}

	// set the range of scanlines to render
	rectangle clip = m_visarea;
	if (m_last_partial_scan > clip.min_y)
		clip.min_y = m_last_partial_scan;
	if (scanline < clip.max_y)
		clip.max_y = scanline;

	// skip if entirely outside of visible area
	if (clip.min_y > clip.max_y)
	{
		LOG_PARTIAL_UPDATES(("skipped because outside of visible area\n"));
		return false;
	}

	// otherwise, render
	LOG_PARTIAL_UPDATES(("updating %d-%d\n", clip.min_y, clip.max_y));
	g_profiler.start(PROFILER_VIDEO);

	UINT32 flags = UPDATE_HAS_NOT_CHANGED;
	screen_bitmap &curbitmap = m_bitmap[m_curbitmap];
	switch (curbitmap.format())
	{
		default:
		case BITMAP_FORMAT_IND16:   flags = m_screen_update_ind16(*this, curbitmap.as_ind16(), clip);   break;
		case BITMAP_FORMAT_RGB32:   flags = m_screen_update_rgb32(*this, curbitmap.as_rgb32(), clip);   break;
	}

	m_partial_updates_this_frame++;
	g_profiler.stop();

	// if we modified the bitmap, we have to commit
	m_changed |= ~flags & UPDATE_HAS_NOT_CHANGED;

	// remember where we left off
	m_last_partial_scan = scanline + 1;
	return true;
}


//-------------------------------------------------
//  update_now - perform an update from the last
//  beam position up to the current beam position
//-------------------------------------------------

void screen_device::update_now()
{
	int current_vpos = vpos();
	int current_hpos = hpos();

	// since we can currently update only at the scanline
	// level, we are trying to do the right thing by
	// updating including the current scanline, only if the
	// beam is past the halfway point horizontally.
	// If the beam is in the first half of the scanline,
	// we only update up to the previous scanline.
	// This minimizes the number of pixels that might be drawn
	// incorrectly until we support a pixel level granularity
	if (current_hpos < (m_width / 2) && current_vpos > 0)
		current_vpos = current_vpos - 1;

	update_partial(current_vpos);
}


//-------------------------------------------------
//  reset_partial_updates - reset the partial
//  updating state
//-------------------------------------------------

void screen_device::reset_partial_updates()
{
	m_last_partial_scan = 0;
	m_partial_updates_this_frame = 0;
	m_scanline0_timer->adjust(time_until_pos(0));
}


//-------------------------------------------------
//  vpos - returns the current vertical position
//  of the beam
//-------------------------------------------------

int screen_device::vpos() const
{
	attoseconds_t delta = (machine().time() - m_vblank_start_time).as_attoseconds();
	int vpos;

	// round to the nearest pixel
	delta += m_pixeltime / 2;

	// compute the v position relative to the start of VBLANK
	vpos = delta / m_scantime;

	// adjust for the fact that VBLANK starts at the bottom of the visible area
	return (m_visarea.max_y + 1 + vpos) % m_height;
}


//-------------------------------------------------
//  hpos - returns the current horizontal position
//  of the beam
//-------------------------------------------------

int screen_device::hpos() const
{
	attoseconds_t delta = (machine().time() - m_vblank_start_time).as_attoseconds();

	// round to the nearest pixel
	delta += m_pixeltime / 2;

	// compute the v position relative to the start of VBLANK
	int vpos = delta / m_scantime;

	// subtract that from the total time
	delta -= vpos * m_scantime;

	// return the pixel offset from the start of this scanline
	return delta / m_pixeltime;
}


//-------------------------------------------------
//  time_until_pos - returns the amount of time
//  remaining until the beam is at the given
//  hpos,vpos
//-------------------------------------------------

attotime screen_device::time_until_pos(int vpos, int hpos) const
{
	// validate arguments
	assert(vpos >= 0);
	assert(hpos >= 0);

	// since we measure time relative to VBLANK, compute the scanline offset from VBLANK
	vpos += m_height - (m_visarea.max_y + 1);
	vpos %= m_height;

	// compute the delta for the given X,Y position
	attoseconds_t targetdelta = (attoseconds_t)vpos * m_scantime + (attoseconds_t)hpos * m_pixeltime;

	// if we're past that time (within 1/2 of a pixel), head to the next frame
	attoseconds_t curdelta = (machine().time() - m_vblank_start_time).as_attoseconds();
	if (targetdelta <= curdelta + m_pixeltime / 2)
		targetdelta += m_frame_period;
	while (targetdelta <= curdelta)
		targetdelta += m_frame_period;

	// return the difference
	return attotime(0, targetdelta - curdelta);
}


//-------------------------------------------------
//  time_until_vblank_end - returns the amount of
//  time remaining until the end of the current
//  VBLANK (if in progress) or the end of the next
//  VBLANK
//-------------------------------------------------

attotime screen_device::time_until_vblank_end() const
{
	// if we are in the VBLANK region, compute the time until the end of the current VBLANK period
	attotime target_time = m_vblank_end_time;
	if (!vblank())
		target_time += attotime(0, m_frame_period);
	return target_time - machine().time();
}


//-------------------------------------------------
//  register_vblank_callback - registers a VBLANK
//  callback
//-------------------------------------------------

void screen_device::register_vblank_callback(vblank_state_delegate vblank_callback)
{
	// validate arguments
	assert(!vblank_callback.isnull());

	// check if we already have this callback registered
	callback_item *item;
	for (item = m_callback_list.first(); item != NULL; item = item->next())
		if (item->m_callback == vblank_callback)
			break;

	// if not found, register
	if (item == NULL)
		m_callback_list.append(*global_alloc(callback_item(vblank_callback)));
}


//-------------------------------------------------
//  register_screen_bitmap - registers a bitmap
//  that should track the screen size
//-------------------------------------------------

void screen_device::register_screen_bitmap(bitmap_t &bitmap)
{
	// append to the list
	m_auto_bitmap_list.append(*global_alloc(auto_bitmap_item(bitmap)));

	// if allocating now, just do it
	bitmap.allocate(width(), height());
	if (m_palette != NULL)
		bitmap.set_palette(m_palette->palette());
}


//-------------------------------------------------
//  vblank_begin - call any external callbacks to
//  signal the VBLANK period has begun
//-------------------------------------------------

void screen_device::vblank_begin()
{
	// reset the starting VBLANK time
	m_vblank_start_time = machine().time();
	m_vblank_end_time = m_vblank_start_time + attotime(0, m_vblank_period);

	// if this is the primary screen and we need to update now
	if (this == machine().first_screen() && !(m_video_attributes & VIDEO_UPDATE_AFTER_VBLANK))
		machine().video().frame_update();

	// call the screen specific callbacks
	for (callback_item *item = m_callback_list.first(); item != NULL; item = item->next())
		item->m_callback(*this, true);
	if (!m_screen_vblank.isnull())
		m_screen_vblank(*this, true);

	// reset the VBLANK start timer for the next frame
	m_vblank_begin_timer->adjust(time_until_vblank_start());

	// if no VBLANK period, call the VBLANK end callback immediately, otherwise reset the timer
	if (m_vblank_period == 0)
		vblank_end();
	else
		m_vblank_end_timer->adjust(time_until_vblank_end());
}


//-------------------------------------------------
//  vblank_end - call any external callbacks to
//  signal the VBLANK period has ended
//-------------------------------------------------

void screen_device::vblank_end()
{
	// call the screen specific callbacks
	for (callback_item *item = m_callback_list.first(); item != NULL; item = item->next())
		item->m_callback(*this, false);
	if (!m_screen_vblank.isnull())
		m_screen_vblank(*this, false);

	// if this is the primary screen and we need to update now
	if (this == machine().first_screen() && (m_video_attributes & VIDEO_UPDATE_AFTER_VBLANK))
		machine().video().frame_update();

	// increment the frame number counter
	m_frame_number++;
}


//-------------------------------------------------
//  update_quads - set up the quads for this
//  screen
//-------------------------------------------------

bool screen_device::update_quads()
{
	// only update if live
	if (machine().render().is_live(*this))
	{
		// only update if empty and not a vector game; otherwise assume the driver did it directly
		if (m_type != SCREEN_TYPE_VECTOR && (m_video_attributes & VIDEO_SELF_RENDER) == 0)
		{
			// if we're not skipping the frame and if the screen actually changed, then update the texture
			if (!machine().video().skip_this_frame() && m_changed)
			{
#ifdef USE_SCALE_EFFECTS
				if (scale_effect.effect > 0)
					texture_set_scale_bitmap(m_visarea, 0);
				else
#endif /* USE_SCALE_EFFECTS */
					m_texture[m_curbitmap]->set_bitmap(m_bitmap[m_curbitmap], m_visarea, m_bitmap[m_curbitmap].texformat());
				m_curtexture = m_curbitmap;
				m_curbitmap = 1 - m_curbitmap;
			}

			// create an empty container with a single quad
			m_container->empty();
			m_container->add_quad(0.0f, 0.0f, 1.0f, 1.0f, rgb_t(0xff,0xff,0xff,0xff), m_texture[m_curtexture], PRIMFLAG_BLENDMODE(BLENDMODE_NONE) | PRIMFLAG_SCREENTEX(1));
		}
	}

	// reset the screen changed flags
	bool result = m_changed;
	m_changed = false;
	return result;
}


//-------------------------------------------------
//  update_burnin - update the burnin bitmap
//-------------------------------------------------

void screen_device::update_burnin()
{
#undef rand
	if (!m_burnin.valid())
		return;

	screen_bitmap &curbitmap = m_bitmap[m_curtexture];
	if (!curbitmap.valid())
		return;

	int srcwidth = curbitmap.width();
	int srcheight = curbitmap.height();
	int dstwidth = m_burnin.width();
	int dstheight = m_burnin.height();
	int xstep = (srcwidth << 16) / dstwidth;
	int ystep = (srcheight << 16) / dstheight;
	int xstart = ((UINT32)rand() % 32767) * xstep / 32767;
	int ystart = ((UINT32)rand() % 32767) * ystep / 32767;
	int srcx, srcy;
	int x, y;

	switch (curbitmap.format())
	{
		default:
		case BITMAP_FORMAT_IND16:
		{
			// iterate over rows in the destination
			bitmap_ind16 &srcbitmap = curbitmap.as_ind16();
			for (y = 0, srcy = ystart; y < dstheight; y++, srcy += ystep)
			{
				UINT64 *dst = &m_burnin.pix64(y);
				const UINT16 *src = &srcbitmap.pix16(srcy >> 16);
				const rgb_t *palette = m_palette->palette()->entry_list_adjusted();
				for (x = 0, srcx = xstart; x < dstwidth; x++, srcx += xstep)
				{
					rgb_t pixel = palette[src[srcx >> 16]];
					dst[x] += pixel.g() + pixel.r() + pixel.b();
				}
			}
			break;
		}

		case BITMAP_FORMAT_RGB32:
		{
			// iterate over rows in the destination
			bitmap_rgb32 &srcbitmap = curbitmap.as_rgb32();
			for (y = 0, srcy = ystart; y < dstheight; y++, srcy += ystep)
			{
				UINT64 *dst = &m_burnin.pix64(y);
				const UINT32 *src = &srcbitmap.pix32(srcy >> 16);
				for (x = 0, srcx = xstart; x < dstwidth; x++, srcx += xstep)
				{
					rgb_t pixel = src[srcx >> 16];
					dst[x] += pixel.g() + pixel.r() + pixel.b();
				}
			}
			break;
		}
	}
}


//-------------------------------------------------
//  finalize_burnin - finalize the burnin bitmap
//-------------------------------------------------

void screen_device::finalize_burnin()
{
	if (!m_burnin.valid())
		return;

	// compute the scaled visible region
	rectangle scaledvis;
	scaledvis.min_x = m_visarea.min_x * m_burnin.width() / m_width;
	scaledvis.max_x = m_visarea.max_x * m_burnin.width() / m_width;
	scaledvis.min_y = m_visarea.min_y * m_burnin.height() / m_height;
	scaledvis.max_y = m_visarea.max_y * m_burnin.height() / m_height;

	// wrap a bitmap around the memregion we care about
	bitmap_argb32 finalmap(scaledvis.width(), scaledvis.height());
	int srcwidth = m_burnin.width();
	int srcheight = m_burnin.height();
	int dstwidth = finalmap.width();
	int dstheight = finalmap.height();
	int xstep = (srcwidth << 16) / dstwidth;
	int ystep = (srcheight << 16) / dstheight;

	// find the maximum value
	UINT64 minval = ~(UINT64)0;
	UINT64 maxval = 0;
	for (int y = 0; y < srcheight; y++)
	{
		UINT64 *src = &m_burnin.pix64(y);
		for (int x = 0; x < srcwidth; x++)
		{
			minval = MIN(minval, src[x]);
			maxval = MAX(maxval, src[x]);
		}
	}

	if (minval == maxval)
		return;

	// now normalize and convert to RGB
	for (int y = 0, srcy = 0; y < dstheight; y++, srcy += ystep)
	{
		UINT64 *src = &m_burnin.pix64(srcy >> 16);
		UINT32 *dst = &finalmap.pix32(y);
		for (int x = 0, srcx = 0; x < dstwidth; x++, srcx += xstep)
		{
			int brightness = (UINT64)(maxval - src[srcx >> 16]) * 255 / (maxval - minval);
			dst[x] = rgb_t(0xff, brightness, brightness, brightness);
		}
	}

	// write the final PNG

	// compute the name and create the file
	emu_file file(machine().options().snapshot_directory(), OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);
	file_error filerr = file.open(machine().basename(), PATH_SEPARATOR "burnin-", this->tag()+1, ".png") ;
	if (filerr == FILERR_NONE)
	{
		png_info pnginfo = { 0 };
//      png_error pngerr;
		char text[256];

		// add two text entries describing the image
		sprintf(text,"%s %s", emulator_info::get_appname(), build_version);
		png_add_text(&pnginfo, "Software", text);
		sprintf(text, "%s %s", machine().system().manufacturer, machine().system().description);
		png_add_text(&pnginfo, "System", text);

		// now do the actual work
		png_write_bitmap(file, &pnginfo, finalmap, 0, NULL);

		// free any data allocated
		png_free(&pnginfo);
	}
}


//-------------------------------------------------
//  finalize_burnin - finalize the burnin bitmap
//-------------------------------------------------

void screen_device::load_effect_overlay(const char *filename)
{
	// ensure that there is a .png extension
	astring fullname(filename);
	int extension = fullname.rchr(0, '.');
	if (extension != -1)
		fullname.del(extension, -1);
	fullname.cat(".png");

	// load the file
	emu_file file(machine().options().art_path(), OPEN_FLAG_READ);
	render_load_png(m_screen_overlay_bitmap, file, NULL, fullname);
	if (m_screen_overlay_bitmap.valid())
		m_container->set_overlay(&m_screen_overlay_bitmap);
	else
		osd_printf_warning(_("Unable to load effect PNG file '%s'\n"), fullname.cstr());
}