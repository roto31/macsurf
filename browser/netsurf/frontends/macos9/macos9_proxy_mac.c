/*
 * Lotus Navigator - macos9_proxy_mac.c
 *
 * See macos9_proxy_mac.h. The probe uses the asynchronous Open Transport
 * pattern from macTLS (ostls_async.c): the notifier only sets flags at
 * interrupt time and macos9_proxy_mac_idle() does every OT call at app
 * time, so a missing proxy costs the user nothing but a log line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/nsoption.h"
#include "macos9.h"
#include "lotus_brand.h"
#include "macos9_ic.h"
#include "macos9_proxy.h"
#include "macos9_proxy_mac.h"
#include "macsurf_debug_log.h"

#include <MacTypes.h>
#include <Events.h>
#include <Menus.h>

#ifdef __MWERKS__
#include <OpenTransport.h>
#include <OpenTptInternet.h>
extern OTClientContextPtr macos9_ot_context;
#else
/* Retro68 syntax-check stubs (no Open Transport headers there); mirrors
 * the stub block in macTLS/os9/ostls_async.c. CW8 builds the real thing. */
typedef long OSStatus;
typedef long OTResult;
typedef long OTEventCode;
typedef void *EndpointRef;
typedef void *OTConfigurationRef;
typedef struct { unsigned long maxlen, len; UInt8 *buf; } TNetbuf;
typedef struct { TNetbuf addr; TNetbuf opt; TNetbuf udata; long sequence; } TCall;
typedef struct { unsigned short fAddressType; char fName[256]; } DNSAddress;
typedef void (*OTNotifyProcPtr)(void *, OTEventCode, OTResult, void *);
typedef OTNotifyProcPtr OTNotifyUPP;
#define NewOTNotifyUPP(fn) ((OTNotifyUPP)(fn))
#define kOTNoDataErr  (-3162)
#define T_OPENCOMPLETE  0x20000001L
#define T_BINDCOMPLETE  0x20000002L
#define T_CONNECT       0x00000002L
#define T_DISCONNECT    0x00000010L
static OTConfigurationRef OTCreateConfiguration(const char *s){(void)s;return (OTConfigurationRef)1;}
static OSStatus OTAsyncOpenEndpointInContext(OTConfigurationRef c,unsigned long f,void *i,OTNotifyUPP p,void *x,void *ctx){(void)c;(void)f;(void)i;(void)p;(void)x;(void)ctx;return noErr;}
static OSStatus OTBind(EndpointRef e,void *r,void *o){(void)e;(void)r;(void)o;return noErr;}
static OSStatus OTConnect(EndpointRef e,TCall *c,void *r){(void)e;(void)c;(void)r;return noErr;}
static OSStatus OTRcvConnect(EndpointRef e,TCall *c){(void)e;(void)c;return noErr;}
static OTResult OTSnd(EndpointRef e,void *b,long n,long f){(void)e;(void)b;(void)f;return n;}
static OTResult OTRcv(EndpointRef e,void *b,long n,long *f){(void)e;(void)b;(void)n;(void)f;return kOTNoDataErr;}
static OSStatus OTSetNonBlocking(EndpointRef e){(void)e;return noErr;}
static OSStatus OTCloseProvider(EndpointRef e){(void)e;return noErr;}
static long OTInitDNSAddress(DNSAddress *d,const char *s){(void)d;(void)s;return 0;}
static void OTMemzero(void *p,unsigned long n){memset(p,0,n);}
static void *macos9_ot_context = NULL;
#endif

/* Connect must finish in 1.5 s, the HEAD reply by 3 s (60 ticks/s). */
#define PROBE_CONNECT_TICKS 90UL
#define PROBE_TOTAL_TICKS   180UL
/* An abandoned open is given this long to complete so it can be closed. */
#define PROBE_DRAIN_TICKS   600UL
#define PROBE_REPLY_LEN     512
#define MENU_SIGNATURE_LEN  128

