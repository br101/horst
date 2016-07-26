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

#include <stdlib.h>
#include <string.h>

#include <uwifi/wlan80211.h>
#include <uwifi/node.h>
#include <uwifi/util.h>

#include "main.h"
#include "essid.h"


static void update_essid_split_status(struct essid_info* e)
{
	struct uwifi_node* n;
	unsigned char* last_bssid = NULL;

	e->split = 0;

	/* essid can't be split if it only contains 1 node */
	if (e->num_nodes <= 1 && essids.split_essid == e) {
		essids.split_active = 0;
		essids.split_essid = NULL;
		return;
	}

	/* check for split */
	list_for_each(&e->nodes, n, essid_nodes) {
		DBG_PRINT("SPLIT      node %p src " MAC_FMT " bssid " MAC_FMT "\n",
			n, MAC_PAR(n->wlan_src), MAC_PAR(n->wlan_bssid));

		if (n->wlan_mode & WLAN_MODE_AP)
			continue;

		if (last_bssid && memcmp(last_bssid, n->wlan_bssid, WLAN_MAC_LEN) != 0) {
			e->split = 1;
			DBG_PRINT("SPLIT *** DETECTED!!!\n");
		}
		last_bssid = n->wlan_bssid;
	}

	/* if a split occurred on this essid, record it */
	if (e->split > 0) {
		DBG_PRINT("SPLIT *** active\n");
		essids.split_active = 1;
		essids.split_essid = e;
	}
	else if (e == essids.split_essid) {
		DBG_PRINT("SPLIT *** ok now\n");
		essids.split_active = 0;
		essids.split_essid = NULL;
	}
}

void remove_node_from_essid(struct uwifi_node* n)
{
	DBG_PRINT("SPLIT   remove node from old essid\n");
	list_del(&n->essid_nodes);
	n->essid->num_nodes--;

	update_essid_split_status(n->essid);

	/* delete essid if it has no more nodes */
	if (n->essid->num_nodes == 0) {
		DBG_PRINT("SPLIT   essid empty, delete\n");
		list_del(&n->essid->list);
		free(n->essid);
	}
	n->essid = NULL;
}

void update_essids(struct uwifi_packet* p, struct uwifi_node* n)
{
	struct essid_info* e;

	if (n == NULL || (p->phy_flags & PHY_FLAG_BADFCS))
		return; /* ignore */

	/* only check beacons and probe response frames */
	if ((p->wlan_type != WLAN_FRAME_BEACON) &&
	    (p->wlan_type != WLAN_FRAME_PROBE_RESP))
		return;

	DBG_PRINT("SPLIT check ibss '%s' node " MAC_FMT "bssid " MAC_FMT "\n" , p->wlan_essid,
		MAC_PAR(n->wlan_src), MAC_PAR(p->wlan_bssid));

	/* find essid if already recorded */
	list_for_each(&essids.list, e, list) {
		if (strncmp(e->essid, p->wlan_essid, WLAN_MAX_SSID_LEN) == 0) {
			DBG_PRINT("SPLIT   essid found\n");
			break;
		}
	}

	/* if not add new essid */
	if (&e->list == &essids.list.n) {
		DBG_PRINT("SPLIT   essid not found, adding new\n");
		e = malloc(sizeof(struct essid_info));
		strncpy(e->essid, p->wlan_essid, WLAN_MAX_SSID_LEN);
		e->essid[WLAN_MAX_SSID_LEN-1] = '\0';
		e->num_nodes = 0;
		e->split = 0;
		list_head_init(&e->nodes);
		list_add_tail(&essids.list, &e->list);
	}

	/* if node had another essid before, remove it there */
	if (n->essid != NULL && n->essid != e)
		remove_node_from_essid(n);

	/* new node */
	if (n->essid == NULL) {
		DBG_PRINT("SPLIT   node not found, adding new " MAC_FMT "\n",
			MAC_PAR(n->wlan_src));
		list_add_tail(&e->nodes, &n->essid_nodes);
		e->num_nodes++;
		n->essid = e;
	}

	update_essid_split_status(e);
}
