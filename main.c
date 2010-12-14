/* horst - Highly Optimized Radio Scanning Tool
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol_parser.h"
#include "display.h"
#include "network.h"
#include "main.h"
#include "capture.h"
#include "util.h"
#include "ieee80211.h"
#include "ieee80211_util.h"
#include "wext.h"

struct list_head nodes;
struct essid_meta_info essids;
struct history hist;
struct statistics stats;
struct chan_freq channels[MAX_CHANNELS];
struct channel_info spectrum[MAX_CHANNELS];

struct config conf = {
	.node_timeout		= NODE_TIMEOUT,
	.channel_time		= CHANNEL_TIME,
	.ifname			= INTERFACE_NAME,
	.display_interval	= DISPLAY_UPDATE_INTERVAL,
	.sleep_time		= SLEEP_TIME,
	.filter_pkt		= 0xffffff,
	.recv_buffer_size	= RECV_BUFFER_SIZE,
	.port			= DEFAULT_PORT,
};

struct timeval the_time;

static int mon; /* monitoring socket */
static FILE* DF = NULL;
static struct timeval last_nodetimeout;
static struct timeval last_channelchange;

/*
 * receive packet buffer
 * size: max 80211 frame (2312) + space for prism2 header (144)
 * or radiotap header (usually only 26) + some extra
 */
static unsigned char buffer[2312 + 200];

/* for select */
static fd_set read_fds;
static fd_set write_fds;
static fd_set excpt_fds;
static struct timeval tv;


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
	    !(p->wlan_bssid[0] == 0 && p->wlan_bssid[1] == 0 &&
	      p->wlan_bssid[2] == 0 && p->wlan_bssid[3] == 0 &&
	      p->wlan_bssid[4] == 0 && p->wlan_bssid[5] == 0)) {
		memcpy(n->wlan_bssid, p->wlan_bssid, MAC_LEN);
	}
	if (IEEE80211_IS_MGMT_STYPE(p->wlan_type, IEEE80211_STYPE_BEACON)) {
		n->wlan_tsf = p->wlan_tsf;
		n->wlan_bintval = p->wlan_bintval;
	}
	iir_average(n->phy_snr_avg, p->phy_snr);
	iir_average(n->phy_sig_avg, p->phy_signal);
	if (p->phy_snr > n->phy_snr_max)
		n->phy_snr_max = p->phy_snr;
	if (p->phy_signal > n->phy_sig_max || n->phy_sig_max == 0)
		n->phy_sig_max = p->phy_signal;
	if ((n->phy_snr_min == 0 && p->phy_snr > 0) || p->phy_snr < n->phy_snr_min)
		n->phy_snr_min = p->phy_snr;
	if (p->wlan_channel != 0)
		n->wlan_channel = p->wlan_channel;
	if (!IEEE80211_IS_CTRL(p->wlan_type))
		n->wlan_wep = p->wlan_wep;
	if (p->wlan_seqno != 0) {
		if (p->wlan_retry && p->wlan_seqno == n->wlan_seqno) {
			n->wlan_retries_all++;
			n->wlan_retries_last++;
		} else {
			n->wlan_retries_last = 0;
		}
		n->wlan_seqno = p->wlan_seqno;
	}
}


static struct node_info*
node_update(struct packet_info* pkt)
{
	struct node_info* n;

	if (pkt->wlan_src[0] == 0 && pkt->wlan_src[1] == 0 &&
	    pkt->wlan_src[2] == 0 && pkt->wlan_src[3] == 0 &&
	    pkt->wlan_src[4] == 0 && pkt->wlan_src[5] == 0) {
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
		DEBUG("node adding\n");
		n = malloc(sizeof(struct node_info));
		memset(n, 0, sizeof(struct node_info));
		n->essid = NULL;
		INIT_LIST_HEAD(&n->on_channels);
		list_add_tail(&n->list, &nodes);
	}

	copy_nodeinfo(n, pkt);

	return n;
}


