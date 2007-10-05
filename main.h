/* olsr scanning tool
 *
 * Copyright (C) 2005  Bruno Randolf
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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <time.h>

#define DO_DEBUG 0

#if DO_DEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

struct packet_info {
	int pkt_types;
	int prism_signal;
	int prism_noise;
	int snr;
	int wlan_type;
	int wlan_stype;
	unsigned char wlan_src[6];
	unsigned char wlan_dst[6];
	unsigned char wlan_bssid[6];
	unsigned char wlan_tsf[8];
	unsigned char wlan_essid[255];
	unsigned int ip_src;
	unsigned int ip_dst;
	int olsr_type;
	int olsr_neigh;
	int olsr_tc;
};

extern struct packet_info current_packet;

#define MAX_NODES 255

#define PKT_TYPE_BEACON		0x01
#define PKT_TYPE_PROBE_REQ	0x02
#define PKT_TYPE_DATA		0x04
#define PKT_TYPE_IP		0x08
#define PKT_TYPE_OLSR		0x10
#define PKT_TYPE_OLSR_LQ	0x20
#define PKT_TYPE_OLSR_GW	0x40

struct node_info {
	int status;
	int pkt_types;
	unsigned int ip_src;
	struct packet_info last_pkt;
	time_t last_seen;
	int olsr_neigh;
	int olsr_tc;
	int pkt_count;
	int olsr_count;
	unsigned char wlan_bssid[6];
	unsigned long tsfl;
	unsigned long tsfh;
};

extern struct node_info nodes[MAX_NODES];

extern int paused;
extern int olsr_only;
extern int no_ctrl;

#define NODE_TIMEOUT 60 /* seconds */

void finish_all(int sig);

#endif
