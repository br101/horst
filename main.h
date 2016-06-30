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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdlib.h>
#include <time.h>

#undef LIST_HEAD 
#include "ccan/list/list.h"
#include "average.h"
#include "channel.h"
#include "wlan80211.h"

#define VERSION "5.0-pre"
#define CONFIG_FILE "/etc/horst.conf"

#ifndef DO_DEBUG
#define DO_DEBUG 0
#endif

#if DO_DEBUG
#define DEBUG(...) do { if (conf.debug) printf(__VA_ARGS__); } while (0)
#else
#define DEBUG(...)
#endif

/* #include <net/if.h> conflicts with <linux/if.h> in ifctrl-wext and there does
 * not seem to be a better solution that to just define IF_NAMESIZE ourselves */
#define IF_NAMESIZE	16

#define MAC_LEN			6
#define MAX_CONF_VALUE_STRLEN	200
#define MAX_CONF_NAME_STRLEN	32

#define MAX_HISTORY		255
#define MAX_RATES		44	/* 12 legacy rates and 32 MCS */
#define MAX_FSTYPE		0xff
#define MAX_FILTERMAC		9

#define MAX_NODE_NAME_STRLEN	18
#define MAX_NODE_NAMES		64

/* higher level packet types */
#define PKT_TYPE_ARP		BIT(0)
#define PKT_TYPE_IP		BIT(1)
#define PKT_TYPE_ICMP		BIT(2)
#define PKT_TYPE_UDP		BIT(3)
#define PKT_TYPE_TCP		BIT(4)
#define PKT_TYPE_OLSR		BIT(5)
#define PKT_TYPE_BATMAN		BIT(6)
#define PKT_TYPE_MESHZ		BIT(7)

#define PKT_TYPE_ALL		(PKT_TYPE_ARP | PKT_TYPE_IP | PKT_TYPE_ICMP | \
				 PKT_TYPE_UDP | PKT_TYPE_TCP | \
				 PKT_TYPE_OLSR | PKT_TYPE_BATMAN | PKT_TYPE_MESHZ)

#define WLAN_MODE_AP		BIT(0)
#define WLAN_MODE_IBSS		BIT(1)
#define WLAN_MODE_STA		BIT(2)
#define WLAN_MODE_PROBE		BIT(3)
#define WLAN_MODE_4ADDR		BIT(4)
#define WLAN_MODE_UNKNOWN	BIT(5)

#define WLAN_MODE_ALL		(WLAN_MODE_AP | WLAN_MODE_IBSS | WLAN_MODE_STA | WLAN_MODE_PROBE | WLAN_MODE_4ADDR | WLAN_MODE_UNKNOWN)

#define PHY_FLAG_SHORTPRE	BIT(0)
#define PHY_FLAG_BADFCS		BIT(1)
#define PHY_FLAG_A		BIT(2)
#define PHY_FLAG_B		BIT(3)
#define PHY_FLAG_G		BIT(4)
#define PHY_FLAG_MODE_MASK	BIT(5)

#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803    /* IEEE 802.11 + radiotap header */
#endif

#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802      /* IEEE 802.11 + Prism2 header  */
#endif

#define DEFAULT_MAC_NAME_FILE	"/tmp/dhcp.leases"

struct packet_info {
	/* general */
	unsigned int		pkt_types;	/* bitmask of packet types */

	/* wlan phy (from radiotap) */
	int			phy_signal;	/* signal strength (usually dBm) */
	unsigned int		phy_rate;	/* physical rate * 10 (=in 100kbps) */
	unsigned char		phy_rate_idx;	/* MCS index */
	unsigned char		phy_rate_flags;	/* MCS flags */
	unsigned int		phy_freq;	/* frequency from driver */
	unsigned int		phy_flags;	/* A, B, G, shortpre */