static void
update_essid_split_status(struct essid_info* e)
{
	struct node_info* n;
	unsigned char* last_bssid = NULL;

	e->split = 0;

	/* essid can't be split if it only contains 1 node */
	if (e->num_nodes <= 1 && essids.split_essid == e) {
		essids.split_active = 0;
		essids.split_essid = NULL;
		return;
	}

	/* check for split */
	list_for_each_entry(n, &e->nodes, essid_nodes) {
		DEBUG("SPLIT      node %p src %s",
			n, ether_sprintf(n->last_pkt.wlan_src));
		DEBUG(" bssid %s\n", ether_sprintf(n->wlan_bssid));

		if (n->wlan_mode == WLAN_MODE_AP) {
			continue;
		}

		if (last_bssid && memcmp(last_bssid, n->wlan_bssid, MAC_LEN) != 0) {
			e->split = 1;
			DEBUG("SPLIT *** DETECTED!!!\n");
		}
		last_bssid = n->wlan_bssid;
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


static void
remove_node_from_essid(struct node_info* node)
{
	DEBUG("SPLIT   remove node from old essid\n");
	list_del(&node->essid_nodes);
	node->essid->num_nodes--;

	update_essid_split_status(node->essid);

	/* delete essid if it has no more nodes */
	if (node->essid->num_nodes == 0) {
		DEBUG("SPLIT   essid empty, delete\n");
		list_del(&node->essid->list);
		free(node->essid);
	}
	node->essid = NULL;
}


static void
check_ibss_split(struct packet_info* pkt, struct node_info* pkt_node)
{
	struct essid_info* e;

	/* only check beacons (XXX: what about PROBE?) */
	if (!IEEE80211_IS_MGMT_STYPE(pkt->wlan_type, IEEE80211_STYPE_BEACON)) {
		return;
	}

	if (pkt_node == NULL)
		return;

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

	/* if node had another essid before, remove it there */
	if (pkt_node->essid != NULL && pkt_node->essid != e) {
		remove_node_from_essid(pkt_node);
	}

	/* new node */
	if (pkt_node->essid == NULL) {
		DEBUG("SPLIT   node not found, adding new %s\n",
			ether_sprintf(pkt->wlan_src));
		list_add_tail(&pkt_node->essid_nodes, &e->nodes);
		e->num_nodes++;
		pkt_node->essid = e;
	}

	update_essid_split_status(e);
}


static int
filter_packet(struct packet_info* pkt)
{
	int i;

	if (conf.filter_off) {
		return 0;
	}

	if (!(pkt->pkt_types & conf.filter_pkt)) {
		stats.filtered_packets++;
		return 1;
	}

	if (MAC_NOT_EMPTY(conf.filterbssid) &&
	    memcmp(pkt->wlan_bssid, conf.filterbssid, MAC_LEN) != 0) {
		stats.filtered_packets++;
		return 1;
	}

	if (conf.do_macfilter) {
		for (i = 0; i < MAX_FILTERMAC; i++) {
			if (MAC_NOT_EMPTY(pkt->wlan_src) &&
			    conf.filtermac_enabled[i] &&
			    memcmp(pkt->wlan_src, conf.filtermac[i], MAC_LEN) == 0) {
				return 0;
			}
		}
		stats.filtered_packets++;
		return 1;
	}
	return 0;
}


static void
update_history(struct packet_info* p)
{
	if (p->phy_signal == 0) {
		return;
	}

	hist.signal[hist.index] = p->phy_signal;
	hist.noise[hist.index] = p->phy_noise;
	hist.rate[hist.index] = p->phy_rate;
	hist.type[hist.index] = p->wlan_type;
	hist.retry[hist.index] = p->wlan_retry;
	hist.index++;
	if (hist.index == MAX_HISTORY) {
		hist.index = 0;
	}
}


static void
update_statistics(struct packet_info* p)
{
	if (p->phy_rate == 0) {
		return;
	}

	stats.packets++;
	stats.bytes += p->pkt_len;

	if (p->phy_rate > 0 && p->phy_rate < MAX_RATES) {
		stats.duration += p->pkt_duration;
		stats.packets_per_rate[p->phy_rate]++;
		stats.bytes_per_rate[p->phy_rate] += p->pkt_len;
		stats.duration_per_rate[p->phy_rate] += p->pkt_duration;
	}
	if (p->wlan_type >= 0 && p->wlan_type < MAX_FSTYPE) {
		stats.packets_per_type[p->wlan_type]++;
		stats.bytes_per_type[p->wlan_type] += p->pkt_len;
		if (p->phy_rate > 0 && p->phy_rate < MAX_RATES)
			stats.duration_per_type[p->wlan_type] += p->pkt_duration;
	}
}


static void
update_spectrum(struct packet_info* p, struct node_info* n)
{
	struct channel_info* chan;
	struct chan_node* cn;
	int i;

	/* if physical channel not available from pkt, best guess from config */
	if (!p->phy_chan)
		i = conf.current_channel;
	else {
		/* find channel index from packet channel */
		for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++)
			if (channels[i].chan == p->phy_chan)
				break;
	}

	if (i < 0 || i >= conf.num_channels || i >= MAX_CHANNELS)
		return; /* chan not found */

	chan = &spectrum[i];
	chan->signal = p->phy_signal;
	chan->packets++;
	chan->bytes += p->pkt_len;
	chan->durations += p->pkt_duration;
	chan->signal_avg = iir_average(chan->signal_avg, chan->signal);

	if (!n)
		return;

	/* add node to channel if not already there */
	list_for_each_entry(cn, &chan->nodes, chan_list) {
		if (cn->node == n) {
			DEBUG("SPEC node found %p\n", cn->node);
			break;
		}
	}
	if (cn->node != n) {
		DEBUG("SPEC node adding %p chan %d\n", n, i);
		cn = malloc(sizeof(struct chan_node));
		cn->node = n;
		cn->chan = chan;
		list_add_tail(&cn->chan_list, &chan->nodes);
		list_add_tail(&cn->node_list, &n->on_channels);
		chan->num_nodes++;
		n->num_on_channels++;
	}
	/* keep signal of this node as seen on this channel */
	cn->sig = p->phy_signal;
	cn->sig_avg = iir_average(cn->sig_avg, cn->sig);
	cn->packets++;
}


static void 
write_to_file(struct packet_info* pkt)
{
	fprintf(DF, "%s, %s, ",
		get_packet_type_name(pkt->wlan_type), ether_sprintf(pkt->wlan_src));
	fprintf(DF, "%s, ", ether_sprintf(pkt->wlan_dst));
	fprintf(DF, "%s, ", ether_sprintf(pkt->wlan_bssid));
	fprintf(DF, "%x, %d, %d, %d, %d, %d, ",
		pkt->pkt_types, pkt->phy_signal, pkt->phy_noise, pkt->phy_snr,
		pkt->pkt_len, pkt->phy_rate);
	fprintf(DF, "%016llx, ", (unsigned long long)pkt->wlan_tsf);
	fprintf(DF, "%s, %d, %d, %d, ",
		pkt->wlan_essid, pkt->wlan_mode, pkt->wlan_channel, pkt->wlan_wep);
	fprintf(DF, "%s, ", ip_sprintf(pkt->ip_src));
	fprintf(DF, "%s, ", ip_sprintf(pkt->ip_dst));
	fprintf(DF, "%d, %d, %d\n", pkt->olsr_type, pkt->olsr_neigh, pkt->olsr_tc);
}


static void
timeout_nodes(void)
{
	struct node_info *n, *m;
	struct chan_node *cn, *cn2;

	if ((the_time.tv_sec - last_nodetimeout.tv_sec) < conf.node_timeout ) {
		return;
	}

	list_for_each_entry_safe(n, m, &nodes, list) {
		if (n->last_seen < (the_time.tv_sec - conf.node_timeout)) {
			list_del(&n->list);
			if (n->essid != NULL) {
				remove_node_from_essid(n);
			}
			list_for_each_entry_safe(cn, cn2, &n->on_channels, node_list) {
				list_del(&cn->node_list);
				list_del(&cn->chan_list);
				cn->chan->num_nodes--;
				free(cn);
			}
			free(n);
		}
	}
	last_nodetimeout = the_time;
}


static void
handle_packet(struct packet_info* p)
{
	struct node_info* node;

	if (conf.port && cli_fd != -1) {
		net_send_packet(p);
	}
	if (conf.dumpfile != NULL) {
		write_to_file(p);
	}
	if (conf.quiet || conf.paused) {
		return;
	}

	/* in display mode */
	if (filter_packet(p)) {
		return;
	}

	p->pkt_duration = ieee80211_frame_duration(p->phy_flags & PHY_FLAG_MODE_MASK,
			p->pkt_len, p->phy_rate * 5, p->phy_flags & PHY_FLAG_SHORTPRE,
			0 /*shortslot*/, p->wlan_type, p->wlan_qos_class,
			p->wlan_retries);

	node = node_update(p);

	if (node)
		p->wlan_retries = node->wlan_retries_last;

	update_history(p);
	update_statistics(p);
	update_spectrum(p, node);
	check_ibss_split(p, node);

#if !DO_DEBUG
	update_display(p, node);
#endif
}


static void
receive_packet(unsigned char* buffer, int len)
{
	struct packet_info current_packet;

#if DO_DEBUG
	dump_packet(buffer, len);
#endif
	memset(&current_packet, 0, sizeof(current_packet));

	if (!conf.serveraddr) {
		/* local capture */
		if (!parse_packet(buffer, len, &current_packet)) {
			DEBUG("parsing failed\n");
			return;
		}
	}
	else {
		/* client mode - receiving pre-parsed from server */
		if (!net_receive_packet(buffer, len, &current_packet)) {
			DEBUG("receive failed\n");
			return;
		}
	}

	handle_packet(&current_packet);
}


static void
receive_any(void)
{
	int ret, len, mfd;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&excpt_fds);

	FD_SET(0, &read_fds);
	FD_SET(mon, &read_fds);
	if (srv_fd != -1) {
		FD_SET(srv_fd, &read_fds);
	}
	tv.tv_sec = 0;
	tv.tv_usec = conf.sleep_time;
	mfd = max(mon, srv_fd) + 1;

	ret = select(mfd, &read_fds, &write_fds, &excpt_fds, &tv);
	if (ret == -1 && errno == EINTR) { /* interrupted */
		return;
	}
	if (ret == 0) { /* timeout */
		return;
	}
	if (ret < 0) {
		err(1, "select()");
	}

	/* stdin */
	if (FD_ISSET(0, &read_fds)) {
		handle_user_input();
	}

	/* packet */
	if (FD_ISSET(mon, &read_fds)) {
		len = recv_packet(mon, buffer, sizeof(buffer));
		receive_packet(buffer, len);
	}

	/* server */
	if (srv_fd != -1 && FD_ISSET(srv_fd, &read_fds)) {
		net_handle_server_conn();
	}
}


