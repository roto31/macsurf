/*
 * Lotus Navigator - macos9_proxy.h
 *
 * Optional LAN proxy acceleration. The browser never depends on a proxy:
 * a configured proxy is probed in the background, used only while it
 * answers, and dropped (with a cool-off) the moment it fails. Every
 * request then goes direct exactly as before.
 *
 * This header and macos9_proxy.c are portable C89 with no Toolbox
 * dependencies, so the routing policy is host-unit-tested. The Open
 * Transport probe and the UI live in macos9_proxy_mac.c.
 *
 * Time is passed in as 60 Hz ticks (TickCount on the Mac).
 */
#ifndef MACOS9_PROXY_H
#define MACOS9_PROXY_H

#define MACOS9_PROXY_HOST_LEN    64
#define MACOS9_PROXY_TARGET_LEN  80

/* 60 Hz ticks */
#define MACOS9_PROXY_COOLOFF_TICKS   3600UL   /* 60 s direct-only after a failure */
#define MACOS9_PROXY_RECHECK_TICKS   7200UL   /* re-probe a healthy proxy every 120 s */

enum macos9_proxy_kind {
	MACOS9_PROXY_KIND_NONE = 0,
	MACOS9_PROXY_KIND_GENERIC,   /* answered, but not recognised: http only */
	MACOS9_PROXY_KIND_SQUID,     /* http only: rejects absolute https:// URLs */
	MACOS9_PROXY_KIND_WEBONE     /* fetches https:// for the client */
};

enum macos9_proxy_state {
	MACOS9_PROXY_UNCONFIGURED = 0,
	MACOS9_PROXY_UNKNOWN,        /* configured, probe not finished yet */
	MACOS9_PROXY_UP,
	MACOS9_PROXY_DOWN
};

enum macos9_proxy_consent {
	MACOS9_PROXY_CONSENT_UNASKED = -1,
	MACOS9_PROXY_CONSENT_NO = 0,
	MACOS9_PROXY_CONSENT_YES = 1
};

/* Parse "host:port", "host" (port defaults to default_port) or
 * "http://host:port/". Returns 1 on success. */
int macos9_proxy_parse_target(const char *text, int default_port,
	char *host_out, int host_cap, int *port_out);

/* 1 for dotted-quad IPv4 in 10/8, 172.16/12, 192.168/16 or 169.254/16.
 * Hostnames are rejected: a name could resolve anywhere. */
int macos9_proxy_host_is_private(const char *host);

/* Classify a probe response from its raw header block. */
int macos9_proxy_kind_from_headers(const char *headers, long len);

/* (Re)configure. A disabled, unparseable or non-private proxy leaves the
 * module UNCONFIGURED. Changing the target resets the state to UNKNOWN. */
void macos9_proxy_configure(int enabled, const char *host, int port);

/* Consent record as stored in the http_proxy_https_consent option:
 * "<target>" = yes, "!<target>" = no, anything else = unasked. */
void macos9_proxy_load_consent(const char *stored);
void macos9_proxy_set_https_consent(int yes);
int  macos9_proxy_https_consent(void);
const char *macos9_proxy_consent_record(void);

/* Probe scheduling. */
int  macos9_proxy_probe_due(unsigned long now);
void macos9_proxy_probe_started(unsigned long now);
void macos9_proxy_probe_finished(int ok, int kind, unsigned long now);

/* A request through the proxy could not connect or send. */
void macos9_proxy_note_failure(unsigned long now);

/* Routing: 1 if a request for host should go through the proxy now.
 * HTTPS needs a WebOne proxy and stored consent, and never carries a
 * request body (logins stay end-to-end TLS). */
int macos9_proxy_route(const char *host, int is_https, int has_body,
	const char *noproxy_list);

/* 1 once per proxy target when an https-capable proxy is up and the
 * user has not been asked yet. Calling it marks the question as asked. */
int macos9_proxy_take_consent_prompt(void);

int macos9_proxy_state(void);
int macos9_proxy_kind(void);
const char *macos9_proxy_target(void);   /* "host:port", "" if unconfigured */
const char *macos9_proxy_host(void);
int macos9_proxy_port(void);

/* Menu/status text, e.g. "Proxy: WebOne 192.168.1.120:8081". */
void macos9_proxy_describe(char *out, int cap);

#endif /* MACOS9_PROXY_H */
