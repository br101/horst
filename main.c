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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <err.h>

#include "protocol_parser.h"
#include "display.h"
#include "network.h"
#include "main.h"
#include "capture.h"
#include "util.h"
#include "ieee80211.h"
#include "ieee80211_util.h"

static void get_options(int argv, char** argc);
static struct node_info* node_update(struct packet_info* pkt);
static void check_ibss_split(struct packet_info* pkt, struct node_info* pkt_node);
static int filter_packet(struct packet_info* pkt);
static void update_history(struct packet_info* pkt);
static void update_statistics(struct packet_info* pkt);
static void write_to_file(struct packet_info* pkt);

struct packet_info current_packet;

struct list_head nodes;
struct essid_meta_info essids;
struct history hist;
struct statistics stats;

struct config conf = {
	.node_timeout		= NODE_TIMEOUT,
	.ifname			= INTERFACE_NAME,
	.display_interval	= DISPLAY_UPDATE_INTERVAL,
	.sleep_time		= SLEEP_TIME,
	.filter_pkt		= 0xffffff,
	.recv_buffer_size	= RECV_BUFFER_SIZE,
};

struct timeval the_time;

static int mon; /* monitoring socket */
static FILE* DF = NULL;


int
main(int argc, char** argv)
{
	unsigned char buffer[8192];
	int len;
	struct node_info* node;

	get_options(argc, argv);

	if (!conf.quiet)
		printf("horst: using monitoring interface %s\n", conf.ifname);

	signal(SIGINT, finish_all);

	gettimeofday(&stats.stats_time, NULL);

	mon = open_packet_socket(conf.ifname, sizeof(buffer), conf.recv_buffer_size);
	if (mon < 0)
		err(1, "couldn't open packet socket");

	conf.arphrd = device_get_arptype();
	if (conf.arphrd != ARPHRD_IEEE80211_PRISM &&
	    conf.arphrd != ARPHRD_IEEE80211_RADIOTAP) {
		printf("wrong monitor type. please use radiotap or prism2 headers\n");
		exit(1);
	}

	if (conf.dumpfile != NULL) {
		DF = fopen(conf.dumpfile, "w");
		if (DF == NULL)
			err(1, "couldn't open dump file");
	}

	INIT_LIST_HEAD(&essids.list);
	INIT_LIST_HEAD(&nodes);

	if (conf.rport)
		net_init_socket(conf.rport);
#if !DO_DEBUG
	else {
		init_display();
	}
#endif

	while ((len = recv_packet(buffer, sizeof(buffer))))
	{
		handle_user_input();

		if (conf.paused || len == -1) {
			/*
			 * no packet received or paused: just wait a few ms
			 * if we wait too long here we will loose packets
			 * if we don't sleep there will be 100% system load
			 */
			usleep(conf.sleep_time);
			continue;
		}
#if DO_DEBUG
		dump_packet(buffer, len);
#endif
		memset(&current_packet, 0, sizeof(current_packet));
		if (!parse_packet(buffer, len)) {
			DEBUG("parsing failed\n");
			continue;
		}

		if (filter_packet(&current_packet))
			continue;

		gettimeofday(&the_time, NULL);

		if (conf.dumpfile != NULL)
			write_to_file(&current_packet);

		node = node_update(&current_packet);

		update_history(&current_packet);
		update_statistics(&current_packet);
		check_ibss_split(&current_packet, node);

		if (conf.rport) {
			net_send_packet();
			continue;
		}
#if !DO_DEBUG
		update_display(&current_packet, node);
#endif
	}
	/* will never */
	return 0;
}