enum probe_phase {
	PROBE_IDLE,
	PROBE_OPENING,
	PROBE_BINDING,
	PROBE_CONNECTING,
	PROBE_READING,
	PROBE_DRAINING
};

struct proxy_probe {
	/* Written by the notifier at interrupt time. */
	volatile int open_done;
	volatile int bind_done;
	volatile int connect_done;
	volatile int disconnected;
	volatile OTResult result;
	EndpointRef volatile ep;
	/* App time only. */
	int phase;
	unsigned long started;
	char target[MACOS9_PROXY_TARGET_LEN];
	DNSAddress dns;
	TCall call;
	char reply[PROBE_REPLY_LEN];
	long reply_len;
};

static struct proxy_probe g_probe;
static OTNotifyUPP g_probe_upp = NULL;
static char g_menu_signature[MENU_SIGNATURE_LEN];

extern int macos9_http_fetcher_active(void);
extern int macos9_https_fetcher_active(void);

/* ------------------------------------------------------------------ */
/* Configuration                                                      */
/* ------------------------------------------------------------------ */

/* Internet Config's HTTP proxy (Internet control panel on Mac OS 9, the
 * Network preferences on Mac OS X). Returns 1 with host/port filled in. */
static int proxy_read_internet_config(char *host, int host_cap, int *port)
{
#ifdef __MWERKS__
	ICInstance ic;
	ICAttr attr;
	Boolean use_proxy = false;
	Str255 value;
	char text[256];
	long size;
	int found = 0;

	if (ICStart(&ic, LOTUS_APP_CREATOR) != noErr) return 0;
	size = (long)sizeof use_proxy;
	if (ICGetPref(ic, "\pUseHTTPProxy", &attr, &use_proxy, &size) == noErr &&
	    use_proxy) {
		size = (long)sizeof value;
		if (ICGetPref(ic, "\pHTTPProxyHost", &attr, value, &size) == noErr &&
		    size > 0) {
			memcpy(text, value + 1, value[0]);
			text[value[0]] = '\0';
			found = macos9_proxy_parse_target(text,
				nsoption_int(http_proxy_port), host, host_cap, port);
		}
	}
	ICStop(ic);
	return found;
#else
	(void)host; (void)host_cap; (void)port;
	return 0;
#endif
}

static void proxy_reload_config(void)
{
	char host[MACOS9_PROXY_HOST_LEN];
	int port = 0;
	int found = 0;
	const char *configured = nsoption_charp(http_proxy_host);

	if (configured != NULL && configured[0] != '\0') {
		found = macos9_proxy_parse_target(configured,
			nsoption_int(http_proxy_port), host, sizeof host, &port);
	}
	if (!found) {
		found = proxy_read_internet_config(host, sizeof host, &port);
	}
	macos9_proxy_configure(nsoption_bool(http_proxy) && found,
		found ? host : NULL, port);
	macos9_proxy_load_consent(nsoption_charp(http_proxy_https_consent));
	macsurf_debug_log_writef("LIFE proxy config enabled=%d target=%s",
		(int)nsoption_bool(http_proxy),
		found ? macos9_proxy_target() : "(none)");
}

static char *proxy_copy_string(const char *text)
{
	char *copy = (char *)malloc(strlen(text) + 1);
	if (copy != NULL) strcpy(copy, text);
	return copy;
}

static void proxy_store_consent(void)
{
	nsoption_set_charp(http_proxy_https_consent,
		proxy_copy_string(macos9_proxy_consent_record()));
	macos9_prefs_save();
}

/* ------------------------------------------------------------------ */
/* Probe                                                              */
/* ------------------------------------------------------------------ */

