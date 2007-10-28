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

#ifndef _MAIN_H_
#define _MAIN_H_

#ifndef DO_DEBUG
#define DO_DEBUG 0
#endif

#define MAX_NODES 255
#define MAX_ESSIDS 255
#define MAX_BSSIDS 255
#define MAX_HISTORY 255
#define MAX_ESSID_LEN 255

#define PKT_TYPE_BEACON		0x01
#define PKT_TYPE_PROBE_REQ	0x02
#define PKT_TYPE_DATA		0x04
#define PKT_TYPE_IP		0x08
#define PKT_TYPE_OLSR		0x10
#define PKT_TYPE_OLSR_LQ	0x20
#define PKT_TYPE_OLSR_GW	0x40

#define WLAN_MODE_AP		0x01
#define WLAN_MODE_IBSS		0x02
#define WLAN_MODE_STA		0x04

#define NODE_TIMEOUT 60 /* seconds */

struct packet_info {
	int pkt_types;
	int signal;
	int noise;
	int snr;
	int rate;
	int wlan_type;
	int wlan_stype;
	unsigned char wlan_src[6];
	unsigned char wlan_dst[6];
	unsigned char wlan_bssid[6];
	unsigned char wlan_tsf[8];
	char wlan_essid[255];
	int wlan_mode;
	unsigned int ip_src;
	unsigned int ip_dst;
	int olsr_type;
	int olsr_neigh;
	int olsr_tc;
};

extern struct packet_info current_packet;

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
	int wlan_mode;
	unsigned long tsfl;
	unsigned long tsfh;
	int snr_min;
	int snr_max;
	int essid;
};

extern struct node_info nodes[MAX_NODES];

struct essid_info {
	char essid[MAX_ESSID_LEN];
	int nodes[MAX_NODES];
	int num_nodes;
	int split;
};

extern struct essid_info essids[MAX_ESSIDS];

struct split_info {
	int essid[MAX_ESSIDS];
	int count;
};

extern struct split_info splits;

struct history {
	int signal[MAX_HISTORY];
	int noise[MAX_HISTORY];
	int rate[MAX_HISTORY];
	int type[MAX_HISTORY];
	int index;
};

extern struct history hist;

extern int paused;
extern int olsr_only;
extern int no_ctrl;
extern int arphrd; // the device ARP type


void
finish_all(int sig);

#endif
