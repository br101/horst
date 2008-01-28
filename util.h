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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>

#if DO_DEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

void
dump_packet(const unsigned char* buf, int len);

const char*
ether_sprintf(const unsigned char *mac);

const char*
ip_sprintf(const unsigned int ip);

void
convert_string_to_mac(const char* string, unsigned char* mac);

int
normalize(float val, int max_val, int max);

#define normalize_db(_val, _max) \
	normalize((_val) - 30, 70, (_max))

char
get_packet_type_char(int type);

const char*
get_packet_type_name(int type);

const char*
kilo_mega_ize(unsigned int val);

#define MAC_NOT_EMPTY(_mac) (_mac[0] || _mac[1] || _mac[2] || _mac[3] || _mac[4] || _mac[5])
#define MAC_EMPTY(_mac) (!_mac[0] && !_mac[1] && !_mac[2] && !_mac[3] && !_mac[4] && !_mac[5])

#define TOGGLE_BIT(_x, _m) (_x) ^= (_m)

#define max(x,y) ((x) > (y) ? (x) : (y))

#endif