void
get_options(int argc, char** argv)
{
	int c;
	static int n;

	while((c = getopt(argc, argv, "hqi:t:p:e:d:w:o:b:")) > 0) {
		switch (c) {
			case 'p':
				conf.rport = atoi(optarg);
				break;
			case 'q':
				conf.quiet = 1;
				break;
			case 'i':
				conf.ifname = optarg;
				break;
			case 'o':
				conf.dumpfile = optarg;
				break;
			case 't':
				conf.node_timeout = atoi(optarg);
				break;
			case 'b':
				conf.recv_buffer_size = atoi(optarg);
				break;
			case 's':
				/* reserved for spectro meter */
				break;
			case 'd':
				conf.display_interval = atoi(optarg);
				break;
			case 'w':
				conf.sleep_time = atoi(optarg);
				break;
			case 'e':
				if (n >= MAX_FILTERMAC)
					break;
				conf.do_macfilter = 1;
				convert_string_to_mac(optarg, conf.filtermac[n]);
				printf("%s\n", ether_sprintf(conf.filtermac[n]));
				n++;
				break;
			case 'h':
			default:
				printf("usage: %s [-h] [-q] [-i interface] [-t sec] [-p port] [-e mac] [-d usec] [-w usec] [-o file]\n\n"
					"Options (default value)\n"
					"  -h\t\tthis help\n"
					"  -q\t\tquiet [basically useless]\n"
					"  -i <intf>\tinterface (wlan0)\n"
					"  -t <sec>\tnode timeout (60)\n"
					"  -p <port>\tuse port\n"
					"  -e <mac>\tfilter all macs ecxept this\n"
					"  -d <usec>\tdisplay update interval (100000 = 100ms = 10fps)\n"
					"  -w <usec>\twait loop (1000 = 1ms)\n"
					"  -o <filename>\twrite packet info into file\n"
					"  -b <bytes>\treceive buffer size (6750000)\n"
					"\n",
					argv[0]);
				exit(0);
				break;
		}
	}
}


void
free_lists(void)
{
	struct essid_info *e, *f;
	struct node_ptr_list *n, *m;
	struct node_info *ni, *mi;

	/* free node list */
	list_for_each_entry_safe(ni, mi, &nodes, list) {
		DEBUG("free node %s\n", ether_sprintf(ni->last_pkt.wlan_src));
		list_del(&ni->list);
		free(ni);
	}

	/* free essids and their node references */
	list_for_each_entry_safe(e, f, &essids.list, list) {
		DEBUG("free essid '%s'\n", e->essid);
		list_for_each_entry_safe(n, m, &e->nodes, list) {
			DEBUG("  free node ptr %s\n", ether_sprintf(n->node->last_pkt.wlan_src));
			list_del(&n->list);
			free(n);
		}
		list_del(&e->list);
		free(e);
	}
}


void
finish_all(int sig)
{
	free_lists();

	close_packet_socket();
	
	if (DF != NULL)
		fclose(DF);

#if !DO_DEBUG
	if (conf.rport)
		net_finish();
	else
		finish_display(sig);
#endif
	exit(0);
}