/* INTERRUPT TIME: flags only, no allocation, no Toolbox. */
static pascal void probe_notifier(void *context, OTEventCode event,
	OTResult result, void *cookie)
{
	struct proxy_probe *probe = (struct proxy_probe *)context;
	if (probe == NULL) return;
	probe->result = result;
	switch (event) {
	case T_OPENCOMPLETE:
		probe->ep = (EndpointRef)cookie;
		if (probe->ep != NULL) OTSetNonBlocking(probe->ep);
		probe->open_done = 1;
		break;
	case T_BINDCOMPLETE:
		probe->bind_done = 1;
		break;
	case T_CONNECT:
		probe->connect_done = 1;
		break;
	case T_DISCONNECT:
		probe->disconnected = 1;
		break;
	default:
		break;
	}
}

static void probe_close_endpoint(void)
{
	if (g_probe.ep != NULL) {
		OTCloseProvider(g_probe.ep);
		g_probe.ep = NULL;
	}
}

static void probe_report(int ok, int kind, const char *why)
{
	/* The setting changed while this probe ran: its answer is stale and
	 * the new target is still waiting for its own probe. */
	if (strcmp(g_probe.target, macos9_proxy_target()) != 0) return;
	macos9_proxy_probe_finished(ok, kind, (unsigned long)TickCount());
	macsurf_debug_log_writef("LIFE proxy probe %s target=%s kind=%d (%s)",
		ok ? "UP" : "DOWN", g_probe.target, kind, why);
}

static void probe_finish(int ok, int kind, const char *why)
{
	probe_close_endpoint();
	g_probe.phase = PROBE_IDLE;
	probe_report(ok, kind, why);
}

static void probe_start(unsigned long now)
{
	OTConfigurationRef cfg;
	OSStatus err;

	if (macos9_ot_context == NULL) return;
	g_probe.open_done = 0;
	g_probe.bind_done = 0;
	g_probe.connect_done = 0;
	g_probe.disconnected = 0;
	g_probe.result = noErr;
	g_probe.ep = NULL;
	g_probe.reply_len = 0;
	g_probe.reply[0] = '\0';
	g_probe.started = now;
	strcpy(g_probe.target, macos9_proxy_target());
	macos9_proxy_probe_started(now);

	if (g_probe_upp == NULL) g_probe_upp = NewOTNotifyUPP(probe_notifier);
	cfg = OTCreateConfiguration("tcp");
	if (cfg == NULL) {
		probe_finish(0, MACOS9_PROXY_KIND_NONE, "config");
		return;
	}
	err = OTAsyncOpenEndpointInContext(cfg, 0, NULL, g_probe_upp,
		&g_probe, macos9_ot_context);
	if (err != noErr) {
		probe_finish(0, MACOS9_PROXY_KIND_NONE, "open");
		return;
	}
	g_probe.phase = PROBE_OPENING;
}

static void probe_send_request(void)
{
	char request[160];
	long request_len;

	sprintf(request,
		"HEAD / HTTP/1.0\r\n"
		"Host: %s\r\n"
		"User-Agent: " LOTUS_APP_NAME " proxy check\r\n\r\n",
		g_probe.target);
	request_len = (long)strlen(request);
	if (OTSnd(g_probe.ep, request, request_len, 0) != request_len) {
		probe_finish(0, MACOS9_PROXY_KIND_NONE, "send");
		return;
	}
	g_probe.phase = PROBE_READING;
}

static void probe_read_reply(void)
{
	OTResult received;
	int kind;
	long room = PROBE_REPLY_LEN - 1 - g_probe.reply_len;

	received = OTRcv(g_probe.ep, g_probe.reply + g_probe.reply_len, room,
		NULL);
	if (received > 0) {
		g_probe.reply_len += received;
		g_probe.reply[g_probe.reply_len] = '\0';
	}
	if (strstr(g_probe.reply, "\r\n\r\n") == NULL &&
	    g_probe.reply_len < PROBE_REPLY_LEN - 1 &&
	    (received > 0 || received == kOTNoDataErr)) {
		return;
	}
	kind = macos9_proxy_kind_from_headers(g_probe.reply, g_probe.reply_len);
	probe_finish(kind != MACOS9_PROXY_KIND_NONE, kind, "head");
}

