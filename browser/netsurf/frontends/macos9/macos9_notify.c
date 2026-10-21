/*
 * Lotus Navigator built-in notifications (Lotus Notify phases N2 and N3).
 * See macos9_notify.h.
 *
 * C89 / CW8-clean: no inline, no // comments, declarations at top of block.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "netsurf/browser_window.h"
#include "macos9.h"
#include "macsurf_config.h"
#include "macsurf_debug.h"
#include "lotus_theme.h"
#include "macos9_notify.h"
#include "macos9_proxy.h"
#include "macsurf_osver.h"

static int notify_clamp(int value, int low, int high, int fallback)
{
	return (value < low || value > high) ? fallback : value;
}

void macos9_notify_settings_from_options(macos9_notify_settings *settings)
{
	macos9_notify_settings_defaults(settings);
	settings->style[LN_TYPE_INFO] = notify_clamp(nsoption_int(notify_style_info), LN_STYLE_DEFAULT, LN_STYLE_POPUP, LN_STYLE_DEFAULT);
	settings->style[LN_TYPE_PROGRESS] = notify_clamp(nsoption_int(notify_style_progress), LN_STYLE_DEFAULT, LN_STYLE_POPUP, LN_STYLE_DEFAULT);
	settings->style[LN_TYPE_WARNING] = notify_clamp(nsoption_int(notify_style_warning), LN_STYLE_DEFAULT, LN_STYLE_POPUP, LN_STYLE_DEFAULT);
	settings->style[LN_TYPE_ERROR] = notify_clamp(nsoption_int(notify_style_error), LN_STYLE_DEFAULT, LN_STYLE_POPUP, LN_STYLE_DEFAULT);
	settings->position = notify_clamp(nsoption_int(notify_position), LN_POSITION_UPPER_RIGHT, LN_POSITION_CENTRE, LN_POSITION_UPPER_RIGHT);
	settings->banner_seconds = notify_clamp(nsoption_int(notify_banner_seconds), LN_BANNER_MIN_SECONDS, LN_BANNER_MAX_SECONDS, LN_BANNER_DEFAULT_SECONDS);
	settings->sound = nsoption_bool(notify_sound) ? 1 : 0;
	settings->category_on[MACOS9_NOTIFY_DOWNLOADS] = nsoption_bool(notify_downloads) ? 1 : 0;
	settings->category_on[MACOS9_NOTIFY_PROXY] = nsoption_bool(notify_proxy) ? 1 : 0;
	settings->category_on[MACOS9_NOTIFY_SECURITY] = nsoption_bool(notify_security) ? 1 : 0;
	settings->category_on[MACOS9_NOTIFY_HOUSEKEEPING] = nsoption_bool(notify_housekeeping) ? 1 : 0;
	settings->category_on[MACOS9_NOTIFY_WEB] = nsoption_bool(notify_web) ? 1 : 0;
}

void macos9_notify_settings_to_options(const macos9_notify_settings *settings)
{
	nsoption_set_int(notify_style_info, settings->style[LN_TYPE_INFO]);
	nsoption_set_int(notify_style_progress, settings->style[LN_TYPE_PROGRESS]);
	nsoption_set_int(notify_style_warning, settings->style[LN_TYPE_WARNING]);
	nsoption_set_int(notify_style_error, settings->style[LN_TYPE_ERROR]);
	nsoption_set_int(notify_position, settings->position);
	nsoption_set_int(notify_banner_seconds, settings->banner_seconds);
	nsoption_set_bool(notify_sound, settings->sound != 0);
	nsoption_set_bool(notify_downloads, settings->category_on[MACOS9_NOTIFY_DOWNLOADS] != 0);
	nsoption_set_bool(notify_proxy, settings->category_on[MACOS9_NOTIFY_PROXY] != 0);
	nsoption_set_bool(notify_security, settings->category_on[MACOS9_NOTIFY_SECURITY] != 0);
	nsoption_set_bool(notify_housekeeping, settings->category_on[MACOS9_NOTIFY_HOUSEKEEPING] != 0);
	nsoption_set_bool(notify_web, settings->category_on[MACOS9_NOTIFY_WEB] != 0);
}

#ifdef __MACOS9__

#define NOTIFY_SLOTS          8
#define NOTIFY_POSITIONS      (LN_POSITION_CENTRE + 1)
#define NOTIFY_BANNER_WIDTH   320
#define NOTIFY_POPUP_WIDTH    380
#define NOTIFY_DIALOG_WIDTH   420
#define NOTIFY_PAD            12
#define NOTIFY_ICON           32
#define NOTIFY_TEXT_LEFT      (NOTIFY_PAD + NOTIFY_ICON + 12)
#define NOTIFY_TITLE_LINE     16
#define NOTIFY_LINE           13
#define NOTIFY_BANNER_LINES   2
#define NOTIFY_POPUP_LINES    10
#define NOTIFY_PROGRESS_H     8
#define NOTIFY_BUTTON_H       20
#define NOTIFY_BUTTON_MIN_W   72
#define NOTIFY_CHECKBOX_H     20
#define NOTIFY_FONT_GENEVA    3
#define NOTIFY_FONT_APP       1
#define NOTIFY_ELLIPSIS       ((char)0xC9)   /* MacRoman */
#define NOTIFY_TICKS          60
#define NOTIFY_PROGRESS_STEP  10
#define NOTIFY_BUTTON_FLASH   8

typedef struct {
	int           used;
	long          id;
	long          seq;
	WindowRef     win;
	ControlRef    ok_button;
	ControlRef    action_button;
	int           has_file;
	int           file_action;
	FSSpec        file;
	int           type;
	int           style;
	int           position;
	int           banner_seconds;
	int           sound;
	int           shown;
	int           moved;
	int           progress;          /* -1 when the notice has no progress bar */
	unsigned long expires;           /* tick deadline; 0 = until clicked */
	short         width;
	short         height;            /* content height */
	short         frame_top;         /* title bar height above the content */
	char          thread[LN_MAX_NAME];
	char          url[LN_MAX_URL];
	char          title[LN_MAX_TITLE * 2];
	char          message[LN_MAX_MESSAGE * 2];
} notify_slot;