static void
sigpipe_handler(int sig)
{
	/* ignore signal here - we will handle it after write failed */
}


static void
get_options(int argc, char** argv)
{
	int c;
	static int n;

	while((c = getopt(argc, argv, "hqsi:t:p:e:d:w:o:b:c:")) > 0) {
		switch (c) {
		case 'p':
			conf.port = optarg;
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
			conf.do_change_channel = 1;
			break;
		case 'd':
			conf.display_interval = atoi(optarg) * 1000;
			break;
		case 'w':
			conf.sleep_time = atoi(optarg);
			break;
		case 'e':
			if (n >= MAX_FILTERMAC)
				break;
			conf.do_macfilter = 1;
			convert_string_to_mac(optarg, conf.filtermac[n]);
			conf.filtermac_enabled[n] = 1;
			n++;
			break;
		case 'c':
			conf.serveraddr = optarg;
			break;
		case 'h':
		default:
			printf("usage: %s [-h] [-q] [-i interface] [-t sec] [-p port] [-e mac] [-d usec] [-w usec] [-o file]\n\n"
				"Options (default value)\n"
				"  -h\t\tthis help\n"
				"  -q\t\tquiet [basically useless]\n"
				"  -s\t\tscan (change channel automatically)\n"
				"  -i <intf>\tinterface (wlan0)\n"
				"  -t <sec>\tnode timeout (60)\n"
				"  -c <IP>\tconnect to server\n"
				"  -p <port>\tuse port (4444)\n"
				"  -e <mac>\tfilter all macs except these (multiple)\n"
				"  -d <ms>\tdisplay update interval (100)\n"
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
	int i;
	struct essid_info *e, *f;
	struct node_info *ni, *mi;
	struct chan_node *cn, *cn2;

	/* free node list */
	list_for_each_entry_safe(ni, mi, &nodes, list) {
		DEBUG("free node %s\n", ether_sprintf(ni->last_pkt.wlan_src));
		list_del(&ni->list);
		free(ni);
	}

	/* free essids */
	list_for_each_entry_safe(e, f, &essids.list, list) {
		DEBUG("free essid '%s'\n", e->essid);
		list_del(&e->list);
		free(e);
	}

	/* free channel nodes */
	for (i = 0; i < conf.num_channels; i++) {
		list_for_each_entry_safe(cn, cn2, &spectrum[i].nodes, chan_list) {
			DEBUG("free chan_node %p\n", cn);
			list_del(&cn->chan_list);
			cn->chan->num_nodes--;
			free(cn);
		}
	}
}


void
finish_all(int sig)
{
	free_lists();

	if (!conf.serveraddr) {
		close_packet_socket();
	}

	if (DF != NULL) {
		fclose(DF);
	}

#if !DO_DEBUG
	if (conf.port) {
		net_finish();
	}

	if (!conf.quiet) {
		finish_display(sig);
	}
#endif
	exit(0);
}


#if 0
void print_rate_duration_table(void)
{
	int i;

	printf("LEN\t1M l\t1M s\t2M l\t2M s\t5.5M l\t5.5M s\t11M l\t11M s\t");
	printf("6M\t9\t12M\t18M\t24M\t36M\t48M\t54M\n");
	for (i=10; i<=2304; i+=10) {
		printf("%d:\t%d\t%d\t", i,
			ieee80211_frame_duration(PHY_FLAG_G, i, 10, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 10, 1));
		printf("%d\t%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 20, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 20, 1));
		printf("%d\t%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 55, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 55, 1));
		printf("%d\t%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 110, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 110, 1));

		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 60, 1));
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 90, 1));
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 120, 1)),
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 180, 1)),
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 240, 1)),
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 360, 1));
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 480, 1)),
		printf("%d\n",
			ieee80211_frame_duration(PHY_FLAG_G, i, 540, 1));
	}
}
#endif