	/* wlan mac */
	unsigned int		wlan_len;	/* packet length */
	uint16_t		wlan_type;	/* frame control field */
	unsigned char		wlan_src[MAC_LEN]; /* transmitter (TA) */
	unsigned char		wlan_dst[MAC_LEN]; /* receiver (RA) */
	unsigned char		wlan_bssid[MAC_LEN];
	char			wlan_essid[WLAN_MAX_SSID_LEN];
	uint64_t		wlan_tsf;	/* timestamp from beacon */
	unsigned int		wlan_bintval;	/* beacon interval */
	unsigned int		wlan_mode;	/* AP, STA or IBSS */
	unsigned char		wlan_channel;	/* channel from beacon, probe */
	enum chan_width		wlan_chan_width;
	unsigned char		wlan_tx_streams;
	unsigned char		wlan_rx_streams;
	unsigned char		wlan_qos_class;	/* for QDATA frames */
	unsigned int		wlan_nav;	/* frame NAV duration */
	unsigned int		wlan_seqno;	/* sequence number */

	/* flags */
	unsigned int		wlan_wep:1,	/* WEP on/off */
				wlan_retry:1,
				wlan_wpa:1,
				wlan_rsn:1,
				wlan_ht40plus:1;

	/* batman-adv */
	unsigned char		bat_version;
	unsigned char		bat_packet_type;
	unsigned char		bat_gw:1;

	/* IP */
	unsigned int		ip_src;
	unsigned int		ip_dst;
	unsigned int		tcpudp_port;
	unsigned int		olsr_type;
	unsigned int		olsr_neigh;
	unsigned int		olsr_tc;

	/* calculated from other values */
	unsigned int		pkt_duration;	/* packet "airtime" */
	int			pkt_chan_idx;	/* received while on channel */
	int			wlan_retries;	/* retry count for this frame */
};

struct essid_info;

struct node_info {
	/* housekeeping */
	struct list_node	list;
	struct list_node	essid_nodes;
	struct list_head	on_channels;	/* channels this node was seen on */
	unsigned int		num_on_channels;
	time_t			last_seen;	/* timestamp */

	/* general packet info */
	unsigned int		pkt_types;	/* bitmask of packet types we've seen */
	unsigned int		pkt_count;	/* nr of packets seen */

	/* wlan phy (from radiotap) */
	int			phy_sig_max;
	struct ewma		phy_sig_avg;
	unsigned long		phy_sig_sum;
	int			phy_sig_count;

	/* wlan mac */
	unsigned char		wlan_bssid[MAC_LEN];
	unsigned int		wlan_channel;	/* channel from beacon, probe frames */
	unsigned int		wlan_mode;	/* AP, STA or IBSS */
	uint64_t		wlan_tsf;
	unsigned int		wlan_bintval;
	unsigned int		wlan_retries_all;
	unsigned int		wlan_retries_last;
	unsigned int		wlan_seqno;
	struct essid_info*	essid;
	struct node_info*	wlan_ap_node;
	enum chan_width		wlan_chan_width;
	unsigned char		wlan_tx_streams;
	unsigned char		wlan_rx_streams;


	unsigned int		wlan_wep:1,	/* WEP active? */
				wlan_wpa:1,
				wlan_rsn:1,
				wlan_ht40plus:1;


	/* batman */
	unsigned char		bat_gw:1;

	/* IP */
	unsigned int		ip_src;		/* IP address (if known) */
	unsigned int		olsr_count;	/* number of OLSR packets */
	unsigned int		olsr_neigh;	/* number if OLSR neighbours */
	unsigned int		olsr_tc;	/* unused */

	struct packet_info	last_pkt;
};

extern struct list_head nodes;

struct essid_info {
	struct list_node	list;
	char			essid[WLAN_MAX_SSID_LEN];
	struct list_head	nodes;
	unsigned int		num_nodes;
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
	int			rate[MAX_HISTORY];
	unsigned int		type[MAX_HISTORY];
	unsigned int		retry[MAX_HISTORY];
	unsigned int		index;
};

extern struct history hist;