static void probe_advance(void)
{
	OSStatus err;
	long address_len;

	switch (g_probe.phase) {
	case PROBE_OPENING:
		if (!g_probe.open_done) return;
		if (g_probe.result != noErr || g_probe.ep == NULL) {
			probe_finish(0, MACOS9_PROXY_KIND_NONE, "open");
			return;
		}
		if (OTBind(g_probe.ep, NULL, NULL) != noErr) {
			probe_finish(0, MACOS9_PROXY_KIND_NONE, "bind");
			return;
		}
		g_probe.phase = PROBE_BINDING;
		return;
	case PROBE_BINDING:
		if (!g_probe.bind_done) return;
		if (g_probe.result != noErr) {
			probe_finish(0, MACOS9_PROXY_KIND_NONE, "bind");
			return;
		}
		OTMemzero(&g_probe.dns, sizeof g_probe.dns);
		OTMemzero(&g_probe.call, sizeof g_probe.call);
		address_len = (long)OTInitDNSAddress(&g_probe.dns, g_probe.target);
		g_probe.call.addr.buf = (UInt8 *)&g_probe.dns;
		g_probe.call.addr.len = address_len;
		err = OTConnect(g_probe.ep, &g_probe.call, NULL);
		if (err != noErr && err != kOTNoDataErr) {
			probe_finish(0, MACOS9_PROXY_KIND_NONE, "connect");
			return;
		}
		g_probe.phase = PROBE_CONNECTING;
		return;
	case PROBE_CONNECTING:
		if (!g_probe.connect_done) return;
		if (g_probe.result != noErr ||
		    OTRcvConnect(g_probe.ep, NULL) != noErr) {
			probe_finish(0, MACOS9_PROXY_KIND_NONE, "connect");
			return;
		}
		probe_send_request();
		return;
	case PROBE_READING:
		probe_read_reply();
		return;
	default:
		return;
	}
}

static void probe_pump(unsigned long now)
{
	unsigned long elapsed = now - g_probe.started;
	unsigned long limit;

	if (g_probe.phase == PROBE_IDLE) {
		if (macos9_proxy_probe_due(now)) probe_start(now);
		return;
	}
	if (g_probe.phase == PROBE_DRAINING) {
		if (g_probe.open_done || elapsed > PROBE_DRAIN_TICKS) {
			probe_close_endpoint();
			g_probe.phase = PROBE_IDLE;
		}
		return;
	}
	if (g_probe.disconnected) {
		probe_finish(0, MACOS9_PROXY_KIND_NONE, "refused");
		return;
	}
	limit = (g_probe.phase == PROBE_READING) ?
		PROBE_TOTAL_TICKS : PROBE_CONNECT_TICKS;
	if (elapsed > limit) {
		if (g_probe.phase == PROBE_OPENING && g_probe.ep == NULL) {
			/* The endpoint does not exist yet; close it once it does. */
			g_probe.phase = PROBE_DRAINING;
			probe_report(0, MACOS9_PROXY_KIND_NONE, "timeout");
			return;
		}
		probe_finish(0, MACOS9_PROXY_KIND_NONE, "timeout");
		return;
	}
	probe_advance();
}

/* ------------------------------------------------------------------ */
/* Consent and menu                                                   */
/* ------------------------------------------------------------------ */

static void proxy_ask_https_consent(void)
{
	char question[160];
	int yes;
	int remember = 1;

	sprintf(question, "Use the proxy at %s for secure sites too?",
		macos9_proxy_target());
	yes = macos9_chrome_ask_with_checkbox(question,
		"Secure pages load much faster this way. Traffic between this "
		"Mac and the proxy is not encrypted, so only allow it on a "
		"network you trust. You can change this in the View menu.",
		"Use Proxy", "Don't Use", "Remember for this proxy", &remember);
	macos9_proxy_set_https_consent(yes);
	if (remember) proxy_store_consent();
	macsurf_debug_log_writef("LIFE proxy https consent=%d remember=%d target=%s",
		yes, remember, macos9_proxy_target());
}

