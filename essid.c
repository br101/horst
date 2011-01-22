#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "util.h"
#include "ieee80211.h"

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


void
remove_node_from_essid(struct node_info* n)
{
	DEBUG("SPLIT   remove node from old essid\n");
	list_del(&n->essid_nodes);
	n->essid->num_nodes--;

	update_essid_split_status(n->essid);

	/* delete essid if it has no more nodes */
	if (n->essid->num_nodes == 0) {
		DEBUG("SPLIT   essid empty, delete\n");
		list_del(&n->essid->list);
		free(n->essid);
	}
	n->essid = NULL;
}


void
update_essids(struct packet_info* p, struct node_info* n)
{
	struct essid_info* e;

	/* only check beacons (XXX: what about PROBE?) */
	if (!IEEE80211_IS_MGMT_STYPE(p->wlan_type, IEEE80211_STYPE_BEACON)) {
		return;
	}

	if (n == NULL)
		return;

	DEBUG("SPLIT check ibss '%s' node %s ", p->wlan_essid,
		ether_sprintf(p->wlan_src));
	DEBUG("bssid %s\n", ether_sprintf(p->wlan_bssid));

	/* find essid if already recorded */
	list_for_each_entry(e, &essids.list, list) {
		if (strncmp(e->essid, p->wlan_essid, MAX_ESSID_LEN) == 0) {
			DEBUG("SPLIT   essid found\n");
			break;
		}
	}

	/* if not add new essid */
	if (&e->list == &essids.list) {
		DEBUG("SPLIT   essid not found, adding new\n");
		e = malloc(sizeof(struct essid_info));
		strncpy(e->essid, p->wlan_essid, MAX_ESSID_LEN);
		e->num_nodes = 0;
		e->split = 0;
		INIT_LIST_HEAD(&e->nodes);
		list_add_tail(&e->list, &essids.list);
	}

	/* if node had another essid before, remove it there */
	if (n->essid != NULL && n->essid != e) {
		remove_node_from_essid(n);
	}

	/* new node */
	if (n->essid == NULL) {
		DEBUG("SPLIT   node not found, adding new %s\n",
			ether_sprintf(p->wlan_src));
		list_add_tail(&n->essid_nodes, &e->nodes);
		e->num_nodes++;
		n->essid = e;
	}

	update_essid_split_status(e);
}
