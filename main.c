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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h> /* needed for newer wireless tools */
#include <string.h>

#include <getopt.h>
#include <signal.h>
#include <time.h>

#include <linux/wireless.h> //XXX: with or without linux/???
/* it's not good to include kernel headers, i know... ;( */

#include "protocol_parser.h"
#include "display.h"
#include "network.h"
#include "main.h"

static int device_index(int fd, const char *if_name);
static void device_promisc(int fd, const char *if_name, int on);
static int device_get_arptype(int fd, const char *device);
static int init_packet_socket(char* devname);
static void get_options(int argv, char** argc);
static int node_update(struct packet_info* pkt);
static void check_ibss_split(struct packet_info* pkt, int pkt_node);

struct packet_info current_packet;

/* no, i dont want to implement a linked list now */

struct node_info nodes[MAX_NODES];

struct essid_info essids[MAX_ESSIDS];

struct split_info splits;

char* ifname = "wlan0";

int paused = 0;
int olsr_only = 0;
int no_ctrl = 0;
int do_filter = 0;

unsigned char filtermac[6];

static int mon; /* monitoring socket */

int rport = 0;

int quiet = 0;

int arphrd;


/* may be better to integrate all this into kismet */
int
main(int argc, char** argv)
{
	unsigned char buffer[8192];
	struct sockaddr from;
	socklen_t fromlen;
	int len;
	int n;

	get_options(argc, argv);

	if (!quiet)
		printf("using interface %s\n", ifname);

	signal(SIGINT, finish_all);

	mon = init_packet_socket(ifname);

	if (mon < 0)
		exit(0);
	
	if (rport)
		net_init_socket(rport);
#if !DO_DEBUG
	else {
		init_display();
		update_display(NULL, -1);
	}
#endif

	while ((len = recvfrom(mon, buffer, 8192, MSG_DONTWAIT, &from, &fromlen)))
	{
		handle_user_input();

		if (!paused && len != -1) {
#if DO_DEBUG
			dump_packet(buffer, len);
#endif
			memset(&current_packet,0,sizeof(current_packet));
			parse_packet(buffer, len);

			if (do_filter && 0 != memcmp(current_packet.wlan_src,
			    filtermac, sizeof(filtermac)))
				continue;

			n = node_update(&current_packet);

			check_ibss_split(&current_packet, n);

			if (rport) {
				net_send_packet();
				continue;
			}

#if !DO_DEBUG
			if (olsr_only) {  //XXX simplify logic???
				if (current_packet.pkt_types & PKT_TYPE_OLSR) {
					update_display(&current_packet, n);
				}
			}
			else if (!no_ctrl || (WLAN_FC_TYPE_CTRL != current_packet.wlan_type)) {
				update_display(&current_packet, n);
			}
#endif
		}
		usleep(100000);
	}
	return 0;
}

static int
init_packet_socket(char* devname)
{
	int ret;
	int fd;
	int ifindex;

	/* an alternative could be to use the pcap library */
	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0)
		perror("could not create socket");

	/* bind only to one interface */
	ifindex = device_index(fd, devname);

	struct sockaddr_ll sall;
	sall.sll_ifindex = ifindex;
	sall.sll_family = AF_PACKET;
	sall.sll_protocol = htons(ETH_P_ALL);
	
	ret = bind(fd, (struct sockaddr*)&sall, sizeof(sall));
	if (ret != 0)
		perror("bind failed");

	device_promisc(fd, devname, 1);
	arphrd = device_get_arptype(fd, devname);

	return fd;
}


#if 0
static void
device_address(int fd, const char *if_name)
{
	struct ifreq req;
 
	strncpy(req.ifr_name, if_name, IFNAMSIZ);
	req.ifr_addr.sa_family = AF_INET;

	ioctl(fd, SIOCGIFHWADDR, &req);
	// ioctl(fd, SIOCGIFADDR, &req);
	DEBUG("hw %s\n", ether_sprintf((const unsigned char *)&req.ifr_hwaddr.sa_data));
}
#endif