static void
copy_nodeinfo(struct node_info* n, struct packet_info* p)
{
	memcpy(&(n->last_pkt), p, sizeof(struct packet_info));
	// update timestamp
	n->last_seen = time(NULL);
	n->pkt_count++;
	n->pkt_types |= p->pkt_types;
	if (p->ip_src)
		n->ip_src = p->ip_src;
	if (p->wlan_mode)
		n->wlan_mode = p->wlan_mode;
	if (p->olsr_tc)
		n->olsr_tc = p->olsr_tc;
	if (p->olsr_neigh)
		n->olsr_neigh = p->olsr_neigh;
	if (p->pkt_types & PKT_TYPE_OLSR)
		n->olsr_count++;
	if (p->wlan_bssid[0] != 0xff &&
		! (p->wlan_bssid[0] == 0 && p->wlan_bssid[1] == 0 && p->wlan_bssid[2] == 0 &&
		   p->wlan_bssid[3] == 0 && p->wlan_bssid[4] == 0 && p->wlan_bssid[5] == 0)) {
		memcpy(n->wlan_bssid, p->wlan_bssid, MAC_LEN);
	}
	if ((p->wlan_type & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT &&
	    (p->wlan_type & IEEE80211_FCTL_STYPE) == IEEE80211_STYPE_BEACON) {
		n->tsf = p->wlan_tsf;
	}
	n->snr = p->snr;
	if (p->snr > n->snr_max)
		n->snr_max = p->snr;
	if (p->signal > n->sig_max || n->sig_max == 0)
		n->sig_max = p->signal;
	if ((n->snr_min == 0 && p->snr > 0) || p->snr < n->snr_min)
		n->snr_min = p->snr;
	if (p->wlan_channel !=0)
		n->channel = p->wlan_channel;
	n->wep = p->wlan_wep;
}


static struct node_info*
node_update(struct packet_info* pkt)
{
	struct node_info* n;

	if (pkt->wlan_src[0] == 0 && pkt->wlan_src[1] == 0 && pkt->wlan_src[2] == 0 &&
	    pkt->wlan_src[3] == 0 && pkt->wlan_src[4] == 0 && pkt->wlan_src[5] == 0) {
		return NULL;
	}

	/* find node by wlan source address */
	list_for_each_entry(n, &nodes, list) {
		if (memcmp(pkt->wlan_src, n->last_pkt.wlan_src, MAC_LEN) == 0) {
			DEBUG("node found %p\n", n);
			break;
		}
	}

	/* not found */
	if (&n->list == &nodes) {
		n = malloc(sizeof(struct node_info));
		memset(n, 0, sizeof(struct node_info));
		n->essid = NULL;
		list_add_tail(&n->list, &nodes);
	}

	copy_nodeinfo(n, pkt);

	return n;
}

static struct node_ptr_list*
remove_node_from_old_essid(struct node_info* pkt_node)
{
	struct node_ptr_list *n, *m;

	list_for_each_entry_safe(n, m, &pkt_node->essid->nodes, list) {
		if (n->node == pkt_node) {
			DEBUG("SPLIT   remove node from old essid\n");
			list_del(&n->list);
			pkt_node->essid->num_nodes--;
			break;
		}
	}

	/* also delete essid if it has no more nodes */
	if (pkt_node->essid->num_nodes == 0) {
		DEBUG("SPLIT   essid empty, delete\n");
		list_del(&pkt_node->essid->list);
		free(pkt_node->essid);
		pkt_node->essid = NULL;
	}

	/* in case we didn't finde the node */
	if (&n->list == &pkt_node->essid->nodes)
		n = NULL;

	return n;
}


static void
check_ibss_split(struct packet_info* pkt, struct node_info* pkt_node)
{
	struct essid_info* e;
	struct node_ptr_list* n;
	struct node_info* node;
	unsigned char* last_bssid = NULL;

	/* only check beacons (XXX: what about PROBE?) */
	if (!((pkt->wlan_type & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT &&
	     (pkt->wlan_type & IEEE80211_FCTL_STYPE) == IEEE80211_STYPE_BEACON)) {
		return;
	}

	DEBUG("SPLIT check ibss '%s' node %s ", pkt->wlan_essid,
		ether_sprintf(pkt->wlan_src));
	DEBUG("bssid %s\n", ether_sprintf(pkt->wlan_bssid));

	/* find essid if already recorded */
	list_for_each_entry(e, &essids.list, list) {
		if (strncmp(e->essid, pkt->wlan_essid, MAX_ESSID_LEN) == 0) {
			DEBUG("SPLIT   essid found\n");
			break;
		}
	}

	/* if not add new essid */
	if (&e->list == &essids.list) {
		DEBUG("SPLIT   essid not found, adding new\n");
		e = malloc(sizeof(struct essid_info));
		strncpy(e->essid, pkt->wlan_essid, MAX_ESSID_LEN);
		e->num_nodes = 0;
		e->split = 0;
		INIT_LIST_HEAD(&e->nodes);
		list_add_tail(&e->list, &essids.list);
	}

	/* find node if already recorded */
	list_for_each_entry(n, &e->nodes, list) {
		if (n->node == pkt_node) {
			DEBUG("SPLIT   node found %p\n", n);
			break;
		}
	}

	/* new node */
	if (&n->list == &e->nodes) {
		DEBUG("SPLIT   node not found, adding new %s\n",
			ether_sprintf(pkt->wlan_src));
		n = NULL;
		/* if node had another essid before, move it here */
		if (pkt_node->essid != NULL)
			n = remove_node_from_old_essid(pkt_node);
		if (n == NULL) {
			n = malloc(sizeof(struct node_ptr_list));
			n->node = pkt_node;
		}
		list_add_tail(&n->list, &e->nodes);
		e->num_nodes++;
		pkt_node->essid = e;
	}

	/* check for split */
	e->split = 0;
	list_for_each_entry(n, &e->nodes, list) {
		node = n->node;
		DEBUG("SPLIT      node %p src %s",
			node, ether_sprintf(node->last_pkt.wlan_src));
		DEBUG(" bssid %s\n", ether_sprintf(node->wlan_bssid));

		if (node->wlan_mode == WLAN_MODE_AP)
			continue;

		if (last_bssid && memcmp(last_bssid, node->wlan_bssid, MAC_LEN) != 0) {
			e->split = 1;
			DEBUG("SPLIT *** DETECTED!!!\n");
		}
		last_bssid = node->wlan_bssid;
	}

	/* if a split occurred on this essid, record it */
	if (e->split > 0) {
		DEBUG("SPLIT *** active\n");
		essids.split_active = 1;
		essids.split_essid = e;
	}
	else if (e == essids.split_essid) {
		DEBUG("SPLIT *** ok now\n");
		essids.split_active = 0;
		essids.split_essid = NULL;
	}
}


static int
filter_packet(struct packet_info* pkt)
{
	int i;

	if (conf.filter_off)
		return 0;

	if (!(pkt->pkt_types & conf.filter_pkt)) {
		stats.filtered_packets++;
		return 1;
	}

	if (MAC_NOT_EMPTY(conf.filterbssid) &&
	    memcmp(current_packet.wlan_bssid, conf.filterbssid, MAC_LEN) != 0) {
		stats.filtered_packets++;
		return 1;
	}

	if (conf.do_macfilter) {
		for (i = 0; i < MAX_FILTERMAC; i++) {
			if (memcmp(current_packet.wlan_src, conf.filtermac[i], MAC_LEN) == 0)
				return 0;
		}
		stats.filtered_packets++;
		return 1;
	}
	return 0;
}


static void
update_history(struct packet_info* p)
{
	if (p->signal == 0)
		return;

	hist.signal[hist.index] = p->signal;
	hist.noise[hist.index] = p->noise;
	hist.rate[hist.index] = p->rate;
	hist.type[hist.index] = p->wlan_type;
	hist.index++;
	if (hist.index == MAX_HISTORY)
		hist.index = 0;
}


static void
update_statistics(struct packet_info* p)
{
	int duration;
	duration = ieee80211_frame_duration(p->phy_flags & PHY_FLAG_MODE_MASK,
			p->len, p->rate * 5, p->phy_flags & PHY_FLAG_SHORTPRE);

	stats.packets++;
	stats.bytes += p->len;

	if (p->rate > 0 && p->rate < MAX_RATES) {
		stats.duration += duration;
		stats.packets_per_rate[p->rate]++;
		stats.bytes_per_rate[p->rate] += p->len;
		stats.duration_per_rate[p->rate] += duration;
	}
	if (p->wlan_type >= 0 && p->wlan_type < MAX_FSTYPE) {
		stats.packets_per_type[p->wlan_type]++;
		stats.bytes_per_type[p->wlan_type] += p->len;
		if (p->rate > 0 && p->rate < MAX_RATES)
			stats.duration_per_type[p->wlan_type] += duration;
	}
}


static void 
write_to_file(struct packet_info* pkt)
{
	fprintf(DF, "%s, %s, ",
		get_packet_type_name(pkt->wlan_type), ether_sprintf(pkt->wlan_src));
	fprintf(DF, "%s, ", ether_sprintf(pkt->wlan_dst));
	fprintf(DF, "%s, ", ether_sprintf(pkt->wlan_bssid));
	fprintf(DF, "%x, %d, %d, %d, %d, %d, ",
		pkt->pkt_types, pkt->signal, pkt->noise, pkt->snr, pkt->len, pkt->rate);
	fprintf(DF, "%016llx, ", pkt->wlan_tsf);
	fprintf(DF, "%s, %d, %d, %d, ",
		pkt->wlan_essid, pkt->wlan_mode, pkt->wlan_channel, pkt->wlan_wep);
	fprintf(DF, "%s, ", ip_sprintf(pkt->ip_src));
	fprintf(DF, "%s, ", ip_sprintf(pkt->ip_dst));
	fprintf(DF, "%d, %d, %d\n", pkt->olsr_type, pkt->olsr_neigh, pkt->olsr_tc);
}
