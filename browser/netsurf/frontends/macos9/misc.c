/*
 * MacSurf  -  Mac OS 9 frontend for NetSurf
 * misc.c  -  All gui_misc_table callbacks
 *
 * This file is part of MacSurf, built on the NetSurf engine.
 * Licensed under GPL v2.
 */

#include <stdlib.h>
#include <string.h>

#include "utils/ns_errors.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "netsurf/misc.h"

#include "macos9.h"
#include "macsurf_debug_log.h"

#ifdef __MWERKS__
#include <InternetConfig.h>
#endif

static void
macos9_quit(void)
{
	/* TODO: save preferences, cookies, URL database */
	macos9_done = true;
}

/* Hand URLs the engine cannot fetch (mailto:, ftp:, news:, ...) to the
 * helper application chosen in the Internet control panel (OS 9) or
 * LaunchServices (OS X). */
static nserror
macos9_launch_url(struct nsurl *url)
{
#ifdef __MWERKS__
	ICInstance  ic;
	const char *text;
	long        sel_start;
	long        sel_end;
	OSStatus    err;

	if (url == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	text = nsurl_access(url);
	sel_start = 0;
	sel_end = (long)strlen(text);

	err = ICStart(&ic, LOTUS_APP_CREATOR);
	if (err != noErr) {
		macsurf_debug_log_writef("IC ICStart err=%ld", (long)err);
		return NSERROR_INIT_FAILED;
	}
	err = ICLaunchURL(ic, "\p", (Ptr)text, sel_end, &sel_start, &sel_end);
	ICStop(ic);
	if (err != noErr) {
		macsurf_debug_log_writef("IC ICLaunchURL err=%ld", (long)err);
		return NSERROR_NOT_FOUND;
	}
	return NSERROR_OK;
#else
	(void)url;
	return NSERROR_NOT_IMPLEMENTED;
#endif
}

static nserror
macos9_login(struct nsurl *url, const char *realm,
	     const char *username, const char *password,
	     nserror (*cb)(struct nsurl *url,
			   const char *realm,
			   const char *username,
			   const char *password,
			   void *pw),
	     void *cbpw)
{
	/* TODO: modal dialog with username/password fields */
	return NSERROR_NOT_IMPLEMENTED;
}

static void
macos9_pdf_password(char **owner_pass, char **user_pass, char *path)
{
	/* Not needed  -  no PDF export */
}

static nserror
macos9_present_cookies(const char *search_term)
{
	/* TODO: cookie manager window */
	return NSERROR_NOT_IMPLEMENTED;
}

/* Field order: schedule, quit, launch_url, login, pdf_password,
 * present_cookies (see include/netsurf/misc.h) */
struct gui_misc_table macos9_misc_table = {
	macos9_schedule,
	macos9_quit,
	macos9_launch_url,
	macos9_login,
	macos9_pdf_password,
	macos9_present_cookies
};
