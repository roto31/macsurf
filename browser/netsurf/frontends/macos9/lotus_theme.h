/*
 * lotus_theme.h - Lotus Navigator Carbon Edition chrome palette.
 *
 * Mac OS X 10.5 Leopard "Aqua Blue" appearance. Values are 0xRRGGBB; the
 * focus ring and blue tint were read from NSColor on 10.5.8, the rest are
 * Leopard's defaults for the Blue highlight colour. One accent, used only for
 * interactive and selected states. scripts/branding/30-render-leopard-theme.sh
 * reads this file to colour the toolbar icons, so edit colours here only;
 * scripts/branding/theme_audit.py checks the contrast of each pairing.
 */

#ifndef LOTUS_THEME_H
#define LOTUS_THEME_H

#define LOTUS_ACCENT            0x3875D7   /* progress segment, hover frame */
#define LOTUS_SELECTION         0x2F6AD0   /* selected list row (white text) */
#define LOTUS_FOCUS_RING        0x4A88CF   /* keyboard focus (URL field) */
#define LOTUS_HOVER_FILL        0xDCE7F6   /* plate behind a hovered button */
#define LOTUS_TOOLBAR_MATTE     0xD6D6D6   /* toolbar grey the icons sit on */
#define LOTUS_PROGRESS_TRACK    0xE3E3E3

#define LOTUS_ICON_TOP          0x7DB0EE   /* toolbar glyph gradient */
#define LOTUS_ICON_BOTTOM       0x2C6AD0
#define LOTUS_ICON_EDGE         0x1A4D9E
#define LOTUS_ICON_OFF_TOP      0xC4C4C4   /* unavailable glyph gradient */
#define LOTUS_ICON_OFF_BOTTOM   0x9A9A9A
#define LOTUS_ICON_OFF_EDGE     0x7E7E7E

#define LOTUS_BANNER_TOP        0x5C93D5   /* window title banners */
#define LOTUS_BANNER_BOTTOM     0x1653AA
#define LOTUS_BANNER_RULE       0x0F3F86

#define LOTUS_ROW_STRIPE        0xEDF3FE   /* alternating list rows */
#define LOTUS_ROW_TINT_TOP      0xEDF3FE   /* list section headers */
#define LOTUS_ROW_TINT_BOTTOM   0xD5E3F7
#define LOTUS_HEADER_TEXT       0x1A3F7A   /* section header and folder text */

#define LOTUS_BUTTON_TOP        0xF6F9FE   /* manager push buttons */
#define LOTUS_BUTTON_BOTTOM     0xD3E1F5

#define LOTUS_R8(c)   ((int)(((c) >> 16) & 0xFF))
#define LOTUS_G8(c)   ((int)(((c) >> 8) & 0xFF))
#define LOTUS_B8(c)   ((int)((c) & 0xFF))
/* Expands to three 8-bit arguments, for the r, g, b gradient helpers. */
#define LOTUS_RGB8(c) LOTUS_R8(c), LOTUS_G8(c), LOTUS_B8(c)
#define LOTUS_C16(v)  ((unsigned short)(((v) << 8) | (v)))
/* Aggregate initialiser for a QuickDraw RGBColor. */
#define LOTUS_RGBCOLOR(c) { LOTUS_C16(LOTUS_R8(c)), LOTUS_C16(LOTUS_G8(c)), LOTUS_C16(LOTUS_B8(c)) }
#define LOTUS_SET_RGBCOLOR(rgb, c) \
	((rgb).red = LOTUS_C16(LOTUS_R8(c)), \
	 (rgb).green = LOTUS_C16(LOTUS_G8(c)), \
	 (rgb).blue = LOTUS_C16(LOTUS_B8(c)))

#endif /* LOTUS_THEME_H */
