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

#define MAC_LEN			6

#define MAX_NODES		255
#define MAX_ESSIDS		255
#define MAX_BSSIDS		255
#define MAX_HISTORY		255
#define MAX_ESSID_LEN		255
#define MAX_RATES		55	/* 54M + 1 for array index */
#define MAX_FSTYPE		0xff
#define MAX_FILTERMAC		9

/* packet types we actually care about, e.g filter */
#define PKT_TYPE_CTRL		0x000001
#define PKT_TYPE_MGMT		0x000002
#define PKT_TYPE_DATA		0x000004

#define PKT_TYPE_BEACON		0x000010
#define PKT_TYPE_PROBE		0x000020
#define PKT_TYPE_ASSOC		0x000040
#define PKT_TYPE_AUTH		0x000080
#define PKT_TYPE_RTS		0x000100
#define PKT_TYPE_CTS		0x000200
#define PKT_TYPE_ACK		0x000400
#define PKT_TYPE_NULL		0x000800

#define PKT_TYPE_ARP		0x001000
#define PKT_TYPE_IP		0x002000
#define PKT_TYPE_ICMP		0x004000
#define PKT_TYPE_UDP		0x008000
#define PKT_TYPE_TCP		0x010000
#define PKT_TYPE_OLSR		0x020000
#define PKT_TYPE_OLSR_LQ	0x040000
#define PKT_TYPE_OLSR_GW	0x080000
#define PKT_TYPE_BATMAN		0x100000

#define PKT_TYPE_ALL_MGMT	(PKT_TYPE_BEACON | PKT_TYPE_PROBE | PKT_TYPE_ASSOC | PKT_TYPE_AUTH)
#define PKT_TYPE_ALL_CTRL	(PKT_TYPE_RTS | PKT_TYPE_CTS | PKT_TYPE_ACK)
#define PKT_TYPE_ALL_DATA	(PKT_TYPE_NULL | PKT_TYPE_ARP | PKT_TYPE_ICMP | PKT_TYPE_IP | \
				 PKT_TYPE_UDP | PKT_TYPE_TCP | PKT_TYPE_OLSR | PKT_TYPE_OLSR_LQ | \
				 PKT_TYPE_OLSR_GW | PKT_TYPE_BATMAN)

#define WLAN_MODE_AP		0x01
#define WLAN_MODE_IBSS		0x02
#define WLAN_MODE_STA		0x04
#define WLAN_MODE_PROBE		0x08

#define PHY_FLAG_SHORTPRE	0x0001
#define PHY_FLAG_A		0x0010
#define PHY_FLAG_B		0x0020
#define PHY_FLAG_G		0x0040
#define PHY_FLAG_MODE_MASK	0x00f0

/* default config vlaues */
#define INTERFACE_NAME		"wlan0"
#define NODE_TIMEOUT		60	/* seconds */
/* update display every 100ms - "10 frames per sec should be enough for everyone" ;) */
#define DISPLAY_UPDATE_INTERVAL 100000	/* usec */
#define SLEEP_TIME		1000	/* usec */
#define RECV_BUFFER_SIZE	6750000 /* 54Mbps in byte */


struct packet_info {
	int pkt_types;
	int signal;
	int noise;
	int snr;
	int len;
	int rate;
	int phy_freq;
	int phy_flags;
	int wlan_type;
	unsigned char wlan_src[MAC_LEN];
	unsigned char wlan_dst[MAC_LEN];
	unsigned char wlan_bssid[MAC_LEN];
	unsigned char wlan_tsf[8];
	char wlan_essid[255];
	int wlan_mode;
	unsigned char wlan_channel;
	int wlan_wep;
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
	unsigned char wlan_bssid[MAC_LEN];
	int channel;
	int wlan_mode;
	unsigned long tsfl;
	unsigned long tsfh;
	int snr;
	int snr_min;
	int snr_max;
	int sig_max;
	int essid;
	int wep;
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
	unsigned long symbols;
	unsigned long duration;

	unsigned long packets_per_rate[MAX_RATES];
	unsigned long bytes_per_rate[MAX_RATES];

	unsigned long packets_per_type[MAX_FSTYPE];
	unsigned long bytes_per_type[MAX_FSTYPE];
	unsigned long airtime_per_type[MAX_FSTYPE];
	unsigned long symbols_per_type[MAX_FSTYPE];

	unsigned long filtered_packets;

	struct timeval stats_time;
};

extern struct statistics stat;

struct config {
	char* ifname;
	int rport;
	int quiet;
	int node_timeout;
	int display_interval;
	int sleep_time;
	char* dumpfile;
	int recv_buffer_size;

	unsigned char filtermac[MAX_FILTERMAC][MAC_LEN];
	unsigned char filterbssid[MAC_LEN];
	int filter_pkt;
	int do_filter;

	/* this isn't exactly config, but wtf... */
	int arphrd; // the device ARP type
	int paused;
	int do_macfilter;
};

extern struct config conf;

void
finish_all(int sig);

#endif