void
auto_change_channel(void)
{
	if (the_time.tv_sec == last_channelchange.tv_sec &&
	     (the_time.tv_usec - last_channelchange.tv_usec) < conf.channel_time) {
		return;
	}

	if (conf.current_channel >= 0)
		spectrum[conf.current_channel].durations_last =
					spectrum[conf.current_channel].durations;

	last_channelchange = the_time;

	if (conf.do_change_channel == 0)
		return;

	conf.current_channel++;
	if (conf.current_channel >= conf.num_channels ||
		conf.current_channel >= MAX_CHANNELS ||
		(conf.channel_max && conf.current_channel >= conf.channel_max))
	    conf.current_channel = 0;

	wext_set_channel(mon, conf.ifname, channels[conf.current_channel].freq);
}


void
init_channels(void)
{
	int i, freq, ch;

	conf.current_channel = -1;

	/* get all available channels */
	conf.num_channels = wext_get_channels(mon, conf.ifname, channels);
	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++)
		INIT_LIST_HEAD(&spectrum[i].nodes);

	/* get current channel &  map to our channel array */
	freq = wext_get_freq(mon, conf.ifname);
	if (freq == 0)
		return;

	ch = ieee80211_frequency_to_channel(freq);
	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++)
		if (channels[i].chan == ch)
			break;

	if (i < MAX_CHANNELS)
		conf.current_channel = i;
	DEBUG("***%d\n", conf.current_channel);
}


