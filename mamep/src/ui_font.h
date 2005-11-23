/*********************************************************************

  ui_font.h

  日本語文字列を処理する補助マクロ及び関数を提供します。

*********************************************************************/

#ifndef UI_FONT_H
#define UI_FONT_H

#include "palette.h"
#include "ui_pal.h"

void uifont_buildfont(int *rotcharwidth, int *rotcharheight);
void uifont_freefont(void);
int uifont_need_font_warning(void);
int uifont_decodechar(const unsigned char *s, UINT16 *code);
void uifont_drawchar(mame_bitmap *dest, UINT16 code, int color, int sx, int sy, const rectangle *bounds);

#ifdef UI_COLOR_DISPLAY
void convert_command_move(char *buf);
#endif /* UI_COLOR_DISPLAY */


extern pen_t uifont_colortable[MAX_COLORTABLE];
#endif
