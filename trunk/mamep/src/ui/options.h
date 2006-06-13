/***************************************************************************

  M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
  Win32 Portions Copyright (C) 1997-2003 Michael Soderstrom and Chris Kirmse

  This file is part of MAME32, and may only be used, modified and
  distributed under the terms of the MAME license, in "readme.txt".
  By continuing to use, modify or distribute this file you indicate
  that you have read the license and understand and accept it fully.

 ***************************************************************************/

#ifndef OPTIONS_H
#define OPTIONS_H

#include "osd_cpu.h"
#include "input.h" /* for input_seq definition */

#define MAX_SYSTEM_BIOS		8
#define MAX_GAMEDESC 256


enum 
{
	COLUMN_GAMES = 0,
	COLUMN_ROMS,
	COLUMN_SAMPLES,
	COLUMN_DIRECTORY,
	COLUMN_TYPE,
	COLUMN_TRACKBALL,
	COLUMN_PLAYED,
	COLUMN_MANUFACTURER,
	COLUMN_YEAR,
	COLUMN_CLONE,
	COLUMN_SRCDRIVERS,
	COLUMN_PLAYTIME,
	COLUMN_MAX
};


// can't be the same as the audit_verify_roms() results, listed in audit.h
enum
{
	UNKNOWN	= -1
};

enum
{
	SPLITTER_LEFT = 0,
	SPLITTER_RIGHT,
	SPLITTER_MAX
};

typedef struct
{
	int x, y, width, height;
} AREA;

typedef struct
{
	char *seq_string;	/* KEYCODE_LALT KEYCODE_A, etc... */
	input_seq is;		/* sequence definition in MAME's internal keycodes */
} KeySeq;