static void proxy_set_server(void)
{
	char text[96];
	char host[MACOS9_PROXY_HOST_LEN];
	int port = 0;
	const char *current = macos9_proxy_target();

	if (!macos9_chrome_prompt_line("Proxy Server (address:port)",
			current[0] != '\0' ? current : "", text, sizeof text)) {
		return;
	}
	if (!macos9_proxy_parse_target(text, nsoption_int(http_proxy_port),
			host, sizeof host, &port) ||
	    !macos9_proxy_host_is_private(host)) {
		macos9_chrome_ask("That is not a local network proxy address.",
			"Enter the proxy's numeric address on this network, "
			"for example 192.168.1.120:8081.", "OK", NULL);
		return;
	}
	nsoption_set_charp(http_proxy_host, proxy_copy_string(host));
	nsoption_set_int(http_proxy_port, port);
	nsoption_set_bool(http_proxy, true);
	macos9_prefs_save();
	proxy_reload_config();
}

static void proxy_menu_refresh(void)
{
	MenuHandle menu = GetMenuHandle(MENU_VIEW);
	char status[96];
	char signature[MENU_SIGNATURE_LEN];
	Str255 status_pstr;
	size_t status_len;
	int secure = (macos9_proxy_https_consent() == MACOS9_PROXY_CONSENT_YES);
	int has_target = (macos9_proxy_target()[0] != '\0');

	if (menu == NULL) return;
	macos9_proxy_describe(status, sizeof status);
	sprintf(signature, "%d%d%d|%s", (int)nsoption_bool(http_proxy), secure,
		has_target, status);
	if (strcmp(signature, g_menu_signature) == 0) return;
	strcpy(g_menu_signature, signature);

	CheckMenuItem(menu, ITEM_VIEW_PROXY_USE, nsoption_bool(http_proxy));
	CheckMenuItem(menu, ITEM_VIEW_PROXY_SECURE, secure);
	if (has_target) EnableMenuItem(menu, ITEM_VIEW_PROXY_SECURE);
	else DisableMenuItem(menu, ITEM_VIEW_PROXY_SECURE);
	status_len = strlen(status);
	status_pstr[0] = (unsigned char)status_len;
	memcpy(status_pstr + 1, status, status_len);
	SetMenuItemText(menu, ITEM_VIEW_PROXY_STATUS, status_pstr);
	DisableMenuItem(menu, ITEM_VIEW_PROXY_STATUS);
}

/* ------------------------------------------------------------------ */
/* Public entry points                                                */
/* ------------------------------------------------------------------ */

void macos9_proxy_mac_init(void)
{
	g_probe.phase = PROBE_IDLE;
	g_menu_signature[0] = '\0';
	proxy_reload_config();
}

void macos9_proxy_mac_idle(void)
{
	probe_pump((unsigned long)TickCount());
	/* A modal question mid-load would stall in-flight fetches into
	 * their no-progress timeouts, so wait for a quiet moment. */
	if (macos9_http_fetcher_active() == 0 &&
	    macos9_https_fetcher_active() == 0 &&
	    macos9_proxy_take_consent_prompt()) {
		proxy_ask_https_consent();
	}
	proxy_menu_refresh();
}

void macos9_proxy_mac_menu_command(int item)
{
	switch (item) {
	case ITEM_VIEW_PROXY_USE:
		nsoption_set_bool(http_proxy, !nsoption_bool(http_proxy));
		macos9_prefs_save();
		proxy_reload_config();
		break;
	case ITEM_VIEW_PROXY_SECURE:
		if (macos9_proxy_target()[0] == '\0') break;
		macos9_proxy_set_https_consent(
			macos9_proxy_https_consent() != MACOS9_PROXY_CONSENT_YES);
		proxy_store_consent();
		break;
	case ITEM_VIEW_PROXY_SET:
		proxy_set_server();
		break;
	default:
		break;
	}
	proxy_menu_refresh();
}
