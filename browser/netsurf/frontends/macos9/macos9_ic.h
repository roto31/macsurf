/*
 * Lotus Navigator - macos9_ic.h
 *
 * The Internet Config calls the frontend uses. macos9.h defines
 * __INTERNETCONFIG__ so Carbon.h skips InternetConfig.h (that header needs
 * AliasRecord by value before Aliases.h arrives), which also turns any
 * later #include <InternetConfig.h> into a no-op. Prototypes match
 * Universal Interfaces 3.4; the routines live in CarbonLib / InternetConfig.
 */
#ifndef MACOS9_IC_H
#define MACOS9_IC_H

#ifdef __MWERKS__

typedef struct OpaqueICInstance *ICInstance;
typedef long ICAttr;

extern pascal OSStatus ICStart(ICInstance *inst, OSType signature);
extern pascal OSStatus ICStop(ICInstance inst);
extern pascal OSStatus ICGetPref(ICInstance inst, ConstStr255Param key,
	ICAttr *attr, void *buf, long *size);
extern pascal OSStatus ICLaunchURL(ICInstance inst, ConstStr255Param hint,
	const void *data, long len, long *selStart, long *selEnd);

#endif /* __MWERKS__ */

#endif /* MACOS9_IC_H */
