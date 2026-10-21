/*
 * Lotus Navigator built-in notifications (Lotus Notify phase N2).
 *
 * Banners and pop-up boxes are floating windows owned by the browser; the
 * Lotus Notify core decides their style, position and sound from the
 * Notifications preferences. Questions are browser dialogs: modal, any
 * text length, and never silenced.
 */
#ifndef MACOS9_NOTIFY_H
#define MACOS9_NOTIFY_H

#include "macos9_notify_policy.h"

#ifdef __MACOS9__
#include <Carbon.h>
#endif

typedef struct {
	const char *title;                    /* UTF-8 */
	const char *message;                  /* UTF-8, any length */
	ln_icon     icon;
	const char *buttons[LN_MAX_BUTTONS];  /* [0] default (Return), [1] cancel (Esc) */
	const char *checkbox;                 /* NULL, or the checkbox label */
	int         checkbox_value;           /* in and out */
	int         play_sound;
} macos9_dialog;

void macos9_notify_settings_from_options(macos9_notify_settings *settings);
void macos9_notify_settings_to_options(const macos9_notify_settings *settings);

/* Notices other than questions; notice->category is a
 * macos9_notify_category_names entry. *id_out is 0 when nothing is shown. */
void macos9_notify_post(const ln_notice *notice, long *id_out);
void macos9_notify_update(long id, int percent, const char *message);
void macos9_notify_withdraw(long id);

/* Returns the index of the button clicked. */
int  macos9_notify_ask(macos9_dialog *dialog);

/* Posts one notice of each kind with settings, which need not be saved yet. */
void macos9_notify_show_examples(const macos9_notify_settings *settings);

void macos9_notify_idle(void);

#ifdef __MACOS9__
int  macos9_notify_owns(WindowRef win);
void macos9_notify_draw(WindowRef win);
void macos9_notify_click(WindowRef win, short part, Point where);
#endif

#endif