static notify_slot s_slots[NOTIFY_SLOTS];
static long s_next_id = 1;
static long s_next_seq = 1;
static ln_prefs s_prefs;
static ln_rate_limiter s_limiter;
static int s_limiter_ready = 0;

static long s_example_progress_id = 0;
static int s_example_progress = 0;
static unsigned long s_example_next_tick = 0;

static int s_proxy_seen = MACOS9_PROXY_UNCONFIGURED;
static long s_proxy_notice_id = 0;

static void notify_pstring(const char *src, unsigned char *dest)
{
	size_t n = strlen(src);

	if (n > 255) n = 255;
	dest[0] = (unsigned char)n;
	memcpy(dest + 1, src, n);
}

static void notify_to_roman(const char *utf8, char *out, size_t cap)
{
	size_t n;

	if (utf8 == NULL) utf8 = "";
	n = macos9_utf8_to_macroman(utf8, strlen(utf8), out, cap - 1);
	if (n >= cap) n = cap - 1;
	out[n] = '\0';
}

static void notify_copy(char *dest, const char *src, size_t cap)
{
	if (src == NULL) src = "";
	strncpy(dest, src, cap - 1);
	dest[cap - 1] = '\0';
}

static OSType notify_icon_kind(int icon, int type)
{
	if (icon == LN_ICON_STOP || (icon == LN_ICON_APP && type == LN_TYPE_ERROR)) return kAlertStopIcon;
	if (icon == LN_ICON_CAUTION || (icon == LN_ICON_APP && type == LN_TYPE_WARNING)) return kAlertCautionIcon;
	return kAlertNoteIcon;
}

static void notify_plot_icon(OSType kind, short left, short top)
{
	IconRef ref;
	Rect r;

	if (GetIconRef(kOnSystemDisk, kSystemIconsCreator, kind, &ref) != noErr) return;
	SetRect(&r, left, top, (short)(left + NOTIFY_ICON), (short)(top + NOTIFY_ICON));
	PlotIconRef(&r, kAlignNone, kTransformNone, kIconServicesNormalUsageFlag, ref);
	ReleaseIconRef(ref);
}

static void notify_vgrad(const Rect *r, unsigned long top_colour, unsigned long bottom_colour)
{
	short y;
	short h = (short)(r->bottom - r->top);
	RGBColor c;
	Rect line;

	if (h <= 0) return;
	line.left = r->left;
	line.right = r->right;
	for (y = 0; y < h; y++) {
		int rv = LOTUS_R8(top_colour) + (LOTUS_R8(bottom_colour) - LOTUS_R8(top_colour)) * y / h;
		int gv = LOTUS_G8(top_colour) + (LOTUS_G8(bottom_colour) - LOTUS_G8(top_colour)) * y / h;
		int bv = LOTUS_B8(top_colour) + (LOTUS_B8(bottom_colour) - LOTUS_B8(top_colour)) * y / h;
		c.red = LOTUS_C16(rv);
		c.green = LOTUS_C16(gv);
		c.blue = LOTUS_C16(bv);
		RGBForeColor(&c);
		line.top = (short)(r->top + y);
		line.bottom = (short)(line.top + 1);
		PaintRect(&line);
	}
}

/* Word-wraps text to width in the current port's font, drawing it when
 * draw is set. At max_lines a longer text ends in an ellipsis. Returns the
 * number of lines. */
static int notify_wrap(const char *text, long length, short left, short baseline,
		short width, int max_lines, int draw)
{
	long pos = 0;
	int lines = 0;

	while (pos < length && lines < max_lines) {
		long end = pos;
		long next;
		long last_space = -1;
		short used = 0;

		while (end < length && text[end] != '\r' && text[end] != '\n') {
			short cw = CharWidth((unsigned char)text[end]);
			if (used + cw > width) break;
			if (text[end] == ' ') last_space = end;
			used = (short)(used + cw);
			end++;
		}
		next = end;
		if (end < length && (text[end] == '\r' || text[end] == '\n')) {
			next = end + 1;
		} else if (end < length && last_space > pos) {
			end = last_space;
			next = last_space + 1;
		}
		if (next == pos) {
			end = pos + 1;
			next = end;
		}
		lines++;
		if (draw) {
			MoveTo(left, (short)(baseline + (lines - 1) * NOTIFY_LINE));
			if (lines == max_lines && next < length) {
				short ellipsis = CharWidth((unsigned char)NOTIFY_ELLIPSIS);
				while (end > pos && TextWidth(text, (short)pos, (short)(end - pos)) + ellipsis > width) end--;
				DrawText(text, (short)pos, (short)(end - pos));
				DrawChar(NOTIFY_ELLIPSIS);
			} else {
				DrawText(text, (short)pos, (short)(end - pos));
			}
		}
		pos = next;
	}
	return lines;
}

static void notify_usable(macos9_notify_rect *usable)
{
	Rect r;

	GetAvailableWindowPositioningBounds(GetMainDevice(), &r);
	usable->top = r.top;
	usable->left = r.left;
	usable->bottom = r.bottom;
	usable->right = r.right;
}

static short notify_frame_top(WindowRef win)
{
	Rect structure;
	Rect content;

	GetWindowBounds(win, kWindowStructureRgn, &structure);
	GetWindowBounds(win, kWindowContentRgn, &content);
	return (short)(content.top - structure.top);
}

static notify_slot *notify_find_window(WindowRef win)
{
	int i;

	if (win == NULL) return NULL;
	for (i = 0; i < NOTIFY_SLOTS; i++) {
		if (s_slots[i].used && s_slots[i].win == win) return &s_slots[i];
	}
	return NULL;
}

static notify_slot *notify_find_id(long id)
{
	int i;

	for (i = 0; i < NOTIFY_SLOTS; i++) {
		if (s_slots[i].used && s_slots[i].id == id) return &s_slots[i];
	}
	return NULL;
}

int macos9_notify_owns(WindowRef win)
{
	return notify_find_window(win) != NULL;
}

