/*
 * lotus_brand.h - Lotus Navigator Carbon Edition product identity.
 *
 * Downstream of MacSurf (mplsllc). On-disk data names ("MacSurfData",
 * "MacSurf Preferences", ...) are deliberately unchanged: they live next to
 * the application, so installs never collide, and existing data keeps loading.
 */

#ifndef LOTUS_BRAND_H
#define LOTUS_BRAND_H

#define LOTUS_APP_NAME      "Lotus Navigator"
#define LOTUS_APP_VERSION   "0.1.0"
#define LOTUS_APP_CREATOR   'LtsN'

/*
 * File > Send Debug Log POSTs the Desktop log (which contains visited URLs)
 * to this endpoint. Leave undefined to compile the upload out and disable the
 * menu item; when defined it must be an https:// URL the project controls.
 */
/* #define LOTUS_DEBUG_LOG_UPLOAD_URL "https://example.invalid/upload" */

#endif /* LOTUS_BRAND_H */
