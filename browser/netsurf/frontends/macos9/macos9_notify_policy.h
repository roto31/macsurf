/*
 * Lotus Navigator built-in notifications: settings and placement policy.
 *
 * No Toolbox calls, so it is host-tested (tests/carbon/test_macos9_notify_policy.c).
 * The Lotus Notify core (lotus_notify_core.c) is copied into this folder by
 * the packager; it is not kept in the fork.
 */
#ifndef MACOS9_NOTIFY_POLICY_H
#define MACOS9_NOTIFY_POLICY_H

#include "lotus_notify_core.h"

#define MACOS9_NOTIFY_SIGNATURE LN_FOURCC('L', 't', 's', 'N')

#define MACOS9_NOTIFY_MAX_VISIBLE 3   /* per position */
#define MACOS9_NOTIFY_MARGIN      12
#define MACOS9_NOTIFY_GAP         8

enum {
	MACOS9_NOTIFY_DOWNLOADS,
	MACOS9_NOTIFY_PROXY,
	MACOS9_NOTIFY_SECURITY,
	MACOS9_NOTIFY_HOUSEKEEPING,
	MACOS9_NOTIFY_WEB,
	MACOS9_NOTIFY_CATEGORY_COUNT
};

extern const char *const macos9_notify_category_names[MACOS9_NOTIFY_CATEGORY_COUNT];

typedef struct {
	int style[LN_TYPE_COUNT];     /* ln_style per type; LN_STYLE_DEFAULT = built-in */
	int position;                 /* ln_position */
	int banner_seconds;
	int sound;
	int category_on[MACOS9_NOTIFY_CATEGORY_COUNT];
} macos9_notify_settings;

typedef struct {
	int top;
	int left;
	int bottom;
	int right;
} macos9_notify_rect;

void macos9_notify_settings_defaults(macos9_notify_settings *settings);

/* Built-in mode has no history, so style None drops the notice. */
void macos9_notify_build_prefs(const macos9_notify_settings *settings, ln_prefs *prefs);

/* Places a notice of width x height at position inside usable, below (or,
 * for the lower corners, above) offset pixels already taken by earlier
 * notices at the same position. Returns 0 when it does not fit. */
int macos9_notify_place(const macos9_notify_rect *usable, int position, int width, int height,
                        int offset, macos9_notify_rect *out);

/* Places a question where Mac alerts go: centred across usable, with a third
 * of the leftover height above it. It stays clear of the four corner stacks;
 * top-centre and centre stacks can sit partly behind it while it is open. */
void macos9_notify_place_alert(const macos9_notify_rect *usable, int width, int height,
                               macos9_notify_rect *out);

/* A page's alert() and confirm() offer "Stop dialogs from this page" from
 * this dialog on. */
#define MACOS9_NOTIFY_PAGE_STOP_FROM 2

int macos9_notify_page_offers_stop(int dialogs_on_page);

/* Year from a compiler __DATE__ string ("Oct 21 2026"); 0 when malformed. */
long macos9_notify_year_from_date(const char *compiler_date);

/* A certificate date failure points at the computer's clock when the clock
 * believes it is earlier than the year this browser was built. */
int macos9_notify_clock_suspect(long believed_year, long build_year);

enum {
	MACOS9_NOTIFY_PROXY_QUIET,
	MACOS9_NOTIFY_PROXY_STOPPED,
	MACOS9_NOTIFY_PROXY_BACK
};

/* One notice per change of proxy reachability. *last_seen holds the last
 * up/down state seen (start it at MACOS9_PROXY_UNCONFIGURED); state is the
 * current macos9_proxy_state(). A first result of "down" is reported, a
 * first "up" is not, and an unfinished probe changes nothing. */
int macos9_notify_proxy_change(int *last_seen, int state);

/* Show Examples tracks its Downloading notice by id. A post that was not shown
 * returns 0 (flood limit, category off, style None, Do Not Disturb); keeping the
 * tracked id then lets a notice already on screen count on to 100% and close. */
long macos9_notify_example_progress_id(long tracked_id, long posted_id);

#endif