/* Slot indices, oldest first. */
static int notify_ordered(int order[NOTIFY_SLOTS])
{
	int count = 0;
	int i;
	int j;

	for (i = 0; i < NOTIFY_SLOTS; i++) {
		if (s_slots[i].used) order[count++] = i;
	}
	for (i = 1; i < count; i++) {
		int key = order[i];
		for (j = i - 1; j >= 0 && s_slots[order[j]].seq > s_slots[key].seq; j--) {
			order[j + 1] = order[j];
		}
		order[j + 1] = key;
	}
	return count;
}

static void notify_first_show(notify_slot *s)
{
	ShowWindow(s->win);
	HiliteWindow(s->win, true);
	s->shown = 1;
	if (s->style == LN_STYLE_BANNER && s->progress < 0) {
		s->expires = TickCount() + (unsigned long)s->banner_seconds * NOTIFY_TICKS;
	}
	if (s->sound) SysBeep(30);
}

/* Stacks visible notices per position, oldest nearest the corner, and
 * shows queued ones as room appears. */
static void notify_layout(void)
{
	macos9_notify_rect usable;
	macos9_notify_rect r;
	int offsets[NOTIFY_POSITIONS];
	int counts[NOTIFY_POSITIONS];
	int order[NOTIFY_SLOTS];
	int count;
	int i;

	memset(offsets, 0, sizeof offsets);
	memset(counts, 0, sizeof counts);
	notify_usable(&usable);
	count = notify_ordered(order);
	for (i = 0; i < count; i++) {
		notify_slot *s = &s_slots[order[i]];
		int total = s->height + s->frame_top;

		if (s->moved) continue;
		if (counts[s->position] >= MACOS9_NOTIFY_MAX_VISIBLE) continue;
		if (!macos9_notify_place(&usable, s->position, s->width, total, offsets[s->position], &r)) continue;
		MoveWindow(s->win, (short)r.left, (short)(r.top + s->frame_top), false);
		if (!s->shown) notify_first_show(s);
		offsets[s->position] += total + MACOS9_NOTIFY_GAP;
		counts[s->position]++;
	}
}

static void notify_dismiss(notify_slot *s)
{
	if (s->win != NULL) DisposeWindow(s->win);
	if (s->id == s_example_progress_id) s_example_progress_id = 0;
	memset(s, 0, sizeof *s);
	notify_layout();
}

static void notify_paint(notify_slot *s)
{
	Rect bounds;
	Rect track;
	Rect fill;
	RGBColor black;
	RGBColor colour;
	short text_width;

	SetPortWindowPort(s->win);
	GetWindowPortBounds(s->win, &bounds);
	notify_vgrad(&bounds, LOTUS_BUTTON_TOP, LOTUS_BUTTON_BOTTOM);
	notify_plot_icon(notify_icon_kind(LN_ICON_APP, s->type), NOTIFY_PAD, NOTIFY_PAD);

	black.red = black.green = black.blue = 0;
	RGBForeColor(&black);
	text_width = (short)(s->width - NOTIFY_TEXT_LEFT - NOTIFY_PAD);
	TextFont(NOTIFY_FONT_APP);
	TextSize(12);
	TextFace(bold);
	notify_wrap(s->title, (long)strlen(s->title), NOTIFY_TEXT_LEFT, NOTIFY_PAD + 12, text_width, 1, 1);
	TextFace(normal);
	TextFont(NOTIFY_FONT_GENEVA);
	TextSize(10);
	notify_wrap(s->message, (long)strlen(s->message), NOTIFY_TEXT_LEFT,
		NOTIFY_PAD + NOTIFY_TITLE_LINE + 10, text_width,
		s->style == LN_STYLE_POPUP ? NOTIFY_POPUP_LINES : NOTIFY_BANNER_LINES, 1);

	if (s->progress >= 0) {
		short track_bottom = (short)(bounds.bottom - NOTIFY_PAD);
		if (s->ok_button != NULL) track_bottom = (short)(track_bottom - NOTIFY_BUTTON_H - NOTIFY_PAD);
		SetRect(&track, NOTIFY_TEXT_LEFT, (short)(track_bottom - NOTIFY_PROGRESS_H),
			(short)(bounds.right - NOTIFY_PAD), track_bottom);
		LOTUS_SET_RGBCOLOR(colour, LOTUS_PROGRESS_TRACK);
		RGBForeColor(&colour);
		PaintRoundRect(&track, NOTIFY_PROGRESS_H, NOTIFY_PROGRESS_H);
		fill = track;
		fill.right = (short)(track.left + (track.right - track.left) * s->progress / 100);
		LOTUS_SET_RGBCOLOR(colour, LOTUS_ACCENT);
		RGBForeColor(&colour);
		if (fill.right > fill.left) PaintRoundRect(&fill, NOTIFY_PROGRESS_H, NOTIFY_PROGRESS_H);
		RGBForeColor(&black);
	}
	TextFont(0);
	TextSize(0);
	if (s->ok_button != NULL) Draw1Control(s->ok_button);
	if (s->action_button != NULL) Draw1Control(s->action_button);
}

void macos9_notify_draw(WindowRef win)
{
	notify_slot *s = notify_find_window(win);
	GrafPtr saved;

	if (s == NULL) return;
	GetPort(&saved);
	SetPortWindowPort(win);
	BeginUpdate(win);
	notify_paint(s);
	EndUpdate(win);
	SetPort(saved);
}

static void notify_open_url(const char *url)
{
	nsurl *target = NULL;
	struct browser_window *bw = NULL;

	if (url[0] == '\0') return;
	if (nsurl_create(url, &target) != NSERROR_OK) return;
	browser_window_create(BW_CREATE_HISTORY | BW_CREATE_FOREGROUND, target, NULL, NULL, &bw);
	nsurl_unref(target);
}

static void notify_run_action(notify_slot *s)
{
	if (!s->has_file) {
		notify_open_url(s->url);
	} else if (s->file_action == MACOS9_NOTIFY_FILE_OPEN) {
		macos9_finder_open_file(&s->file);
	} else {
		macos9_finder_reveal_file(&s->file);
	}
}

