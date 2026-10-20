/*
 * Lotus Navigator - macos9_proxy.c
 *
 * Proxy routing policy (see macos9_proxy.h). Portable C89: no Toolbox
 * calls, no snprintf (CW8's MSL has none), declarations at block top.
 */
#include <stdio.h>
#include <string.h>

#include "macos9_proxy.h"

#define MAX_PORT 65535

static int g_state = MACOS9_PROXY_UNCONFIGURED;
static int g_kind = MACOS9_PROXY_KIND_NONE;
static char g_host[MACOS9_PROXY_HOST_LEN];
static int g_port = 0;
static char g_target[MACOS9_PROXY_TARGET_LEN];
static int g_probe_in_flight = 0;
static unsigned long g_state_since = 0;
static char g_consent_record[MACOS9_PROXY_TARGET_LEN + 2];
static int g_consent_prompted = 0;
static char g_cookie_hosts[MACOS9_PROXY_COOKIE_HOSTS][MACOS9_PROXY_COOKIE_HOST_LEN];
static int g_cookie_next = 0;

static int ascii_lower(int c)
{
	return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static int is_digit(int c)
{
	return c >= '0' && c <= '9';
}

static int is_host_char(int c)
{
	return is_digit(c) || (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') || c == '.' || c == '-';
}

static int prefix_equals_ci(const char *text, const char *prefix)
{
	while (*prefix != '\0') {
		if (ascii_lower((unsigned char)*text) !=
				ascii_lower((unsigned char)*prefix)) {
			return 0;
		}
		text++;
		prefix++;
	}
	return 1;
}

static int strings_equal_ci(const char *a, const char *b)
{
	return strlen(a) == strlen(b) && prefix_equals_ci(a, b);
}

/* Case-insensitive search for needle in the first len bytes of text. */
static int contains_ci(const char *text, long len, const char *needle)
{
	long needle_len = (long)strlen(needle);
	long i;
	long j;
	for (i = 0; i + needle_len <= len; i++) {
		for (j = 0; j < needle_len; j++) {
			if (ascii_lower((unsigned char)text[i + j]) !=
					ascii_lower((unsigned char)needle[j])) {
				break;
			}
		}
		if (j == needle_len) return 1;
	}
	return 0;
}

int macos9_proxy_parse_target(const char *text, int default_port,
	char *host_out, int host_cap, int *port_out)
{
	const char *p;
	int host_len = 0;
	long port = 0;
	int port_digits = 0;

	if (text == NULL || host_out == NULL || host_cap < 2 || port_out == NULL)
		return 0;
	p = text;
	while (*p == ' ') p++;
	if (prefix_equals_ci(p, "http://")) p += 7;
	while (is_host_char((unsigned char)p[host_len])) {
		if (host_len + 1 >= host_cap) return 0;
		host_out[host_len] = p[host_len];
		host_len++;
	}
	host_out[host_len] = '\0';
	if (host_len == 0) return 0;
	p += host_len;
	if (*p == ':') {
		p++;
		while (is_digit((unsigned char)*p)) {
			port = port * 10 + (*p - '0');
			if (port > MAX_PORT) return 0;
			port_digits++;
			p++;
		}
		if (port_digits == 0 || port == 0) return 0;
	} else {
		port = default_port;
	}
	while (*p == ' ') p++;
	if (*p != '\0' && *p != '/') return 0;
	if (port <= 0 || port > MAX_PORT) return 0;
	*port_out = (int)port;
	return 1;
}

int macos9_proxy_host_is_private(const char *host)
{
	int octets[4];
	int count = 0;
	int value;
	int digits;
	const char *p = host;

	if (host == NULL) return 0;
	while (count < 4) {
		value = 0;
		digits = 0;
		while (is_digit((unsigned char)*p)) {
			value = value * 10 + (*p - '0');
			if (value > 255 || ++digits > 3) return 0;
			p++;
		}
		if (digits == 0) return 0;
		octets[count++] = value;
		if (count < 4) {
			if (*p != '.') return 0;
			p++;
		}
	}
	if (*p != '\0') return 0;
	return octets[0] == 10 ||
		octets[0] == 127 ||
		(octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) ||
		(octets[0] == 192 && octets[1] == 168) ||
		(octets[0] == 169 && octets[1] == 254);
}

int macos9_proxy_kind_from_headers(const char *headers, long len)
{
	long line_start = 0;
	long i;

	if (headers == NULL || len < 12 || !prefix_equals_ci(headers, "HTTP/1."))
		return MACOS9_PROXY_KIND_NONE;
	for (i = 0; i <= len; i++) {
		if (i == len || headers[i] == '\n') {
			const char *line = headers + line_start;
			long line_len = i - line_start;
			if ((line_len > 7 && prefix_equals_ci(line, "Server:")) ||
			    (line_len > 4 && prefix_equals_ci(line, "Via:"))) {
				if (contains_ci(line, line_len, "webone"))
					return MACOS9_PROXY_KIND_WEBONE;
				if (contains_ci(line, line_len, "squid"))
					return MACOS9_PROXY_KIND_SQUID;
			}
			line_start = i + 1;
		}
	}
	return MACOS9_PROXY_KIND_GENERIC;
}

static int consent_for_current_target(void)
{
	if (g_target[0] == '\0') return MACOS9_PROXY_CONSENT_UNASKED;
	if (strcmp(g_consent_record, g_target) == 0)
		return MACOS9_PROXY_CONSENT_YES;
	if (g_consent_record[0] == '!' &&
	    strcmp(g_consent_record + 1, g_target) == 0)
		return MACOS9_PROXY_CONSENT_NO;
	return MACOS9_PROXY_CONSENT_UNASKED;
}

static void clear_cookie_hosts(void)
{
	int i;
	for (i = 0; i < MACOS9_PROXY_COOKIE_HOSTS; i++) g_cookie_hosts[i][0] = '\0';
	g_cookie_next = 0;
}

static int is_cookie_host(const char *host)
{
	int i;
	for (i = 0; i < MACOS9_PROXY_COOKIE_HOSTS; i++) {
		if (g_cookie_hosts[i][0] != '\0' &&
		    strings_equal_ci(g_cookie_hosts[i], host)) {
			return 1;
		}
	}
	return 0;
}

static void reset_unconfigured(void)
{
	clear_cookie_hosts();
	g_state = MACOS9_PROXY_UNCONFIGURED;
	g_kind = MACOS9_PROXY_KIND_NONE;
	g_host[0] = '\0';
	g_target[0] = '\0';
	g_port = 0;
	g_probe_in_flight = 0;
	g_consent_prompted = 0;
}

void macos9_proxy_configure(int enabled, const char *host, int port)
{
	char new_target[MACOS9_PROXY_TARGET_LEN];

	if (!enabled || host == NULL || host[0] == '\0' ||
	    port <= 0 || port > MAX_PORT ||
	    strlen(host) >= MACOS9_PROXY_HOST_LEN ||
	    !macos9_proxy_host_is_private(host)) {
		reset_unconfigured();
		return;
	}
	sprintf(new_target, "%s:%d", host, port);
	if (g_state != MACOS9_PROXY_UNCONFIGURED &&
	    strcmp(new_target, g_target) == 0) {
		return;
	}
	clear_cookie_hosts();
	strcpy(g_host, host);
	strcpy(g_target, new_target);
	g_port = port;
	g_state = MACOS9_PROXY_UNKNOWN;
	g_kind = MACOS9_PROXY_KIND_NONE;
	g_probe_in_flight = 0;
	g_consent_prompted = 0;
}

void macos9_proxy_load_consent(const char *stored)
{
	g_consent_record[0] = '\0';
	if (stored != NULL && strlen(stored) < sizeof g_consent_record)
		strcpy(g_consent_record, stored);
}

void macos9_proxy_set_https_consent(int yes)
{
	if (g_target[0] == '\0') return;
	sprintf(g_consent_record, "%s%s", yes ? "" : "!", g_target);
	g_consent_prompted = 1;
}

int macos9_proxy_https_consent(void)
{
	return consent_for_current_target();
}

const char *macos9_proxy_consent_record(void)
{
	return g_consent_record;
}

int macos9_proxy_probe_due(unsigned long now)
{
	if (g_state == MACOS9_PROXY_UNCONFIGURED || g_probe_in_flight)
		return 0;
	if (g_state == MACOS9_PROXY_UNKNOWN) return 1;
	if (g_state == MACOS9_PROXY_DOWN)
		return (now - g_state_since) >= MACOS9_PROXY_COOLOFF_TICKS;
	return (now - g_state_since) >= MACOS9_PROXY_RECHECK_TICKS;
}

void macos9_proxy_probe_started(unsigned long now)
{
	(void)now;
	if (g_state != MACOS9_PROXY_UNCONFIGURED) g_probe_in_flight = 1;
}

void macos9_proxy_probe_finished(int ok, int kind, unsigned long now)
{
	g_probe_in_flight = 0;
	if (g_state == MACOS9_PROXY_UNCONFIGURED) return;
	g_state_since = now;
	if (ok) {
		g_state = MACOS9_PROXY_UP;
		g_kind = (kind == MACOS9_PROXY_KIND_NONE) ?
			MACOS9_PROXY_KIND_GENERIC : kind;
	} else {
		g_state = MACOS9_PROXY_DOWN;
		g_kind = MACOS9_PROXY_KIND_NONE;
	}
}

void macos9_proxy_note_failure(unsigned long now)
{
	if (g_state == MACOS9_PROXY_UNCONFIGURED) return;
	g_state = MACOS9_PROXY_DOWN;
	g_kind = MACOS9_PROXY_KIND_NONE;
	g_state_since = now;
}

/* noproxy_list: entries separated by commas or spaces. "example.com"
 * matches the host and its subdomains; ".example.com" only subdomains. */
static int host_in_noproxy_list(const char *host, const char *list)
{
	char entry[MACOS9_PROXY_HOST_LEN];
	size_t host_len = strlen(host);
	size_t entry_len;
	const char *p = list;

	if (list == NULL) return 0;
	while (*p != '\0') {
		while (*p == ',' || *p == ' ') p++;
		entry_len = 0;
		while (p[entry_len] != '\0' && p[entry_len] != ',' &&
		       p[entry_len] != ' ') {
			entry_len++;
		}
		if (entry_len > 0 && entry_len < sizeof entry) {
			memcpy(entry, p, entry_len);
			entry[entry_len] = '\0';
			if (entry[0] != '.' && strings_equal_ci(host, entry))
				return 1;
			if (host_len > entry_len) {
				const char *tail = host + host_len - entry_len;
				if (entry[0] == '.' && strings_equal_ci(tail, entry))
					return 1;
				if (entry[0] != '.' && tail[-1] == '.' &&
				    strings_equal_ci(tail, entry))
					return 1;
			}
		}
		p += entry_len;
	}
	return 0;
}

int macos9_proxy_route(const char *host, int is_https, int has_body,
	const char *noproxy_list)
{
	if (g_state != MACOS9_PROXY_UP) return 0;
	if (host == NULL || host[0] == '\0') return 0;
	if (macos9_proxy_host_is_private(host) ||
	    strings_equal_ci(host, "localhost") ||
	    host_in_noproxy_list(host, noproxy_list)) {
		return 0;
	}
	if (macos9_proxy_mangles_cookies() &&
	    (has_body || strlen(host) >= MACOS9_PROXY_COOKIE_HOST_LEN ||
	     is_cookie_host(host))) {
		return 0;
	}
	if (!is_https) return 1;
	return g_kind == MACOS9_PROXY_KIND_WEBONE &&
		consent_for_current_target() == MACOS9_PROXY_CONSENT_YES &&
		!has_body;
}

int macos9_proxy_mangles_cookies(void)
{
	return g_kind == MACOS9_PROXY_KIND_WEBONE;
}

static int is_cookie_attribute(const char *name, long len)
{
	static const char *const attributes[] = {
		"expires", "max-age", "domain", "path", "secure", "httponly",
		"samesite", "priority", "partitioned"
	};
	size_t i;
	for (i = 0; i < sizeof attributes / sizeof attributes[0]; i++) {
		if ((long)strlen(attributes[i]) == len &&
		    prefix_equals_ci(name, attributes[i])) {
			return 1;
		}
	}
	return 0;
}

int macos9_proxy_set_cookie_merged(const char *value, long len)
{
	long seg_end = 0;
	long start;
	long end;
	long eq;
	long j;
	int first = 1;

	if (value == NULL) return 0;
	while (seg_end <= len) {
		start = seg_end;
		while (seg_end < len && value[seg_end] != ';') seg_end++;
		end = seg_end;
		while (start < end && value[start] == ' ') start++;
		while (end > start && value[end - 1] == ' ') end--;
		if (end > start) {
			eq = start;
			while (eq < end && value[eq] != '=') eq++;
			if (first) {
				for (j = eq + 1; j + 1 < end; j++) {
					if (value[j] == '=' && value[j + 1] != '=')
						return 1;
				}
			} else {
				if (!is_cookie_attribute(value + start, eq - start))
					return 1;
				for (j = eq + 1; j < end; j++) {
					if (value[j] == '=') return 1;
				}
			}
			first = 0;
		}
		seg_end++;
	}
	return 0;
}

void macos9_proxy_note_cookie_host(const char *host)
{
	if (host == NULL || host[0] == '\0' ||
	    strlen(host) >= MACOS9_PROXY_COOKIE_HOST_LEN || is_cookie_host(host)) {
		return;
	}
	strcpy(g_cookie_hosts[g_cookie_next], host);
	g_cookie_next = (g_cookie_next + 1) % MACOS9_PROXY_COOKIE_HOSTS;
}

int macos9_proxy_take_consent_prompt(void)
{
	if (g_state != MACOS9_PROXY_UP || g_kind != MACOS9_PROXY_KIND_WEBONE ||
	    g_consent_prompted ||
	    consent_for_current_target() != MACOS9_PROXY_CONSENT_UNASKED) {
		return 0;
	}
	g_consent_prompted = 1;
	return 1;
}

int macos9_proxy_state(void) { return g_state; }
int macos9_proxy_kind(void) { return g_kind; }
const char *macos9_proxy_target(void) { return g_target; }
const char *macos9_proxy_host(void) { return g_host; }
int macos9_proxy_port(void) { return g_port; }

static const char *kind_name(int kind)
{
	switch (kind) {
	case MACOS9_PROXY_KIND_WEBONE: return "WebOne";
	case MACOS9_PROXY_KIND_SQUID: return "Squid";
	default: return "Proxy";
	}
}

void macos9_proxy_describe(char *out, int cap)
{
	char text[160];

	if (out == NULL || cap < 1) return;
	switch (g_state) {
	case MACOS9_PROXY_UNKNOWN:
		sprintf(text, "Proxy: checking %s", g_target);
		break;
	case MACOS9_PROXY_UP:
		sprintf(text, "Proxy: %s %s%s", kind_name(g_kind), g_target,
			(g_kind == MACOS9_PROXY_KIND_WEBONE &&
			 consent_for_current_target() ==
				MACOS9_PROXY_CONSENT_YES) ?
				" (incl. secure sites)" : "");
		break;
	case MACOS9_PROXY_DOWN:
		sprintf(text, "Proxy: %s not answering (direct)", g_target);
		break;
	default:
		strcpy(text, "Proxy: none (direct)");
		break;
	}
	strncpy(out, text, (size_t)cap - 1);
	out[cap - 1] = '\0';
}