typedef struct
{
//
// CORE VIDEO OPTIONS
//
	BOOL	rotate;
	BOOL	ror;
	BOOL	rol;
	BOOL	autoror;
	BOOL	autorol;
	BOOL	flipx;
	BOOL	flipy;
	float	brightness;
	float	pause_brightness;
#ifdef USE_SCALE_EFFECTS
	char*  scale_effect;
#endif /* USE_SCALE_EFFECTS */
//
// CORE VECTOR OPTIONS
//
	BOOL	antialias;
	float	beam;
	float	flicker;
	float	intensity;
//
// CORE SOUND OPTIONS
//
	BOOL	sound;
	int	samplerate;
	BOOL	samples;
	int	volume;
#ifdef USE_VOLUME_AUTO_ADJUST
	BOOL	volume_adjust;
#endif /* USE_VOLUME_AUTO_ADJUST */
	int	audio_latency;
	char*	wavwrite;
//
// CORE MISC OPTIONS
//
	char*	bios;
	BOOL	cheat;
	BOOL	skip_gameinfo;
	BOOL	artwork;
	BOOL	use_backdrops;
	BOOL	use_overlays;
	BOOL	use_bezels;
#ifdef USE_IPS
	char*	ips;
#endif /* USE_IPS */
	BOOL	disable_second_monitor;
	BOOL	confirm_quit;
#ifdef AUTO_PAUSE_PLAYBACK
	BOOL	auto_pause_playback;
#endif /* AUTO_PAUSE_PLAYBACK */
#if (HAS_M68000 || HAS_M68008 || HAS_M68010 || HAS_M68EC020 || HAS_M68020 || HAS_M68040)
	int	m68k_core;
#endif /* (HAS_M68000 || HAS_M68008 || HAS_M68010 || HAS_M68EC020 || HAS_M68020 || HAS_M68040) */
#ifdef TRANS_UI
	BOOL	use_trans_ui;
	int	ui_transparency;
#endif /* TRANS_UI */
//
// CORE STATE/PLAYBACK OPTIONS
//
	char*	playback;
	char*	record;
	char*	state;
	BOOL	autosave;
//
// CORE DEBUGGING OPTIONS
//
	BOOL	log;
	BOOL	oslog;
	BOOL	verbose;
//
// CORE CONFIGURATION OPTIONS
//
	BOOL	readconfig;
//
// INPUT DEVICE OPTIONS
//
	BOOL	mouse;
	BOOL	joystick;
	BOOL	lightgun;
	BOOL	dual_lightgun;
	BOOL	offscreen_reload;
	BOOL	steadykey;
	BOOL	keyboard_leds;
	char*	led_mode;
	float	a2d_deadzone;
	char*	ctrlr;
#ifdef USE_JOY_MOUSE_MOVE
	BOOL	stickpoint;
#endif /* USE_JOY_MOUSE_MOVE */
#ifdef JOYSTICK_ID
	int	joyid1;
	int	joyid2;
	int	joyid3;
	int	joyid4;
	int	joyid5;
	int	joyid6;
	int	joyid7;
	int	joyid8;
#endif /* JOYSTICK_ID */
	char*	paddle_device;
	char*	adstick_device;
	char*	pedal_device;
	char*	dial_device;
	char*	trackball_device;
	char*	lightgun_device;
	char*	digital;
//
// PERFORMANCE OPTIONS
//
	BOOL	autoframeskip;
	int	frameskip;
	BOOL	throttle;
	BOOL	sleep;
	BOOL	rdtsc;
	int	priority;
//
// MISC VIDEO OPTIONS
//
	int	frames_to_run;
	char*	mngwrite;
//
// GLOBAL VIDEO OPTIONS
//
	BOOL	window;
	BOOL	maximize;
	int	numscreens;
	char*	extra_layout;
//
// PER-WINDOW VIDEO OPTIONS
//
	char*	screen0;
	char*	aspect0;
	char*	resolution0;
	char*	view0;
	char*	screen1;
	char*	aspect1;
	char*	resolution1;
	char*	view1;
	char*	screen2;
	char*	aspect2;
	char*	resolution2;
	char*	view2;
	char*	screen3;
	char*	aspect3;
	char*	resolution3;
	char*	view3;
//
// DIRECTX VIDEO OPTIONS
//
#ifndef NEW_RENDER
	BOOL	ddraw;
#endif
	BOOL	direct3d;
	int	d3dversion;
	BOOL	waitvsync;
	BOOL	syncrefresh;
	BOOL	triplebuffer;
	BOOL	switchres;
	BOOL	filter;
	int     prescale;
	float	full_screen_gamma;

#ifndef NEW_RENDER
	BOOL	hwstretch;
	char*	cleanstretch;
	int	refresh;
	BOOL	scanlines;
	BOOL	switchbpp;
	BOOL	keepaspect;
	BOOL	matchrefresh;
	char*	effect;
	float	gamma;

	int	zoom;
	BOOL	d3dtexmanage;

	int	d3dfeedback;
	int	d3dscan;
	BOOL	d3deffectrotate;
	char*	d3dprescale;
	char*	d3deffect;
#endif

} options_type;

// List of artwork types to display in the screen shot area
enum
{
	// these must match array of strings image_tabs_long_name in options.c
	// if you add new Tabs, be sure to also add them to the ComboBox init in dialogs.c
	TAB_SCREENSHOT = 0,
	TAB_FLYER,
	TAB_CABINET,
	TAB_MARQUEE,
	TAB_TITLE,
	TAB_CONTROL_PANEL,
	TAB_HISTORY,
#ifdef STORY_DATAFILE
	TAB_STORY,
#endif /* STORY_DATAFILE */

	MAX_TAB_TYPES,
	BACKGROUND,
	TAB_ALL,
#ifdef USE_IPS
	TAB_NONE,
	TAB_IPS
#else /* USE_IPS */
	TAB_NONE
#endif /* USE_IPS */
};
// Because we have added the Options after MAX_TAB_TYPES, we have to subtract 3 here
// (that's how many options we have after MAX_TAB_TYPES)
#define TAB_SUBTRACT 3


/*----------------------------------------*/
void OptionsInit(void);
void OptionsExit(void);

void FreeGameOptions(options_type *o);
void CopyGameOptions(const options_type *source, options_type *dest);

