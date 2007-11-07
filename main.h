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

#define VERSION "2.0-pre1"

#ifndef DO_DEBUG
#define DO_DEBUG 0
#endif

#define MAX_NODES		255
#define MAX_ESSIDS		255
#define MAX_BSSIDS		255
#define MAX_HISTORY		255
#define MAX_ESSID_LEN		255
#define MAX_RATES		55	/* 54M + 1 for array index */
#define MAX_FSTYPE		0xff

#define PKT_TYPE_ARP		0x001
#define PKT_TYPE_IP		0x002
#define PKT_TYPE_ICMP		0x004
#define PKT_TYPE_UDP		0x008
#define PKT_TYPE_TCP		0x010
#define PKT_TYPE_OLSR		0x020
#define PKT_TYPE_OLSR_LQ	0x040
#define PKT_TYPE_OLSR_GW	0x080
#define PKT_TYPE_BATMAN		0x100

#define WLAN_MODE_AP		0x01
#define WLAN_MODE_IBSS		0x02
#define WLAN_MODE_STA		0x04

#define NODE_TIMEOUT		60	/* seconds */

/* update display every 100ms - "10 frames per sec should be enough for everyone" ;) */
#define DISPLAY_UPDATE_INTERVAL 100000	/* usec */

#define SLEEP_TIME		1000	/* usec */

struct packet_info {
	int pkt_types;
	int signal;
	int noise;
	int snr;
	int len;
	int rate;
	int wlan_type;
	unsigned char wlan_src[6];
	unsigned char wlan_dst[6];
	unsigned char wlan_bssid[6];
	unsigned char wlan_tsf[8];
	char wlan_essid[255];
	int wlan_mode;
	unsigned char wlan_channel;
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
	int channel;
	int wlan_mode;
	unsigned long tsfl;
	unsigned long tsfh;
	int snr;
	int snr_min;
	int snr_max;
	int sig_max;
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

struct statistics {
	unsigned long packets;
	unsigned long bytes;
	unsigned long airtimes;
	unsigned long packets_per_rate[MAX_RATES];
	unsigned long bytes_per_rate[MAX_RATES];

	unsigned long packets_per_type[MAX_FSTYPE];
	unsigned long bytes_per_type[MAX_FSTYPE];
	unsigned long airtime_per_type[MAX_FSTYPE];
};

extern struct statistics stat;

struct config {
	char* ifname;
	int rport;
	int quiet;
	int node_timeout;
	int display_interval;
	int sleep_time;
	unsigned char filtermac[6];

	/* this isn't exactly config, but wtf... */
	int arphrd; // the device ARP type
	int paused;
	int do_filter;
};

extern struct config conf;

void
finish_all(int sig);

#endif
