/* horst - olsr scanning tool
 *
 * Copyright (C) 2005-2010 Bruno Randolf (br1@einfach.org)
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

#include "list.h"

#define VERSION "2.0-rc2"

#ifndef DO_DEBUG
#define DO_DEBUG 0
#endif

#define MAC_LEN			6

#define MAX_NODES		255
#define MAX_ESSIDS		255
#define MAX_BSSIDS		255
#define MAX_HISTORY		255
#define MAX_ESSID_LEN		32
#define MAX_RATES		109	/* in 500kbps steps: 54 * 2 + 1 for array index */
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

/* default config values */
#define INTERFACE_NAME		"wlan0"
#define NODE_TIMEOUT		60	/* seconds */
#define CHANNEL_TIME		250000	/* 250 usec */
/* update display every 100ms - "10 frames per sec should be enough for everyone" ;) */
#define DISPLAY_UPDATE_INTERVAL 100000	/* usec */
#define SLEEP_TIME		1000	/* usec */
#define RECV_BUFFER_SIZE	6750000	/* 54Mbps in byte */
#define DEFAULT_PORT		"4444"	/* string because of getaddrinfo() */

#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803    /* IEEE 802.11 + radiotap header */
#endif

#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802      /* IEEE 802.11 + Prism2 header  */
#endif


struct packet_info {
	/* general */
	int			pkt_types;	/* bitmask of packet types in this pkt */
	int			len;		/* packet length */

	/* wlan phy (from radiotap) */
	int			signal;		/* signal strength (usually dBm) */
	int			noise;		/* noise level (usually dBm) */
	int			snr;		/* signal to noise ratio */
	int			rate;		/* physical rate */
	int			phy_freq;	/* frequency (unused) */
	int			phy_flags;	/* A, B, G, shortpre */

	/* wlan mac */
	int			wlan_type;	/* frame control field */
	unsigned char		wlan_src[MAC_LEN];
	unsigned char		wlan_dst[MAC_LEN];
	unsigned char		wlan_bssid[MAC_LEN];
	char			wlan_essid[MAX_ESSID_LEN];
	u_int64_t		wlan_tsf;	/* timestamp from beacon */
	int			wlan_mode;	/* AP, STA or IBSS */
	unsigned char		wlan_channel;	/* channel from beacon, probe */

	unsigned int		wlan_wep:1,	/* WEP on/off */
				wlan_retry:1;

	/* IP */
	unsigned int		ip_src;
	unsigned int		ip_dst;
	int			olsr_type;
	int			olsr_neigh;
	int			olsr_tc;
};

extern struct packet_info current_packet;

struct essid_info;

struct node_info {
	/* housekeeping */
	struct list_head	list;
	struct list_head	essid_nodes;
	time_t			last_seen;	/* timestamp */

	/* general packet info */
	int			pkt_types;	/* bitmask of packet types we've seen */
	int			pkt_count;	/* nr of packets seen */

	/* wlan phy (from radiotap) */
	int			snr;
	int			snr_min;
	int			snr_max;
	int			sig_max;

	/* wlan mac */
	unsigned char		wlan_bssid[MAC_LEN];
	int			channel;	/* channel from beacon, probe frames */
	int			wlan_mode;	/* AP, STA or IBSS */
	u_int64_t		tsf;
	int			wep;		/* WEP active? */
	struct essid_info*	essid;

	/* IP */
	unsigned int		ip_src;		/* IP address (if known) */
	int			olsr_count;	/* number of OLSR packets */
	int			olsr_neigh;	/* number if OLSR neighbours */
	int			olsr_tc;	/* unused */

	struct packet_info	last_pkt;
};

extern struct list_head nodes;

struct essid_info {
	struct list_head	list;
	char			essid[MAX_ESSID_LEN];
	struct list_head	nodes;
	int			num_nodes;
	int			split;
};

struct essid_meta_info {
	struct list_head	list;
	struct essid_info*	split_essid;
	int			split_active;
};

extern struct essid_meta_info essids;

struct history {
	int			signal[MAX_HISTORY];
	int			noise[MAX_HISTORY];
	int			rate[MAX_HISTORY];
	int			type[MAX_HISTORY];
	int			retry[MAX_HISTORY];
	int			index;
};

extern struct history hist;

struct statistics {
	unsigned long		packets;
	unsigned long		bytes;
	unsigned long		duration;

	unsigned long		packets_per_rate[MAX_RATES];
	unsigned long		bytes_per_rate[MAX_RATES];
	unsigned long		duration_per_rate[MAX_RATES];

	unsigned long		packets_per_type[MAX_FSTYPE];
	unsigned long		bytes_per_type[MAX_FSTYPE];
	unsigned long		duration_per_type[MAX_FSTYPE];

	unsigned long		filtered_packets;

	struct timeval		stats_time;
};

extern struct statistics stats;

struct config {
	char*			ifname;
	char*			port;
	int			quiet;
	int			node_timeout;
	int			channel_time;
	int			current_channel;
	int			display_interval;
	int			sleep_time;
	char*			dumpfile;
	int			recv_buffer_size;
	char*			serveraddr;

	unsigned char		filtermac[MAX_FILTERMAC][MAC_LEN];
	char			filtermac_enabled[MAX_FILTERMAC];
	unsigned char		filterbssid[MAC_LEN];
	int			filter_pkt;
	unsigned int		filter_off:1,
				do_macfilter:1,
				do_change_channel:1;

	/* this isn't exactly config, but wtf... */
	int			arphrd; // the device ARP type
	int			paused;
};

extern struct config conf;

extern struct timeval the_time;


void
finish_all(int sig);

void
free_lists(void);

void
change_channel(int c);

#endif
