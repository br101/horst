/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2011 Bruno Randolf (br1@einfach.org)
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "util.h"
#include "ieee80211.h"


static struct timeval last_nodetimeout;


void remove_node_from_essid(struct node_info* n);


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
	ewma_add(&n->phy_snr_avg, p->phy_snr);
	if (p->phy_snr > n->phy_snr_max)
		n->phy_snr_max = p->phy_snr;
	if (p->phy_signal > n->phy_sig_max || n->phy_sig_max == 0)
		n->phy_sig_max = p->phy_signal;
	if ((n->phy_snr_min == 0 && p->phy_snr > 0) || p->phy_snr < n->phy_snr_min)
		n->phy_snr_min = p->phy_snr;
	if (p->wlan_channel != 0)
		n->wlan_channel = p->wlan_channel;
	else if (p->pkt_chan_idx >= 0)
		n->wlan_channel = channels[p->pkt_chan_idx].chan;

	if (!IEEE80211_IS_CTRL(p->wlan_type))
		n->wlan_wep = p->wlan_wep;
	if (p->wlan_seqno != 0) {
		if (p->wlan_retry && p->wlan_seqno == n->wlan_seqno) {
			n->wlan_retries_all++;
			n->wlan_retries_last++;
		} else
			n->wlan_retries_last = 0;
		n->wlan_seqno = p->wlan_seqno;
	}
}


struct node_info*
node_update(struct packet_info* p)
{
	struct node_info* n;

	if (p->wlan_src[0] == 0 && p->wlan_src[1] == 0 &&
	    p->wlan_src[2] == 0 && p->wlan_src[3] == 0 &&
	    p->wlan_src[4] == 0 && p->wlan_src[5] == 0)
		return NULL;

	/* find node by wlan source address */
	list_for_each_entry(n, &nodes, list) {
		if (memcmp(p->wlan_src, n->last_pkt.wlan_src, MAC_LEN) == 0) {
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
		ewma_init(&n->phy_snr_avg, 1024, 8);
		INIT_LIST_HEAD(&n->on_channels);
		list_add_tail(&n->list, &nodes);
	}

	copy_nodeinfo(n, p);

	return n;
}

void
timeout_nodes(void)
{
	struct node_info *n, *m;
	struct chan_node *cn, *cn2;

	if ((the_time.tv_sec - last_nodetimeout.tv_sec) < conf.node_timeout )
		return;

	list_for_each_entry_safe(n, m, &nodes, list) {
		if (n->last_seen < (the_time.tv_sec - conf.node_timeout)) {
			list_del(&n->list);
			if (n->essid != NULL)
				remove_node_from_essid(n);
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