void macos9_notify_click(WindowRef win, short part, Point where)
{
	notify_slot *s = notify_find_window(win);
	BitMap screen;
	ControlRef hit = NULL;
	Point local = where;
	GrafPtr saved;

	if (s == NULL) return;
	switch (part) {
	case inDrag:
		GetQDGlobalsScreenBits(&screen);
		DragWindow(win, where, &screen.bounds);
		s->moved = 1;
		break;
	case inGoAway:
		if (TrackGoAway(win, where)) notify_dismiss(s);
		break;
	case inContent:
		GetPort(&saved);
		SetPortWindowPort(win);
		GlobalToLocal(&local);
		if (s->ok_button != NULL) {
			FindControl(local, win, &hit);
			if (hit != NULL && TrackControl(hit, local, NULL) != 0) {
				if (hit == s->action_button) notify_run_action(s);
				if (hit == s->action_button || hit == s->ok_button) notify_dismiss(s);
			}
		} else {
			notify_run_action(s);
			notify_dismiss(s);
		}
		SetPort(saved);
		break;
	default:
		break;
	}
}

static void notify_clock(ln_clock *clock)
{
	clock->seconds = TickCount() / NOTIFY_TICKS;
	clock->minute_of_day = 0;
}

static int notify_resolve(const macos9_notify_settings *settings, const ln_notice *notice, ln_result *result)
{
	ln_sender sender;
	ln_clock clock;

	if (!s_limiter_ready) {
		ln_rate_init(&s_limiter);
		s_limiter_ready = 1;
	}
	sender.signature = MACOS9_NOTIFY_SIGNATURE;
	sender.group = LN_GROUP_APPLICATIONS;
	notify_clock(&clock);
	macos9_notify_build_prefs(settings, &s_prefs);
	return ln_resolve(&s_prefs, &sender, notice, &clock,
		notice->type == LN_TYPE_QUESTION ? NULL : &s_limiter, result);
}

static notify_slot *notify_free_slot(void)
{
	int order[NOTIFY_SLOTS];
	int i;

	for (i = 0; i < NOTIFY_SLOTS; i++) {
		if (!s_slots[i].used) return &s_slots[i];
	}
	notify_ordered(order);
	notify_dismiss(&s_slots[order[0]]);
	return &s_slots[order[0]];
}

static void notify_size(notify_slot *s)
{
	short text_width = (short)(s->width - NOTIFY_TEXT_LEFT - NOTIFY_PAD);
	int lines;
	short height;

	SetPortWindowPort(s->win);
	TextFont(NOTIFY_FONT_GENEVA);
	TextSize(10);
	lines = notify_wrap(s->message, (long)strlen(s->message), 0, 0, text_width,
		s->style == LN_STYLE_POPUP ? NOTIFY_POPUP_LINES : NOTIFY_BANNER_LINES, 0);
	if (s->progress >= 0 && lines < NOTIFY_BANNER_LINES) lines = NOTIFY_BANNER_LINES;
	height = (short)(NOTIFY_PAD + NOTIFY_TITLE_LINE + lines * NOTIFY_LINE + NOTIFY_PAD);
	if (s->progress >= 0) height = (short)(height + NOTIFY_PROGRESS_H + 6);
	if (s->style == LN_STYLE_POPUP) height = (short)(height + NOTIFY_BUTTON_H + NOTIFY_PAD);
	if (height < NOTIFY_ICON + 2 * NOTIFY_PAD) height = NOTIFY_ICON + 2 * NOTIFY_PAD;
	s->height = height;
	SizeWindow(s->win, s->width, s->height, false);
	TextFont(0);
	TextSize(0);
}

static void notify_add_action_button(notify_slot *s, const char *label_utf8)
{
	char roman[LN_MAX_BUTTON * 2];
	unsigned char label[LN_MAX_BUTTON * 2];
	Rect ok;
	Rect r;
	short w;

	if (s->ok_button == NULL || label_utf8 == NULL) return;
	notify_to_roman(label_utf8, roman, sizeof roman);
	TextFont(0);
	TextSize(0);
	w = (short)(TextWidth(roman, 0, (short)strlen(roman)) + 24);
	if (w < NOTIFY_BUTTON_MIN_W) w = NOTIFY_BUTTON_MIN_W;
	GetControlBounds(s->ok_button, &ok);
	SetRect(&r, (short)(ok.left - 12 - w), ok.top, (short)(ok.left - 12), ok.bottom);
	notify_pstring(roman, label);
	s->action_button = NewControl(s->win, &r, label, true, 0, 0, 0, kControlPushButtonProc, 0);
}

