/*
 * MacSurf - macos9_woff2.c
 *
 * Placeholder while the WOFF2 decoder is rewritten from the W3C WOFF 2.0
 * Recommendation (Lotus Navigator blocker B08). Every WOFF2 font is
 * reported as unreadable, so @font-face falls back to system fonts.
 */

#include <stddef.h>

#include "macos9_woff2.h"

int macos9_woff2_to_ttf(const unsigned char *src, long src_len,
		unsigned char **out, long *out_len)
{
	(void)src;
	(void)src_len;
	if (out != NULL)
		*out = NULL;
	if (out_len != NULL)
		*out_len = 0;
	return 0;
}