int
main(int argc, char** argv)
{
	INIT_LIST_HEAD(&essids.list);
	INIT_LIST_HEAD(&nodes);

	get_options(argc, argv);

	signal(SIGINT, finish_all);
	signal(SIGPIPE, sigpipe_handler);

	gettimeofday(&stats.stats_time, NULL);

	if (conf.serveraddr) {
		mon = net_open_client_socket(conf.serveraddr, conf.port);
	}
	else {
		mon = open_packet_socket(conf.ifname, sizeof(buffer), conf.recv_buffer_size);
		if (mon < 0) {
			err(1, "couldn't open packet socket");
		}
		conf.arphrd = device_get_arptype();
		if (conf.arphrd != ARPHRD_IEEE80211_PRISM &&
		conf.arphrd != ARPHRD_IEEE80211_RADIOTAP) {
			printf("wrong monitor type. please use radiotap or prism2 headers\n");
			exit(1);
		}
		init_channels();
	}

	if (conf.dumpfile != NULL) {
		DF = fopen(conf.dumpfile, "w");
		if (DF == NULL) {
			err(1, "couldn't open dump file");
		}
	}

	if (!conf.serveraddr && conf.port) {
		net_init_server_socket(conf.port);
	}
#if !DO_DEBUG
	if (!conf.quiet) {
		init_display();
	}
#endif

	for ( /* ever */ ;;)
	{
		receive_any();
		gettimeofday(&the_time, NULL);
		timeout_nodes();
		auto_change_channel();
	}
	/* will never */
	return 0;
}


void
change_channel(int c)
{
	int i;

	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++) {
		if (channels[i].chan == c) {
			wext_set_channel(mon, conf.ifname, channels[i].freq);
			conf.current_channel = i;
			break;
		}
	}
}