static long notify_post_unguarded(const macos9_notify_settings *settings, const ln_notice *notice,
		const FSSpec *file, int file_action, const char *action_label)
{
	ln_result result;
	notify_slot *s;
	Rect bounds;
	Rect button;
	unsigned char title[32];
	int status;
	int i;

	if (notice->type == LN_TYPE_QUESTION) {
		macsurf_debug_log_writef("LIFE NOTIFY post refused: questions use macos9_notify_ask");
		return 0;
	}
	status = notify_resolve(settings, notice, &result);
	macsurf_debug_log_writef("LIFE NOTIFY post type=%d category=%s status=%d decision=%d style=%d position=%d sound=%d",
		(int)notice->type, notice->category != NULL ? notice->category : "-", status,
		(int)result.decision, result.style, result.position, result.sound);
	if (status != LN_OK || result.decision != LN_DECISION_SHOW) return 0;

	if (notice->thread_id != NULL && notice->thread_id[0] != '\0') {
		for (i = 0; i < NOTIFY_SLOTS; i++) {
			s = &s_slots[i];
			if (s->used && strcmp(s->thread, notice->thread_id) == 0) {
				notify_to_roman(notice->title, s->title, sizeof s->title);
				notify_to_roman(notice->message, s->message, sizeof s->message);
				SetPortWindowPort(s->win);
				GetWindowPortBounds(s->win, &bounds);
				InvalWindowRect(s->win, &bounds);
				return s->id;
			}
		}
	}

	s = notify_free_slot();
	memset(s, 0, sizeof *s);
	s->type = notice->type;
	s->style = result.style;
	s->position = notify_clamp(result.position, LN_POSITION_UPPER_RIGHT, LN_POSITION_CENTRE, LN_POSITION_UPPER_RIGHT);
	s->banner_seconds = result.banner_seconds;
	s->sound = result.sound;
	s->progress = notice->type == LN_TYPE_PROGRESS ? 0 : -1;
	s->width = (short)(s->style == LN_STYLE_POPUP ? NOTIFY_POPUP_WIDTH : NOTIFY_BANNER_WIDTH);
	notify_copy(s->thread, notice->thread_id, sizeof s->thread);
	notify_copy(s->url, notice->open_url, sizeof s->url);
	notify_to_roman(notice->title, s->title, sizeof s->title);
	notify_to_roman(notice->message, s->message, sizeof s->message);
	if (file != NULL) {
		s->has_file = 1;
		s->file = *file;
		s->file_action = file_action;
	}

	SetRect(&bounds, 0, 0, s->width, 60);
	if (CreateNewWindow(kFloatingWindowClass, kWindowCloseBoxAttribute, &bounds, &s->win) != noErr ||
	    s->win == NULL) {
		macsurf_debug_log_writef("LIFE NOTIFY window creation failed");
		memset(s, 0, sizeof *s);
		return 0;
	}
	s->used = 1;
	s->id = s_next_id++;
	s->seq = s_next_seq++;
	notify_pstring("Lotus Navigator", title);
	SetWTitle(s->win, title);
	notify_size(s);
	s->frame_top = notify_frame_top(s->win);
	if (s->style == LN_STYLE_POPUP) {
		ControlRef root = NULL;
		Boolean is_default = true;

		CreateRootControl(s->win, &root);
		SetRect(&button, (short)(s->width - NOTIFY_PAD - NOTIFY_BUTTON_MIN_W),
			(short)(s->height - NOTIFY_PAD - NOTIFY_BUTTON_H),
			(short)(s->width - NOTIFY_PAD), (short)(s->height - NOTIFY_PAD));
		s->ok_button = NewControl(s->win, &button, "\pOK", true, 0, 0, 0, kControlPushButtonProc, 0);
		if (s->ok_button != NULL) {
			SetControlData(s->ok_button, kControlEntireControl, kControlPushButtonDefaultTag,
				sizeof is_default, (Ptr)&is_default);
		}
		if (s->has_file) notify_add_action_button(s, action_label);
	}
	notify_layout();
	return s->id;
}

/* Posting sizes text in the notice's window, so the caller's port is kept. */
static long notify_post_file_with(const macos9_notify_settings *settings, const ln_notice *notice,
		const FSSpec *file, int file_action, const char *action_label)
{
	GrafPtr saved;
	long id;

	GetPort(&saved);
	id = notify_post_unguarded(settings, notice, file, file_action, action_label);
	SetPort(saved);
	return id;
}

static long notify_post_with(const macos9_notify_settings *settings, const ln_notice *notice)
{
	return notify_post_file_with(settings, notice, NULL, 0, NULL);
}

void macos9_notify_post(const ln_notice *notice, long *id_out)
{
	macos9_notify_settings settings;
	long id;

	macos9_notify_settings_from_options(&settings);
	id = notify_post_with(&settings, notice);
	if (id_out != NULL) *id_out = id;
}

void macos9_notify_post_file(const ln_notice *notice, const FSSpec *file, int action,
                             const char *action_label, long *id_out)
{
	macos9_notify_settings settings;
	long id;

	macos9_notify_settings_from_options(&settings);
	id = notify_post_file_with(&settings, notice, file, action, action_label);
	if (id_out != NULL) *id_out = id;
}

void macos9_notify_emergency_alert(const char *title, const char *body, int stop)
{
	Str255 ptitle;
	Str255 pbody;
	short item;

	notify_pstring(title != NULL ? title : "", ptitle);
	notify_pstring(body != NULL ? body : "", pbody);
	if (stop) SysBeep(30);
	StandardAlert(stop ? kAlertStopAlert : kAlertNoteAlert, ptitle, pbody, NULL, &item);
}

#define NOTIFY_SYSTEM_DOMAIN      (-32766)   /* kSystemDomain */
#define NOTIFY_PREF_PANES_FOLDER  'ppan'     /* kPreferencePanesFolderType */

/* The Date & Time control panel (Mac OS 9) or preference pane (Mac OS X). */
static int notify_date_time_item(FSSpec *out)
{
	short vref;
	long dir_id;

	if (macsurf_os_is_osx()) {
		if (FindFolder(NOTIFY_SYSTEM_DOMAIN, NOTIFY_PREF_PANES_FOLDER, kDontCreateFolder, &vref, &dir_id) != noErr)
			return 0;
		return FSMakeFSSpec(vref, dir_id, "\pDateAndTime.prefPane", out) == noErr;
	}
	if (FindFolder(kOnSystemDisk, kControlPanelFolderType, kDontCreateFolder, &vref, &dir_id) != noErr) return 0;
	return FSMakeFSSpec(vref, dir_id, "\pDate & Time", out) == noErr;
}

/* A correct clock never reads earlier than the build, so unlike an expiry
 * guess this test does not go stale as the binary ages. */
void macos9_notify_clock_check(const char *believed_date)
{
	static int s_clock_notice_shown = 0;
	char message[LN_MAX_MESSAGE];
	ln_notice notice;
	FSSpec panel;

	if (s_clock_notice_shown || believed_date == NULL || believed_date[0] == '\0') return;
	if (!macos9_notify_clock_suspect(atol(believed_date), macos9_notify_year_from_date(__DATE__))) return;
	s_clock_notice_shown = 1;
	memset(&notice, 0, sizeof notice);
	notice.type = LN_TYPE_ERROR;
	notice.category = "security";
	notice.title = "Check this Mac's clock";
	sprintf(message, "The clock reads %.16s, so secure sites refuse to load. "
		"Set the date and time, then reload the page.", believed_date);
	notice.message = message;
	notice.play_sound = 1;
	if (notify_date_time_item(&panel)) {
		macos9_notify_post_file(&notice, &panel, MACOS9_NOTIFY_FILE_OPEN, "Open Date & Time", NULL);
	} else {
		macos9_notify_post(&notice, NULL);
	}
}

