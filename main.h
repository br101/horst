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
#include "util/average.h"
#include "core/channel.h"
#include "core/wlan80211.h"
#include "core/wlan_parser.h"
#include "core/conf.h"
#include "core/phy_info.h"
#include "linux/platform.h"

#define VERSION "6.0-pre"
#define CONFIG_FILE "/etc/horst.conf"

#ifndef DO_DEBUG
#define DO_DEBUG 0
#endif

#if DO_DEBUG
#define DEBUG(...) do { if (conf.debug) printf(__VA_ARGS__); } while (0)
#else
#define DEBUG(...)
#endif

#define MAX_HISTORY		255
#define MAX_RATES		44	/* 12 legacy rates and 32 MCS */
#define MAX_FSTYPE		0xff

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

#define DEFAULT_MAC_NAME_FILE	"/tmp/dhcp.leases"

struct essid_info;

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

extern struct timespec the_time;

void free_lists(void);
void init_spectrum(void);
void update_spectrum_durations(void);
void handle_packet(struct packet_info* p);
void main_pause(int pause);
void main_reset(void);
void dumpfile_open(const char* name);
const char* mac_name_lookup(const unsigned char* mac, int shorten_mac);

#endif
