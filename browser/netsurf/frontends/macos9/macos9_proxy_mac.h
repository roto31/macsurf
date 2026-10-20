/*
 * Lotus Navigator - macos9_proxy_mac.h
 *
 * Mac side of LAN proxy support: reads the proxy setting, checks it with
 * a non-blocking Open Transport probe, asks once before secure sites use
 * it, and drives the View menu items. Routing policy is macos9_proxy.h.
 */
#ifndef MACOS9_PROXY_MAC_H
#define MACOS9_PROXY_MAC_H

/* After preferences load: apply the proxy setting and stored consent. */
void macos9_proxy_mac_init(void);

/* Every event-loop pass, outside fetch_pump: advances the probe, shows
 * the one-time secure-site question when the browser is idle, and keeps
 * the View menu current. */
void macos9_proxy_mac_idle(void);

/* View menu items ITEM_VIEW_PROXY_* (macos9.h). */
void macos9_proxy_mac_menu_command(int item);

#endif
