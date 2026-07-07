/*******************************************************************************
 * Size: 9 px
 * Bpp: 4
 * Opts: --font NotoSerifSC-VariableFont_wght.ttf --size 9 --bpp 4 --symbols 0123456789. --format lvgl --lv-include lvgl.h --output ui_font_ComfortaaBold9.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef UI_FONT_COMFORTAABOLD9
#define UI_FONT_COMFORTAABOLD9 1
#endif

#if UI_FONT_COMFORTAABOLD9

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+002E "." */
    0x0, 0xac,

    /* U+0030 "0" */
    0x6, 0x56, 0x4, 0x25, 0x2, 0x35, 0x5, 0x7f,
    0x10, 0x13, 0xf1, 0x1, 0x33, 0x28, 0x2b, 0xa1,
    0x28, 0x10,

    /* U+0031 "1" */
    0x0, 0xc2, 0xb0, 0x2, 0xa0, 0x1f, 0xfc, 0x61,
    0x57, 0x30,

    /* U+0032 "2" */
    0x6, 0x47, 0x3, 0x44, 0x0, 0x99, 0x7, 0x88,
    0x0, 0x78, 0x2, 0x76, 0x0, 0x2b, 0x0, 0x13,
    0xe2, 0x8,

    /* U+0033 "3" */
    0x16, 0x47, 0x3, 0x74, 0xf0, 0x21, 0x0, 0xc4,
    0xf8, 0x0, 0x27, 0x31, 0x30, 0x1, 0x10, 0x59,
    0xc, 0xc0,

    /* U+0034 "4" */
    0x0, 0x14, 0x0, 0x4c, 0x1, 0x22, 0x0, 0x4,
    0xc0, 0x12, 0x2b, 0xb9, 0x19, 0xde, 0x40, 0xe,

    /* U+0035 "5" */
    0xa, 0x88, 0x8, 0x74, 0x40, 0x40, 0xd5, 0x0,
    0xe, 0xa6, 0x40, 0x11, 0xba, 0x0, 0x9, 0x9a,
    0x53, 0x30,

    /* U+0036 "6" */
    0x0, 0x90, 0x40, 0x59, 0x4, 0x25, 0x80, 0x4,
    0xa, 0xa0, 0x7d, 0x50, 0x33, 0x38, 0x3b, 0x2b,
    0x83, 0x30, 0xc9, 0x48, 0xc0,

    /* U+0037 "7" */
    0x48, 0x85, 0x1a, 0x44, 0x3c, 0xc0, 0x23, 0x0,
    0x91, 0x0, 0x11, 0x90, 0x0, 0x44, 0x1, 0x33,
    0x80, 0x0,

    /* U+0038 "8" */
    0x6, 0x46, 0x3, 0x24, 0x0, 0x16, 0x80, 0x43,
    0x98, 0x20, 0x31, 0xcc, 0x12, 0xa8, 0x2d, 0x9d,
    0x91, 0x50,

    /* U+0039 "9" */
    0x7, 0x56, 0x4, 0x42, 0x99, 0x14, 0xc, 0xea,
    0x80, 0xc0, 0x60, 0xbe, 0x80, 0xaf, 0x82, 0x8,
    0xec, 0x0, 0x45, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 47, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 68, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 66, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 82, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 78, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 78, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 79, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint8_t glyph_id_ofs_list_0[] = {
    0, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 9, 10
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 46, .range_length = 12, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_0, .list_length = 12, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 1,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_ComfortaaBold9 = {
#else
lv_font_t ui_font_ComfortaaBold9 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_COMFORTAABOLD9*/