static int
device_index(int fd, const char *if_name)
{
	struct ifreq req;
 
	strncpy(req.ifr_name, if_name, IFNAMSIZ);
	req.ifr_addr.sa_family = AF_INET;

	ioctl(fd, SIOCGIFINDEX, &req);

	if (req.ifr_ifindex<0) {
		perror("interface not found");
		exit(1);
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

	if (ioctl(fd, SIOCGIFFLAGS, &req) == -1) {
		perror("cound not get flags");
		exit(1);
	}
	
	/* put interface up in any case */
	req.ifr_flags |= IFF_UP;
	
	if (on)
		req.ifr_flags |= IFF_PROMISC;
	else
		req.ifr_flags &= ~IFF_PROMISC;

	if (ioctl(fd, SIOCSIFFLAGS, &req) == -1) {
		perror("cound not set promisc mode");
		exit(1);
	}
}


/*
 *  Get the hardware type of the given interface as ARPHRD_xxx constant.
 */
static int
device_get_arptype(int fd, const char *device)
{
	struct ifreq    ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
		perror("SIOCGIFHWADDR");
		return -1;
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
convert_string_to_mac(const char* string, unsigned char* mac)
{
	int c;
	for(c=0; c < 6 && string; c++)
	{
		int x = 0;
		if (string)
			sscanf(string, "%x", &x);
		mac[c] = x;
		string = strchr(string, ':');
		if (string)
			string++;
	}
}

void
get_options(int argc, char** argv)
{
	int c;
	
	while((c = getopt(argc, argv, "hqi:p:e:")) > 0) {
		switch (c) {
			case 'p':
				rport = atoi(optarg);
				break;
			case 'q':
				quiet = 1;
				break;
			case 'i':
				ifname = optarg;
				break;
			case 'e':
				do_filter = 1;
				convert_string_to_mac(optarg, filtermac);
				printf("%s\n",ether_sprintf(filtermac));
				exit(0);
				break;
			case 'h':
			default:
				printf("usage: %s [-q] [-i <interface>] [-p <remote port>] -e <filtermac>\n\n", argv[0]);
				exit(0);
				break;
		}
	}
}

void
finish_all(int sig)
{
	device_promisc(mon, ifname, 0);
	close(mon);
#if !DO_DEBUG
	if (rport)
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
	if (p->ip_src)
		n->ip_src = p->ip_src;
	n->status=1;
	n->pkt_types |= p->pkt_types;
	n->wlan_mode = p->wlan_mode;
	n->last_seen = time(NULL);
	if (p->olsr_tc)
		n->olsr_tc = p->olsr_tc;
	if (p->olsr_neigh)
		n->olsr_neigh = p->olsr_neigh;
	n->pkt_count++;
	if (p->pkt_types & PKT_TYPE_OLSR)
		n->olsr_count++;
	if (p->wlan_bssid[0] != 0xff)
		memcpy(n->wlan_bssid, p->wlan_bssid, 6);
	if (p->pkt_types & PKT_TYPE_BEACON) {
		n->tsfl = *(unsigned long*)(&p->wlan_tsf[0]);
		n->tsfh = *(unsigned long*)(&p->wlan_tsf[4]);
	}
	if (p->snr > n->snr_max)
		n->snr_max = p->snr;
	if ((n->snr_min == 0 && p->snr > 0) || p->snr < n->snr_min)
		n->snr_min = p->snr;
}

static int
node_update(struct packet_info* pkt)
{
	int i;
	
	for (i=0;i<MAX_NODES;i++) {
		if (nodes[i].status == 1) {
			/* check existing node */
			if (memcmp(pkt->wlan_src, nodes[i].last_pkt.wlan_src, 6) == 0) {
				//wprintw(list_win,"found");
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
	if (!(pkt->pkt_types & PKT_TYPE_BEACON)) {
		return;
	}

	DEBUG("SPLIT check ibss '%s' node %s ", pkt->wlan_essid,
		ether_sprintf(pkt->wlan_src));
	DEBUG("bssid %s\n", ether_sprintf(pkt->wlan_bssid));

	/* find essid */
	for (i=0; i<MAX_ESSIDS; i++) {
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
	for (n=0; n<essids[i].num_nodes && n<MAX_NODES; n++) {
		if (essids[i].nodes[n] == pkt_node) {
			DEBUG("SPLIT   node found %d\n", n);
			break;
		}
	}

	DEBUG("SPLIT   at essid %d count %d node %d\n",i, essids[i].num_nodes, n);

	/* new essid */
	if (essids[i].num_nodes==0) {
		DEBUG("SPLIT   new essid '%s'\n",pkt->wlan_essid);
		strncpy(essids[i].essid, pkt->wlan_essid, MAX_ESSID_LEN);
	}

	/* new node */
	if (essids[i].num_nodes==0 || essids[i].nodes[n] != pkt_node) {
		DEBUG("SPLIT   recorded new node nr %d %d %s\n", n, pkt_node,
			ether_sprintf(pkt->wlan_src) );
		essids[i].nodes[n] = pkt_node;
		essids[i].num_nodes = n+1;
		nodes[pkt_node].essid = i;
	}

	/* check for split */
	essids[i].split = 0;
	for (n=0; n<essids[i].num_nodes && n<MAX_NODES; n++) {
		node = &nodes[essids[i].nodes[n]];
		DEBUG("SPLIT      %d. node %d src %s", n,
			essids[i].nodes[n], ether_sprintf(node->last_pkt.wlan_src));
		DEBUG(" bssid %s\n", ether_sprintf(node->wlan_bssid));

		if (last_bssid && memcmp(last_bssid,node->wlan_bssid,6) != 0) {
			essids[i].split = 1;
			//XXX count number of different bssids
			DEBUG("SPLIT *** DETECTED!!! %d different bssids\n", essids[i].split);
		}
		last_bssid = node->wlan_bssid;
	}

	/* if a split occurred on this essid, record it */
	//XXX record a list of all split essids
	if (essids[i].split>0) {
		DEBUG("SPLIT *** new record %d\n", i);
		splits.count = 1;
		splits.essid[0] = i;
	}
	else {
		splits.count = 0;
	}
}
