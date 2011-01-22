/* copied from linux wireless-2.6/net/mac80211/util.c */

/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * utilities for mac80211
 */

#ifndef _IEEE80211_UTIL_H_
#define _IEEE80211_UTIL_H_

#include "ieee80211.h"


struct packet_info;

int
ieee80211_get_hdrlen(u16 fc);

u8*
ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len);

void
ieee802_11_parse_elems(unsigned char *start, size_t len, struct packet_info *pkt);

int
ieee80211_frame_duration(int phymode, size_t len, int rate, int short_preamble,
			 int ackcts, int shortslot, char qos_class, int retries);

int
ieee80211_frequency_to_channel(int freq);

#endif