void macos9_notify_update(long id, int percent, const char *message)
{
	notify_slot *s = notify_find_id(id);
	Rect bounds;
	GrafPtr saved;

	if (s == NULL) return;
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	if (s->progress >= 0) {
		s->progress = percent;
		if (percent == 100) {
			s->expires = TickCount() + (unsigned long)s->banner_seconds * NOTIFY_TICKS;
		}
	}
	if (message != NULL) notify_to_roman(message, s->message, sizeof s->message);
	GetPort(&saved);
	SetPortWindowPort(s->win);
	GetWindowPortBounds(s->win, &bounds);
	InvalWindowRect(s->win, &bounds);
	SetPort(saved);
}

void macos9_notify_withdraw(long id)
{
	notify_slot *s = notify_find_id(id);

	if (s != NULL) notify_dismiss(s);
}

static void notify_example_progress_tick(void)
{
	char text[48];

	if (s_example_progress_id == 0 || TickCount() < s_example_next_tick) return;
	s_example_progress += NOTIFY_PROGRESS_STEP;
	sprintf(text, "lotus-navigator.sit, %d%%", s_example_progress);
	macos9_notify_update(s_example_progress_id, s_example_progress, text);
	s_example_next_tick = TickCount() + NOTIFY_TICKS;
	if (s_example_progress >= 100) s_example_progress_id = 0;
}

static void notify_proxy_tick(void)
{
	char message[LN_MAX_MESSAGE];
	ln_notice notice;
	int change = macos9_notify_proxy_change(&s_proxy_seen, macos9_proxy_state());

	if (change == MACOS9_NOTIFY_PROXY_QUIET) return;
	memset(&notice, 0, sizeof notice);
	notice.category = "proxy";
	notice.message = message;
	notice.play_sound = 1;
	if (change == MACOS9_NOTIFY_PROXY_STOPPED) {
		notice.type = LN_TYPE_WARNING;
		notice.title = "Proxy stopped answering";
		sprintf(message, "%.100s did not answer, so pages load directly until it comes back.",
			macos9_proxy_target());
	} else {
		notice.type = LN_TYPE_INFO;
		notice.title = "Proxy answering again";
		sprintf(message, "Pages load through %.100s again.", macos9_proxy_target());
	}
	macos9_notify_withdraw(s_proxy_notice_id);
	macos9_notify_post(&notice, &s_proxy_notice_id);
}

void macos9_notify_idle(void)
{
	unsigned long now = TickCount();
	int i;

	notify_example_progress_tick();
	notify_proxy_tick();
	for (i = 0; i < NOTIFY_SLOTS; i++) {
		notify_slot *s = &s_slots[i];
		if (s->used && s->shown && s->expires != 0 && now >= s->expires) notify_dismiss(s);
	}
}

/* ---- Questions: modal browser dialog ---- */

typedef struct {
	WindowRef  win;
	ControlRef buttons[LN_MAX_BUTTONS];
	ControlRef checkbox;
	int        button_count;
	char       title[LN_MAX_TITLE * 2];
	char      *message;
	long       message_length;
	int        max_lines;
	short      width;
	short      height;
	OSType     icon;
	const char *checkbox_label;
} notify_dialog_state;

static void notify_dialog_paint(notify_dialog_state *d)
{
	Rect bounds;
	short text_width = (short)(d->width - NOTIFY_TEXT_LEFT - NOTIFY_PAD - 8);
	RGBColor black;

	SetPortWindowPort(d->win);
	GetWindowPortBounds(d->win, &bounds);
	EraseRect(&bounds);
	notify_plot_icon(d->icon, (short)(NOTIFY_PAD + 4), (short)(NOTIFY_PAD + 4));
	black.red = black.green = black.blue = 0;
	RGBForeColor(&black);
	TextFont(0);
	TextSize(0);
	TextFace(bold);
	notify_wrap(d->title, (long)strlen(d->title), NOTIFY_TEXT_LEFT, NOTIFY_PAD + 16, text_width, 2, 1);
	TextFace(normal);
	TextFont(NOTIFY_FONT_GENEVA);
	TextSize(10);
	notify_wrap(d->message, d->message_length, NOTIFY_TEXT_LEFT,
		NOTIFY_PAD + 2 * NOTIFY_TITLE_LINE + 12, text_width, d->max_lines, 1);
	if (d->checkbox != NULL) {
		Rect cr;
		GetControlBounds(d->checkbox, &cr);
		TextFont(NOTIFY_FONT_APP);
		TextSize(12);
		MoveTo((short)(cr.right + 4), (short)(cr.top + 14));
		DrawText(d->checkbox_label, 0, (short)strlen(d->checkbox_label));
	}
	TextFont(0);
	TextSize(0);
	DrawControls(d->win);
}

static int notify_dialog_text_lines(notify_dialog_state *d, const macos9_notify_rect *usable)
{
	short text_width = (short)(d->width - NOTIFY_TEXT_LEFT - NOTIFY_PAD - 8);
	int fixed = NOTIFY_PAD + 2 * NOTIFY_TITLE_LINE + 12 + 3 * NOTIFY_PAD + NOTIFY_BUTTON_H + NOTIFY_CHECKBOX_H + 40;
	int lines;

	d->max_lines = (usable->bottom - usable->top - fixed) / NOTIFY_LINE;
	if (d->max_lines < 3) d->max_lines = 3;
	TextFont(NOTIFY_FONT_GENEVA);
	TextSize(10);
	lines = notify_wrap(d->message, d->message_length, 0, 0, text_width, d->max_lines, 0);
	TextFont(0);
	TextSize(0);
	return lines;
}

