/*
 * Lotus Navigator built-in notifications: settings and placement policy.
 * See macos9_notify_policy.h.
 */
#include <stddef.h>

#include "macos9_notify_policy.h"

const char *const macos9_notify_category_names[MACOS9_NOTIFY_CATEGORY_COUNT] = {
	"downloads", "proxy", "security", "housekeeping", "web"
};

void macos9_notify_settings_defaults(macos9_notify_settings *settings)
{
	int i;

	for (i = 0; i < LN_TYPE_COUNT; i++) {
		settings->style[i] = LN_STYLE_DEFAULT;
	}
	settings->position = LN_POSITION_UPPER_RIGHT;
	settings->banner_seconds = LN_BANNER_DEFAULT_SECONDS;
	settings->sound = 1;
	for (i = 0; i < MACOS9_NOTIFY_CATEGORY_COUNT; i++) {
		settings->category_on[i] = (i != MACOS9_NOTIFY_WEB);
	}
}

void macos9_notify_build_prefs(const macos9_notify_settings *settings, ln_prefs *prefs)
{
	ln_app_prefs *app;
	ln_appearance *group;
	int i;

	ln_prefs_init(prefs);
	ln_prefs_set_history_size(prefs, 0);
	prefs->sounds = settings->sound != 0;

	app = ln_prefs_add_app(prefs, MACOS9_NOTIFY_SIGNATURE, 1);
	for (i = 0; i < MACOS9_NOTIFY_CATEGORY_COUNT; i++) {
		ln_category_prefs *category = ln_app_declare_category(app, macos9_notify_category_names[i]);

		if (category != NULL) {
			category->enabled = settings->category_on[i] != 0;
		}
	}
	for (i = 0; i < LN_TYPE_COUNT; i++) {
		if (i != LN_TYPE_QUESTION) {
			prefs->by_type[i].style = settings->style[i];
		}
	}
	group = &prefs->by_group[LN_GROUP_APPLICATIONS];
	group->position = settings->position;
	group->banner_seconds = settings->banner_seconds;
}

int macos9_notify_place(const macos9_notify_rect *usable, int position, int width, int height,
                        int offset, macos9_notify_rect *out)
{
	int centre_left = (usable->left + usable->right - width) / 2;
	int top_edge = usable->top + MACOS9_NOTIFY_MARGIN + offset;
	int bottom_edge = usable->bottom - MACOS9_NOTIFY_MARGIN - offset;

	switch (position) {
	case LN_POSITION_UPPER_LEFT:
		out->left = usable->left + MACOS9_NOTIFY_MARGIN;
		out->top = top_edge;
		break;
	case LN_POSITION_LOWER_RIGHT:
		out->left = usable->right - MACOS9_NOTIFY_MARGIN - width;
		out->top = bottom_edge - height;
		break;
	case LN_POSITION_LOWER_LEFT:
		out->left = usable->left + MACOS9_NOTIFY_MARGIN;
		out->top = bottom_edge - height;
		break;
	case LN_POSITION_TOP_CENTRE:
		out->left = centre_left;
		out->top = top_edge;
		break;
	case LN_POSITION_CENTRE:
		out->left = centre_left;
		out->top = (usable->top + usable->bottom - height) / 2 + offset;
		break;
	default:
		out->left = usable->right - MACOS9_NOTIFY_MARGIN - width;
		out->top = top_edge;
		break;
	}
	out->right = out->left + width;
	out->bottom = out->top + height;
	return out->left >= usable->left && out->right <= usable->right &&
	       out->top >= usable->top && out->bottom <= usable->bottom;
}