struct statistics {
	unsigned long		packets;
	unsigned long		retries;
	unsigned long		bytes;
	unsigned long		duration;

	unsigned long		packets_per_rate[MAX_RATES];
	unsigned long		bytes_per_rate[MAX_RATES];
	unsigned long		duration_per_rate[MAX_RATES];

	unsigned long		packets_per_type[MAX_FSTYPE];
	unsigned long		bytes_per_type[MAX_FSTYPE];
	unsigned long		duration_per_type[MAX_FSTYPE];

	unsigned long		filtered_packets;

	struct timespec		stats_time;
};

extern struct statistics stats;

struct channel_info {
	int			signal;
	struct ewma		signal_avg;
	unsigned long		packets;
	unsigned long		bytes;
	unsigned long		durations;
	unsigned long		durations_last;
	struct ewma		durations_avg;
	struct list_head	nodes;
	unsigned int		num_nodes;
};

extern struct channel_info spectrum[MAX_CHANNELS];

/* helper for keeping lists of nodes for each channel
 * (a node can be on more than one channel) */
struct chan_node {
	struct node_info*	node;
	struct channel_info*	chan;
	struct list_node	chan_list;	/* list for nodes per channel */
	struct list_node	node_list;	/* list for channels per node */
	int			sig;
	struct ewma		sig_avg;
	unsigned long		packets;
};

struct node_names_info {
	struct node_name {
		unsigned char	mac[MAC_LEN];
		char		name[MAX_NODE_NAME_STRLEN + 1];
	} entry[MAX_NODE_NAMES];
	int count;
};

extern struct node_names_info node_names;

struct config {
	char			ifname[IF_NAMESIZE + 1];
	int			port;
	int			quiet;
	int			node_timeout;
	int			channel_time;
	int			channel_max;
	int			channel_set_num;	/* value we want to set */
	enum chan_width		channel_set_width;	/* value we want to set */
	enum chan_width		channel_width;
	int			channel_idx;	/* index into channels array */
	int			channel_scan_rounds;
	int			display_interval;
	char			display_view;
	char			dumpfile[MAX_CONF_VALUE_STRLEN + 1];
	int			recv_buffer_size;
	char			serveraddr[MAX_CONF_VALUE_STRLEN + 1];
	char			control_pipe[MAX_CONF_VALUE_STRLEN + 1];
	char			mac_name_file[MAX_CONF_VALUE_STRLEN + 1];

	unsigned char		filtermac[MAX_FILTERMAC][MAC_LEN];
	char			filtermac_enabled[MAX_FILTERMAC];
	unsigned char		filterbssid[MAC_LEN];
	unsigned int		filter_pkt;
	uint16_t		filter_stype[WLAN_NUM_TYPES];  /* one for MGMT, CTRL, DATA */
	unsigned int		filter_mode;
	unsigned int		filter_off:1,
				filter_badfcs:1,
				do_change_channel:1,
				channel_ht40plus:1,	/* channel is HT40+ */
				channel_set_ht40plus:1,	/* value we want to set */
				allow_client:1,
				allow_control:1,
				debug:1,
				mac_name_lookup:1,
				add_monitor:1,
	/* this isn't exactly config, but wtf... */
				do_macfilter:1,
				display_initialized:1,
				channel_initialized:1,
				monitor_added:1;
	int			arphrd; // the device ARP type
	unsigned char		my_mac_addr[MAC_LEN];
	int			paused;
	int			if_type;
	int			if_phy;
	unsigned int		if_freq;
	unsigned int		max_phy_rate;
};

extern struct config conf;

extern struct timespec the_time;

void free_lists(void);
void init_spectrum(void);
void update_spectrum_durations(void);
void handle_packet(struct packet_info* p);
void __attribute__ ((format (printf, 1, 2)))
printlog(const char *fmt, ...);
void main_pause(int pause);
void main_reset(void);
void dumpfile_open(const char* name);
const char* mac_name_lookup(const unsigned char* mac, int shorten_mac);

#endif