static void notify_dialog_controls(notify_dialog_state *d, const macos9_dialog *dialog)
{
	ControlRef root = NULL;
	Boolean is_default = true;
	short right = (short)(d->width - NOTIFY_PAD - 4);
	short bottom = (short)(d->height - NOTIFY_PAD - 4);
	unsigned char label[64];
	char roman[64];
	Rect r;
	int i;

	CreateRootControl(d->win, &root);
	TextFont(0);
	TextSize(0);
	for (i = 0; i < LN_MAX_BUTTONS && dialog->buttons[i] != NULL; i++) {
		short w;
		notify_to_roman(dialog->buttons[i], roman, sizeof roman);
		w = (short)(TextWidth(roman, 0, (short)strlen(roman)) + 24);
		if (w < NOTIFY_BUTTON_MIN_W) w = NOTIFY_BUTTON_MIN_W;
		if (i == 2) {
			SetRect(&r, NOTIFY_TEXT_LEFT, (short)(bottom - NOTIFY_BUTTON_H),
				(short)(NOTIFY_TEXT_LEFT + w), bottom);
		} else {
			SetRect(&r, (short)(right - w), (short)(bottom - NOTIFY_BUTTON_H), right, bottom);
			right = (short)(right - w - 12);
		}
		notify_pstring(roman, label);
		d->buttons[i] = NewControl(d->win, &r, label, true, 0, 0, 0, kControlPushButtonProc, 0);
		d->button_count = i + 1;
	}
	if (d->buttons[0] != NULL) {
		SetControlData(d->buttons[0], kControlEntireControl, kControlPushButtonDefaultTag,
			sizeof is_default, (Ptr)&is_default);
	}
	if (dialog->checkbox != NULL) {
		SetRect(&r, NOTIFY_TEXT_LEFT, (short)(bottom - NOTIFY_BUTTON_H - 12 - NOTIFY_CHECKBOX_H),
			(short)(NOTIFY_TEXT_LEFT + NOTIFY_CHECKBOX_H), (short)(bottom - NOTIFY_BUTTON_H - 12));
		d->checkbox = NewControl(d->win, &r, "\p", true, (short)(dialog->checkbox_value ? 1 : 0), 0, 1,
			kControlCheckBoxProc, 0);
	}
}

static void notify_dialog_flash(ControlRef button)
{
	unsigned long final_ticks;

	HiliteControl(button, kControlButtonPart);
	Delay(NOTIFY_BUTTON_FLASH, &final_ticks);
	HiliteControl(button, kControlNoPart);
}

/* Returns the button index for a key, or -1. */
static int notify_dialog_key(notify_dialog_state *d, const EventRecord *ev)
{
	char ch = (char)(ev->message & charCodeMask);
	int cancel = d->button_count > 1 ? 1 : 0;

	if (ch == '\r' || ch == 0x03) {
		notify_dialog_flash(d->buttons[0]);
		return 0;
	}
	if (ch == 0x1B || ((ev->modifiers & cmdKey) && ch == '.')) {
		notify_dialog_flash(d->buttons[cancel]);
		return cancel;
	}
	return -1;
}

/* Returns the button index for a click, or -1. */
static int notify_dialog_click(notify_dialog_state *d, const EventRecord *ev)
{
	WindowRef which;
	short part = FindWindow(ev->where, &which);
	ControlRef hit = NULL;
	Point local = ev->where;
	BitMap screen;
	int i;

	if (which != d->win) {
		if (macos9_notify_owns(which)) macos9_notify_click(which, part, ev->where);
		else SysBeep(1);
		return -1;
	}
	if (part == inDrag) {
		GetQDGlobalsScreenBits(&screen);
		DragWindow(d->win, ev->where, &screen.bounds);
		return -1;
	}
	if (part != inContent) return -1;
	SetPortWindowPort(d->win);
	GlobalToLocal(&local);
	FindControl(local, d->win, &hit);
	if (hit == NULL || TrackControl(hit, local, NULL) == 0) return -1;
	if (hit == d->checkbox) {
		SetControlValue(d->checkbox, (short)(GetControlValue(d->checkbox) ? 0 : 1));
		return -1;
	}
	for (i = 0; i < d->button_count; i++) {
		if (hit == d->buttons[i]) return i;
	}
	return -1;
}