BOOL FolderHasVector(const char *name);
options_type* GetFolderOptions(const char *name);
options_type* GetDefaultOptions(void);
options_type* GetVectorOptions(void);
options_type* GetSourceOptions(int driver_index);
options_type* GetParentOptions(int driver_index);
options_type* GetGameOptions(int driver_index);

BOOL GetGameUsesDefaults(int driver_index);
void SetGameUsesDefaults(int driver_index, BOOL use_defaults);
BOOL GetFolderUsesDefaults(const char *name);
void SetFolderUsesDefaults(const char *name, BOOL use_defaults);

const char *GetUnifiedFolder(int driver_index);
int GetUnifiedDriver(const char *name);

const game_driver *GetSystemBiosInfo(int bios_index);
const char *GetDefaultBios(int bios_index);
void SetDefaultBios(int bios_index, const char *value);

void SaveOptions(void);
void SaveDefaultOptions(void);
void SaveFolderOptions(const char *name);
void SaveGameOptions(int driver_index);

void ResetGUI(void);
void ResetGameDefaults(void);
void ResetAllGameOptions(void);
void ResetGameOptions(int driver_index);


/*----------------------------------------*/
char * GetVersionString(void);

const char * GetImageTabLongName(int tab_index);
const char * GetImageTabShortName(int tab_index);

void SetViewMode(int val);
int  GetViewMode(void);

void SetGameCheck(BOOL game_check);
BOOL GetGameCheck(void);

void SetVersionCheck(BOOL version_check);
BOOL GetVersionCheck(void);

void SetJoyGUI(BOOL joygui);
BOOL GetJoyGUI(void);

void SetKeyGUI(BOOL keygui);
BOOL GetKeyGUI(void);

void SetCycleScreenshot(int cycle_screenshot);
int GetCycleScreenshot(void);

void SetStretchScreenShotLarger(BOOL stretch);
BOOL GetStretchScreenShotLarger(void);

void SetScreenshotBorderSize(int size);
int GetScreenshotBorderSize(void);

void SetScreenshotBorderColor(COLORREF uColor);
COLORREF GetScreenshotBorderColor(void);

void SetFilterInherit(BOOL inherit);
BOOL GetFilterInherit(void);

void SetOffsetClones(BOOL offset);
BOOL GetOffsetClones(void);

void SetGameCaption(BOOL caption);
BOOL GetGameCaption(void);

void SetBroadcast(BOOL broadcast);
BOOL GetBroadcast(void);

void SetRandomBackground(BOOL random_bg);
BOOL GetRandomBackground(void);

void SetSavedFolderID(UINT val);
UINT GetSavedFolderID(void);

void SetShowScreenShot(BOOL val);
BOOL GetShowScreenShot(void);

void SetShowFolderList(BOOL val);
BOOL GetShowFolderList(void);

BOOL GetShowFolder(int folder);
void SetShowFolder(int folder, BOOL show);


void SetShowStatusBar(BOOL val);
BOOL GetShowStatusBar(void);

void SetShowToolBar(BOOL val);
BOOL GetShowToolBar(void);

void SetShowTabCtrl(BOOL val);
BOOL GetShowTabCtrl(void);

void SetCurrentTab(const char *shortname);
const char *GetCurrentTab(void);

void SetDefaultGame(const char *name);
const char *GetDefaultGame(void);

void SetWindowArea(AREA *area);
void GetWindowArea(AREA *area);

void SetWindowState(UINT state);
UINT GetWindowState(void);

void SetColumnWidths(int widths[]);
void GetColumnWidths(int widths[]);

void SetColumnOrder(int order[]);
void GetColumnOrder(int order[]);

void SetColumnShown(int shown[]);
void GetColumnShown(int shown[]);

void SetSplitterPos(int splitterId, int pos);
int  GetSplitterPos(int splitterId);

void SetCustomColor(int iIndex, COLORREF uColor);
COLORREF GetCustomColor(int iIndex);

void SetListFont(LOGFONTA *font);
void GetListFont(LOGFONTA *font);

DWORD GetFolderFlags(const char *folderName);
void  SetFolderFlags(const char *folderName, DWORD dwFlags);

void SetUseBrokenIcon(BOOL use_broken_icon);
BOOL GetUseBrokenIcon(void);

