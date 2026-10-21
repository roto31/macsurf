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

#endif