static int notify_ask_with(const macos9_notify_settings *settings, macos9_dialog *dialog)
{
	notify_dialog_state d;
	macos9_notify_rect usable;
	macos9_notify_rect placed;
	ln_notice probe;
	ln_result result;
	EventRecord ev;
	GrafPtr saved;
	Rect bounds;
	unsigned char window_title[32];
	size_t utf8_length;
	int answer = -1;
	int lines;

	memset(&d, 0, sizeof d);
	memset(&probe, 0, sizeof probe);
	probe.type = LN_TYPE_QUESTION;
	probe.title = "Question";
	probe.buttons[0] = "OK";
	probe.play_sound = dialog->play_sound;
	if (notify_resolve(settings, &probe, &result) != LN_OK) result.sound = 0;

	notify_to_roman(dialog->title, d.title, sizeof d.title);
	utf8_length = dialog->message != NULL ? strlen(dialog->message) : 0;
	d.message = (char *)malloc(utf8_length * 2 + 1);
	if (d.message == NULL) return -1;
	notify_to_roman(dialog->message, d.message, utf8_length * 2 + 1);
	d.message_length = (long)strlen(d.message);
	d.icon = notify_icon_kind(dialog->icon, LN_TYPE_QUESTION);
	d.checkbox_label = "";
	d.width = NOTIFY_DIALOG_WIDTH;

	SetRect(&bounds, 0, 0, d.width, 100);
	if (CreateNewWindow(kMovableModalWindowClass, kWindowNoAttributes, &bounds, &d.win) != noErr ||
	    d.win == NULL) {
		free(d.message);
		return -1;
	}
	GetPort(&saved);
	SetPortWindowPort(d.win);
	SetThemeWindowBackground(d.win, kThemeBrushMovableModalBackground, false);
	notify_pstring("Lotus Navigator", window_title);
	SetWTitle(d.win, window_title);

	notify_usable(&usable);
	lines = notify_dialog_text_lines(&d, &usable);
	d.height = (short)(NOTIFY_PAD + 2 * NOTIFY_TITLE_LINE + 12 + lines * NOTIFY_LINE +
		2 * NOTIFY_PAD + NOTIFY_BUTTON_H + 8);
	if (dialog->checkbox != NULL) d.height = (short)(d.height + NOTIFY_CHECKBOX_H + 12);
	SizeWindow(d.win, d.width, d.height, false);
	notify_dialog_controls(&d, dialog);
	if (dialog->checkbox != NULL) {
		static char label[LN_MAX_TITLE * 2];
		notify_to_roman(dialog->checkbox, label, sizeof label);
		d.checkbox_label = label;
	}
	if (d.button_count == 0) {
		DisposeWindow(d.win);
		free(d.message);
		SetPort(saved);
		return -1;
	}

	macos9_notify_place_alert(&usable, d.width, d.height + notify_frame_top(d.win), &placed);
	MoveWindow(d.win, (short)placed.left, (short)(placed.top + notify_frame_top(d.win)), false);
	ShowWindow(d.win);
	SelectWindow(d.win);
	if (result.sound) SysBeep(30);

	while (answer < 0) {
		WaitNextEvent(everyEvent, &ev, 30, NULL);
		switch (ev.what) {
		case mouseDown:
			answer = notify_dialog_click(&d, &ev);
			break;
		case keyDown:
		case autoKey:
			answer = notify_dialog_key(&d, &ev);
			break;
		case updateEvt:
			if ((WindowRef)ev.message == d.win) {
				BeginUpdate(d.win);
				notify_dialog_paint(&d);
				EndUpdate(d.win);
			} else {
				macos9_handle_update(&ev);
			}
			SetPortWindowPort(d.win);
			break;
		case kHighLevelEvent:
			AEProcessAppleEvent(&ev);
			break;
		case nullEvent:
			macos9_notify_idle();
			SetPortWindowPort(d.win);
			break;
		default:
			break;
		}
	}

	if (d.checkbox != NULL) dialog->checkbox_value = GetControlValue(d.checkbox) != 0;
	macsurf_debug_log_writef("LIFE NOTIFY ask buttons=%d answer=%d checkbox=%d position=%d sound=%d lines=%d",
		d.button_count, answer, dialog->checkbox_value, result.position, result.sound, lines);
	DisposeWindow(d.win);
	free(d.message);
	SetPort(saved);
	return answer;
}

int macos9_notify_ask(macos9_dialog *dialog)
{
	macos9_notify_settings settings;

	macos9_notify_settings_from_options(&settings);
	return notify_ask_with(&settings, dialog);
}

static long notify_example(const macos9_notify_settings *settings, ln_type type, const char *category,
		const char *thread, const char *title, const char *message, const char *url)
{
	ln_notice notice;

	memset(&notice, 0, sizeof notice);
	notice.type = type;
	notice.category = category;
	notice.thread_id = thread;
	notice.title = title;
	notice.message = message;
	notice.open_url = url;
	notice.play_sound = 1;
	return notify_post_with(settings, &notice);
}

void macos9_notify_show_examples(const macos9_notify_settings *settings)
{
	macos9_dialog question;
	char result_text[64];
	int answer;

	notify_example(settings, LN_TYPE_INFO, "housekeeping", NULL, "Information",
		"This is how Lotus Navigator tells you something finished. Click to open " MACSURF_HOME_URL,
		MACSURF_HOME_URL);
	s_example_progress = 0;
	s_example_next_tick = TickCount() + NOTIFY_TICKS;
	s_example_progress_id = notify_example(settings, LN_TYPE_PROGRESS, "downloads", "example-progress",
		"Downloading", "lotus-navigator.sit, 0%", NULL);
	notify_example(settings, LN_TYPE_WARNING, "proxy", NULL, "Proxy stopped answering",
		"192.168.1.120:8081 did not answer, so pages load directly until it comes back.", NULL);
	notify_example(settings, LN_TYPE_ERROR, "downloads", NULL, "Download failed",
		"The server closed the connection before lotus-navigator.sit finished downloading.", NULL);

	memset(&question, 0, sizeof question);
	question.title = "Do you want to continue?";
	question.message =
		"This is a question. Questions always appear as a pop-up box, even when every "
		"notification is turned off, because something is waiting for your answer. "
		"Long text wraps to as many lines as the screen allows instead of being cut at "
		"255 characters, as the old alerts were. Return chooses the default button, "
		"and Esc or Command-period chooses Cancel.";
	question.icon = LN_ICON_NOTE;
	question.buttons[0] = "Continue";
	question.buttons[1] = "Cancel";
	question.checkbox = "Don't ask again";
	question.play_sound = 1;
	answer = notify_ask_with(settings, &question);

	sprintf(result_text, "You chose %s%s.", answer == 0 ? "Continue" : "Cancel",
		question.checkbox_value ? " and \"Don't ask again\"" : "");
	notify_example(settings, LN_TYPE_INFO, "housekeeping", NULL, "Your answer", result_text, NULL);
}

#else /* !__MACOS9__ */

void macos9_notify_post(const ln_notice *notice, long *id_out)
{
	(void)notice;
	if (id_out != NULL) *id_out = 0;
}

void macos9_notify_update(long id, int percent, const char *message)
{
	(void)id;
	(void)percent;
	(void)message;
}

void macos9_notify_withdraw(long id)
{
	(void)id;
}

int macos9_notify_ask(macos9_dialog *dialog)
{
	(void)dialog;
	return -1;
}

void macos9_notify_show_examples(const macos9_notify_settings *settings)
{
	(void)settings;
}

void macos9_notify_idle(void)
{
}

void macos9_notify_emergency_alert(const char *title, const char *body, int stop)
{
	(void)title;
	(void)body;
	(void)stop;
}

void macos9_notify_clock_check(const char *believed_date)
{
	(void)believed_date;
}

#endif /* __MACOS9__ */
