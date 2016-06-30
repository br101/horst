/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2016 Bruno Randolf (br1@einfach.org)
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

#ifndef _WLAN_UTIL_H_
#define _WLAN_UTIL_H_

#include <stdbool.h>
#include <stdint.h>

#include "wlan80211.h"

enum chan_width;

struct pkt_name {
	const char c;
	const char* name;
	const uint16_t fc;
	const char* desc;
};

/*
 * Names and abbreviations for all WLAN frame types (2 bit, but only MGMT, CTRL
 * and DATA defined) and subtypes (4 bit)
 */
extern struct pkt_name stype_names[WLAN_NUM_TYPES][WLAN_NUM_STYPES];

struct pkt_name get_packet_struct(uint16_t type);
char get_packet_type_char(uint16_t type);
const char* get_packet_type_name(uint16_t type);
int rate_to_index(int rate);
int rate_index_to_rate(int idx);
int mcs_index_to_rate(int mcs, bool ht20, bool lgi);
int vht_mcs_index_to_rate(enum chan_width width, int streams, int mcs, bool sgi);
enum chan_width chan_width_from_vht_capab(uint32_t vht);
void ht_streams_from_mcs_set(unsigned char* mcs, unsigned char* rx, unsigned char* tx);
void vht_streams_from_mcs_set(unsigned char* mcs, unsigned char* rx, unsigned char* tx);
const char* get_80211std(enum chan_width width, int chan);
int get_phy_thruput(enum chan_width width, unsigned char streams_rx);

#endif
