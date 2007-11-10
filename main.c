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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#include "protocol_parser.h"
#include "display.h"
#include "network.h"
#include "main.h"
#include "util.h"
#include "ieee80211.h"

static int device_index(int fd, const char *if_name);
static void device_promisc(int fd, const char *if_name, int on);
static int device_get_arptype(int fd, const char *device);
static int init_packet_socket(char* devname);
static void get_options(int argv, char** argc);
static int node_update(struct packet_info* pkt);
static void check_ibss_split(struct packet_info* pkt, int pkt_node);
static int filter_packet(struct packet_info* pkt);
static void update_history(struct packet_info* pkt);
static void update_statistics(struct packet_info* pkt);

struct packet_info current_packet;

/* no, i dont want to implement a linked list now */
struct node_info nodes[MAX_NODES];
struct essid_info essids[MAX_ESSIDS];
struct split_info splits;
struct history hist;
struct statistics stats;

struct config conf = {
	.node_timeout = NODE_TIMEOUT,
	.ifname = "wlan0",
	.display_interval = DISPLAY_UPDATE_INTERVAL,
	.sleep_time = SLEEP_TIME,
	.filter_pkt = 0xffffff,
};

static int mon; /* monitoring socket */


int
main(int argc, char** argv)
{
	unsigned char buffer[8192];
	int len;
	int n;

	get_options(argc, argv);

	if (!conf.quiet)
		printf("using interface %s\n", conf.ifname);

	signal(SIGINT, finish_all);

	mon = init_packet_socket(conf.ifname);

	if (mon < 0)
		exit(0);

	if (conf.rport)
		net_init_socket(conf.rport);
#if !DO_DEBUG
	else {
		init_display();
	}
#endif

	while ((len = recv(mon, buffer, 8192, MSG_DONTWAIT)))
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
	return 0;
}


static int
init_packet_socket(char* devname)
{
	int ret;
	int fd;
	int ifindex;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0)
		err(1, "could not create socket");

	/* bind only to one interface */
	ifindex = device_index(fd, devname);

	struct sockaddr_ll sall;
	sall.sll_ifindex = ifindex;
	sall.sll_family = AF_PACKET;
	sall.sll_protocol = htons(ETH_P_ALL);

	ret = bind(fd, (struct sockaddr*)&sall, sizeof(sall));
	if (ret != 0)
		err(1, "bind failed");

	device_promisc(fd, devname, 1);
	conf.arphrd = device_get_arptype(fd, devname);

	return fd;
}


static int
device_index(int fd, const char *if_name)
{
	struct ifreq req;

	strncpy(req.ifr_name, if_name, IFNAMSIZ);
	req.ifr_addr.sa_family = AF_INET;

	if (ioctl(fd, SIOCGIFINDEX, &req) < 0)
		err(1, "interface %s not found", if_name);

	if (req.ifr_ifindex<0) {
		err(1, "interface %s not found", if_name);
	}
	DEBUG("index %d\n", req.ifr_ifindex);
	return req.ifr_ifindex;
}


static void
device_promisc(int fd, const char *if_name, int on)
{
	struct ifreq req;

	strncpy(req.ifr_name, if_name, IFNAMSIZ);
	req.ifr_addr.sa_family = AF_INET;

	if (ioctl(fd, SIOCGIFFLAGS, &req) < 0) {
		err(1, "could not get device flags for %s", if_name);
	}

	/* put interface up in any case */
	req.ifr_flags |= IFF_UP;

	if (on)
		req.ifr_flags |= IFF_PROMISC;
	else
		req.ifr_flags &= ~IFF_PROMISC;

	if (ioctl(fd, SIOCSIFFLAGS, &req) < 0) {
		err(1, "could not set promisc mode for %s", if_name);
	}
}


/*
 *  Get the hardware type of the given interface as ARPHRD_xxx constant.
 */
static int
device_get_arptype(int fd, const char *device)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		err(1, "could not get arptype");
	}
	DEBUG("ARPTYPE %d\n", ifr.ifr_hwaddr.sa_family);
	return ifr.ifr_hwaddr.sa_family;
}


#if 0
static void
device_wireless_channel(int fd, const char* if_name, int chan)
{
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, if_name, IFNAMSIZ);
	iwr.u.freq.m = chan * 100000;
	iwr.u.freq.e = 1;

	if (ioctl(fd, SIOCSIWFREQ, &iwr) < 0) {
		perror("ioctl[SIOCSIWFREQ]");
		ret = -1;
	}
}
#endif


void
get_options(int argc, char** argv)
{
	int c;

	while((c = getopt(argc, argv, "hqi:t:p:e:d:w:")) > 0) {
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
			case 't':
				conf.node_timeout = atoi(optarg);
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
				printf("usage: %s [-h] [-q] [-i interface] [-t sec] [-p port] [-e mac] [-d usec] [-w usec]\n\n"
					"Options (default value)\n"
					"  -h\t\tthis help\n"
					"  -q\t\tquiet [basically useless]\n"
					"  -i <intf>\tinterface (wlan0)\n"
					"  -t <sec>\tnode timeout (60)\n"
					"  -p <port>\tuse port\n"
					"  -e <mac>\tfilter all macs ecxept this\n"
					"  -d <usec>\tdisplay update interval (100000 = 100ms = 10fps)\n"
					"  -w <usec>\twait loop (1000 = 1ms)\n\n",
					argv[0]);
				exit(0);
				break;
		}
	}
}


void
finish_all(int sig)
{
	device_promisc(mon, conf.ifname, 0);
	close(mon);
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
		n->tsfl = *(unsigned long*)(&p->wlan_tsf[0]);
		n->tsfh = *(unsigned long*)(&p->wlan_tsf[4]);
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

	if (!(pkt->pkt_types & conf.filter_pkt))
		return 1;

	if (conf.do_macfilter) {
		for (i = 0; i < MAX_FILTERMAC; i++) {
			if (memcmp(current_packet.wlan_src, conf.filtermac[i], MAC_LEN) == 0)
				return 0;
		}
		return 1;
	}
	return 0;
}


static void
update_history(struct packet_info* p) {
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
update_statistics(struct packet_info* p) {
	stats.packets++;
	stats.bytes += p->len;
	if (p->rate > 0 && p->rate < MAX_RATES) {
		/* this basically normalizes everything to 1Mbit per sec */
		stats.airtimes += (p->len * 8) / p->rate;
		stats.packets_per_rate[p->rate]++;
		stats.bytes_per_rate[p->rate] += p->len;
	}
	if (p->wlan_type >= 0 && p->wlan_type < MAX_FSTYPE) {
		stats.packets_per_type[p->wlan_type]++;
		stats.bytes_per_type[p->wlan_type] += p->len;
		if (p->rate > 0 && p->rate < MAX_RATES)
			stats.airtime_per_type[p->wlan_type] += p->len / p->rate;
	}
}
