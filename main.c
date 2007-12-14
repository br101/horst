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
static int node_update(struct packet_info* pkt);
static void check_ibss_split(struct packet_info* pkt, int pkt_node);
static int filter_packet(struct packet_info* pkt);
static void update_history(struct packet_info* pkt);
static void update_statistics(struct packet_info* pkt);
static void write_to_file(struct packet_info* pkt);

struct packet_info current_packet;

/* no, i dont want to implement a linked list now */
struct node_info nodes[MAX_NODES];
struct essid_info essids[MAX_ESSIDS];
struct split_info splits;
struct history hist;
struct statistics stats;

struct config conf = {
	.node_timeout = NODE_TIMEOUT,
	.ifname = INTERFACE_NAME,
	.display_interval = DISPLAY_UPDATE_INTERVAL,
	.sleep_time = SLEEP_TIME,
	.filter_pkt = 0xffffff,
	.recv_buffer_size = RECV_BUFFER_SIZE,
};

static int mon; /* monitoring socket */
static FILE* DF = NULL;


int
main(int argc, char** argv)
{
	unsigned char buffer[8192];
	int len;
	int n;

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
		memset(&current_packet,0,sizeof(current_packet));
		if (!parse_packet(buffer, len)) {
			DEBUG("parsing failed\n");
			continue;
		}

		if (filter_packet(&current_packet))
			continue;

		if (conf.dumpfile != NULL)
			write_to_file(&current_packet);

		n = node_update(&current_packet);

		update_history(&current_packet);
		update_statistics(&current_packet);
		check_ibss_split(&current_packet, n);

		if (conf.rport) {
			net_send_packet();
			continue;
		}
#if !DO_DEBUG
		update_display(&current_packet, n);
#endif
	}
	/* will never */
	return 0;
}


void
get_options(int argc, char** argv)
{
	int c;

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
				conf.do_macfilter = 1;
				convert_string_to_mac(optarg, conf.filtermac[0]);
				printf("%s\n", ether_sprintf(conf.filtermac[0]));
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
finish_all(int sig)
{
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
	// update timestamp + status
	n->last_seen = time(NULL);
	n->status=1;
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
		memcpy(n->wlan_bssid, p->wlan_bssid, 6);
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


static int
node_update(struct packet_info* pkt)
{
	int i;

	if (pkt->wlan_src[0] == 0 && pkt->wlan_src[1] == 0 && pkt->wlan_src[2] == 0 &&
	    pkt->wlan_src[3] == 0 && pkt->wlan_src[4] == 0 && pkt->wlan_src[5] == 0) {
		return -1;
	}

	for (i = 0; i < MAX_NODES; i++) {
		if (nodes[i].status == 1) {
			/* check existing node */
			if (memcmp(pkt->wlan_src, nodes[i].last_pkt.wlan_src, 6) == 0) {
				copy_nodeinfo(&nodes[i], pkt);
				return i;
			}
		} else {
			/* past all used nodes: create new node */
			copy_nodeinfo(&nodes[i], pkt);
			nodes[i].essid = -1;
			return i;
		}
	}
	return -1;
}


static void
check_ibss_split(struct packet_info* pkt, int pkt_node)
{
	int i, n;
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

	/* find essid */
	for (i = 0; i < MAX_ESSIDS; i++) {
		if (essids[i].num_nodes == 0) {
			/* unused entry */
			break;
		}
		if (strncmp(essids[i].essid, pkt->wlan_essid, MAX_ESSID_LEN) == 0) {
			/* essid matches */
			DEBUG("SPLIT   essid found\n");
			break;
		}
	}

	/* find node if already recorded */
	for (n = 0; n < essids[i].num_nodes && n < MAX_NODES; n++) {
		if (essids[i].nodes[n] == pkt_node) {
			DEBUG("SPLIT   node found %d\n", n);
			break;
		}
	}

	DEBUG("SPLIT   at essid %d count %d node %d\n", i, essids[i].num_nodes, n);

	/* new essid */
	if (essids[i].num_nodes == 0) {
		DEBUG("SPLIT   new essid '%s'\n", pkt->wlan_essid);
		strncpy(essids[i].essid, pkt->wlan_essid, MAX_ESSID_LEN);
	}

	/* new node */
	if (essids[i].num_nodes == 0 || essids[i].nodes[n] != pkt_node) {
		DEBUG("SPLIT   recorded new node nr %d %d %s\n", n, pkt_node,
			ether_sprintf(pkt->wlan_src) );
		essids[i].nodes[n] = pkt_node;
		essids[i].num_nodes = n + 1;
		nodes[pkt_node].essid = i;
	}

	/* check for split */
	essids[i].split = 0;
	for (n = 0; n < essids[i].num_nodes && n < MAX_NODES; n++) {
		node = &nodes[essids[i].nodes[n]];
		DEBUG("SPLIT      %d. node %d src %s", n,
			essids[i].nodes[n], ether_sprintf(node->last_pkt.wlan_src));
		DEBUG(" bssid %s\n", ether_sprintf(node->wlan_bssid));

		if (node->wlan_mode == WLAN_MODE_AP)
			continue;

		if (last_bssid && memcmp(last_bssid, node->wlan_bssid, 6) != 0) {
			essids[i].split = 1;
			DEBUG("SPLIT *** DETECTED!!!\n");
		}
		last_bssid = node->wlan_bssid;
	}

	/* if a split occurred on this essid, record it */
	if (essids[i].split > 0) {
		DEBUG("SPLIT *** new record %d\n", i);
		splits.count = 1;
		splits.essid[0] = i;
	}
	else {
		splits.count = 0;
	}
}


static int
filter_packet(struct packet_info* pkt)
{
	int i;

	if (!conf.do_filter)
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
