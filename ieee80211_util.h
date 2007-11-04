/* horst - olsr scanning tool
 *
 * Copyright (C) 2005-2007  Bruno Randolf
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _IEEE80211_UTIL_H_
#define _IEEE80211_UTIL_H_

struct packet_info;

int
ieee80211_get_hdrlen(u16 fc);

u8*
ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len);

void
ieee802_11_parse_elems(unsigned char *start, int len, struct packet_info *pkt);

#endif