void SetListFontColor(COLORREF uColor);
COLORREF GetListFontColor(void);

void SetListCloneColor(COLORREF uColor);
COLORREF GetListCloneColor(void);

void SetListBrokenColor(COLORREF uColor);
COLORREF GetListBrokenColor(void);

int GetHistoryTab(void);
void SetHistoryTab(int tab, BOOL show);

int GetShowTab(int tab);
void SetShowTab(int tab, BOOL show);
BOOL AllowedToSetShowTab(int tab, BOOL show);

void SetSortColumn(int column);
int  GetSortColumn(void);

void SetSortReverse(BOOL reverse);
BOOL GetSortReverse(void);

#ifdef USE_SHOW_SPLASH_SCREEN
void SetDisplaySplashScreen(BOOL val);
BOOL GetDisplaySplashScreen(void);
#endif /* USE_SHOW_SPLASH_SCREEN */

int GetRomAuditResults(int driver_index);
void SetRomAuditResults(int driver_index, int audit_results);

int GetSampleAuditResults(int driver_index);
void SetSampleAuditResults(int driver_index, int audit_results);

void IncrementPlayCount(int driver_index);
int GetPlayCount(int driver_index);
void ResetPlayCount(int driver_index);

void IncrementPlayTime(int driver_index, int playtime);
int GetPlayTime(int driver_index);
void GetTextPlayTime(int driver_index, char *buf);
void ResetPlayTime(int driver_index);

char* GetExecCommand(void);
void SetExecCommand(char* cmd);

int GetExecWait(void);
void SetExecWait(int wait);

BOOL GetHideMouseOnStartup(void);
void SetHideMouseOnStartup(BOOL hide);

BOOL GetRunFullScreen(void);
void SetRunFullScreen(BOOL fullScreen);


/*----------------------------------------*/
const char* GetRomDirs(void);
void SetRomDirs(const char* paths);

const char* GetSampleDirs(void);
void  SetSampleDirs(const char* paths);

const char* GetIniDir(void);
void  SetIniDir(const char* path);

const char* GetCfgDir(void);
void SetCfgDir(const char* path);

const char* GetNvramDir(void);
void SetNvramDir(const char* path);

const char* GetMemcardDir(void);
void SetMemcardDir(const char* path);

const char* GetInpDir(void);
void SetInpDir(const char* path);

const char* GetHiDir(void);
void SetHiDir(const char* path);

const char* GetStateDir(void);
void SetStateDir(const char* path);

const char* GetArtDir(void);
void SetArtDir(const char* path);

const char* GetImgDir(void);
void SetImgDir(const char* path);

const char* GetDiffDir(void);
void SetDiffDir(const char* path);

const char* GetCtrlrDir(void);
void SetCtrlrDir(const char* path);

const char* GetCommentDir(void);
void SetCommentDir(const char* path);

#ifdef USE_IPS
const char *GetPatchDir(void);
void SetPatchDir(const char *path);
#endif /* USE_IPS */

const char *GetLangDir(void);
void SetLangDir(const char *path);

const char* GetCheatFile(void);
void SetCheatFile(const char*);

const char* GetHistoryFile(void);
void SetHistoryFile(const char*);

#ifdef STORY_DATAFILE
const char* GetStoryFile(void);
void SetStoryFile(const char*);

#endif /* STORY_DATAFILE */
const char* GetMAMEInfoFile(void);
void SetMAMEInfoFile(const char*);

const char* GetHiscoreFile(void);
void SetHiscoreFile(const char*);

#ifdef UI_COLOR_DISPLAY
const char *GetUIPaletteString(int n);
void SetUIPaletteString(int n, const char *s);
#endif /* UI_COLOR_DISPLAY */

int GetLangcode(void);
void SetLangcode(int langcode);

BOOL UseLangList(void);
void SetUseLangList(BOOL is_use);


/*----------------------------------------*/
const char* GetFlyerDir(void);
void SetFlyerDir(const char* path);

const char* GetCabinetDir(void);
void SetCabinetDir(const char* path);

