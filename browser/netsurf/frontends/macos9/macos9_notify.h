/*
 * Lotus Navigator built-in notifications (Lotus Notify phases N2 and N3).
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

/* Returns the index of the button clicked, or -1 when the dialog could not
 * be shown, so a failure never reads as the default button. */
int  macos9_notify_ask(macos9_dialog *dialog);

/* Posts one notice of each kind with settings, which need not be saved yet. */
void macos9_notify_show_examples(const macos9_notify_settings *settings);

void macos9_notify_idle(void);

/* For the moments a notice window may not be creatable (out of memory, the
 * About box fallback): a system alert that allocates nothing. title and body
 * are MacRoman, cut at 255 characters. */
void macos9_notify_emergency_alert(const char *title, const char *body, int stop);

/* After a certificate date failure: once per session, when the clock's
 * believed date ("YYYY-MM-DD") is earlier than the year this browser was
 * built, an Error notice offers to open Date & Time. */
void macos9_notify_clock_check(const char *believed_date);

#ifdef __MACOS9__
enum {
	MACOS9_NOTIFY_FILE_REVEAL,   /* show the file in the Finder */
	MACOS9_NOTIFY_FILE_OPEN      /* open it, as a double-click would */
};

/* As macos9_notify_post, for a notice about a file. Clicking a banner runs
 * the action; a pop-up box shows action_label (when not NULL) as a button
 * beside OK. */
void macos9_notify_post_file(const ln_notice *notice, const FSSpec *file, int action,
                             const char *action_label, long *id_out);

int  macos9_notify_owns(WindowRef win);
void macos9_notify_draw(WindowRef win);
void macos9_notify_click(WindowRef win, short part, Point where);
#endif

#endif