const char* GetMarqueeDir(void);
void SetMarqueeDir(const char* path);

const char* GetTitlesDir(void);
void SetTitlesDir(const char* path);

const char * GetControlPanelDir(void);
void SetControlPanelDir(const char *path);

const char *GetIconsDir(void);
void SetIconsDir(const char *path);

const char *GetBgDir(void);
void SetBgDir(const char *path);

const char *GetFolderDir(void);
void SetFolderDir(const char *path);

#ifdef USE_VIEW_PCBINFO
const char* GetPcbinfoDir(void);
void SetPcbinfoDir(const char* path);
#endif /* USE_VIEW_PCBINFO */


/*----------------------------------------*/
// Keyboard control of ui
input_seq *Get_ui_key_up(void);
input_seq *Get_ui_key_down(void);
input_seq *Get_ui_key_left(void);
input_seq *Get_ui_key_right(void);
input_seq *Get_ui_key_start(void);
input_seq *Get_ui_key_pgup(void);
input_seq *Get_ui_key_pgdwn(void);
input_seq *Get_ui_key_home(void);
input_seq *Get_ui_key_end(void);
input_seq *Get_ui_key_ss_change(void);
input_seq *Get_ui_key_history_up(void);
input_seq *Get_ui_key_history_down(void);

input_seq *Get_ui_key_context_filters(void);
input_seq *Get_ui_key_select_random(void);
input_seq *Get_ui_key_game_audit(void);
input_seq *Get_ui_key_game_properties(void);
input_seq *Get_ui_key_help_contents(void);
input_seq *Get_ui_key_update_gamelist(void);
input_seq *Get_ui_key_view_folders(void);
input_seq *Get_ui_key_view_fullscreen(void);
input_seq *Get_ui_key_view_pagetab(void);
input_seq *Get_ui_key_view_picture_area(void);
input_seq *Get_ui_key_view_status(void);
input_seq *Get_ui_key_view_toolbars(void);

input_seq *Get_ui_key_view_tab_cabinet(void);
input_seq *Get_ui_key_view_tab_cpanel(void);
input_seq *Get_ui_key_view_tab_flyer(void);
input_seq *Get_ui_key_view_tab_history(void);
#ifdef STORY_DATAFILE
input_seq *Get_ui_key_view_tab_story(void);
#endif /* STORY_DATAFILE */
input_seq *Get_ui_key_view_tab_marquee(void);
input_seq *Get_ui_key_view_tab_screenshot(void);
input_seq *Get_ui_key_view_tab_title(void);
input_seq *Get_ui_key_quit(void);


int GetUIJoyUp(int joycodeIndex);
void SetUIJoyUp(int joycodeIndex, int val);

int GetUIJoyDown(int joycodeIndex);
void SetUIJoyDown(int joycodeIndex, int val);

int GetUIJoyLeft(int joycodeIndex);
void SetUIJoyLeft(int joycodeIndex, int val);

int GetUIJoyRight(int joycodeIndex);
void SetUIJoyRight(int joycodeIndex, int val);

int GetUIJoyStart(int joycodeIndex);
void SetUIJoyStart(int joycodeIndex, int val);

int GetUIJoyPageUp(int joycodeIndex);
void SetUIJoyPageUp(int joycodeIndex, int val);

int GetUIJoyPageDown(int joycodeIndex);
void SetUIJoyPageDown(int joycodeIndex, int val);

int GetUIJoyHome(int joycodeIndex);
void SetUIJoyHome(int joycodeIndex, int val);

int GetUIJoyEnd(int joycodeIndex);
void SetUIJoyEnd(int joycodeIndex, int val);

int GetUIJoySSChange(int joycodeIndex);
void SetUIJoySSChange(int joycodeIndex, int val);

int GetUIJoyHistoryUp(int joycodeIndex);
void SetUIJoyHistoryUp(int joycodeIndex, int val);

int GetUIJoyHistoryDown(int joycodeIndex);
void SetUIJoyHistoryDown(int joycodeIndex, int val);

int GetUIJoyExec(int joycodeIndex);
void SetUIJoyExec(int joycodeIndex, int val);

#endif
