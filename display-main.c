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

/******************* MAIN / OVERVIEW *******************/

#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "main.h"
#include "util.h"
#include "ieee80211.h"
#include "olsr_header.h"
#include "listsort.h"


static WINDOW *sort_win = NULL;
static WINDOW *dump_win = NULL;
static WINDOW *list_win = NULL;
static WINDOW *stat_win = NULL;

static int do_sort = 'n';
/* pointer to the sort function */
static int(*sortfunc)(const struct list_head*, const struct list_head*) = NULL;

/* sizes of split window (list_win & status_win) */
static int win_split;
static int stat_height;

static struct ewma usen_avg;
static struct ewma bpsn_avg;


/******************* UTIL *******************/

void
print_dump_win(const char *str, int refresh)
{
	wattron(dump_win, RED);
	wprintw(dump_win, str);
	wattroff(dump_win, RED);
	if (refresh)
		wrefresh(dump_win);
	else
		wnoutrefresh(dump_win);
}


/******************* SORTING *******************/

static int
compare_nodes_snr(const struct list_head *p1, const struct list_head *p2)
{
	struct node_info* n1 = list_entry(p1, struct node_info, list);
	struct node_info* n2 = list_entry(p2, struct node_info, list);

	if (n1->last_pkt.phy_snr > n2->last_pkt.phy_snr)
		return -1;
	else if (n1->last_pkt.phy_snr == n2->last_pkt.phy_snr)
		return 0;
	else
		return 1;
}


static int
compare_nodes_time(const struct list_head *p1, const struct list_head *p2)
{
	struct node_info* n1 = list_entry(p1, struct node_info, list);
	struct node_info* n2 = list_entry(p2, struct node_info, list);

	if (n1->last_seen > n2->last_seen)
		return -1;
	else if (n1->last_seen == n2->last_seen)
		return 0;
	else
		return 1;
}


static int
compare_nodes_channel(const struct list_head *p1, const struct list_head *p2)
{
	struct node_info* n1 = list_entry(p1, struct node_info, list);
	struct node_info* n2 = list_entry(p2, struct node_info, list);

	if (n1->wlan_channel < n2->wlan_channel)
		return 1;
	else if (n1->wlan_channel == n2->wlan_channel)
		return 0;
	else
		return -1;
}


static int
compare_nodes_bssid(const struct list_head *p1, const struct list_head *p2)
{
	struct node_info* n1 = list_entry(p1, struct node_info, list);
	struct node_info* n2 = list_entry(p2, struct node_info, list);

	return -memcmp(n1->wlan_bssid, n2->wlan_bssid, MAC_LEN);
}


static int
sort_input(int c)
{
	switch (c) {
	case 'n': case 'N': sortfunc = NULL; break;
	case 's': case 'S': sortfunc = compare_nodes_snr; break;
	case 't': case 'T': sortfunc = compare_nodes_time; break;
	case 'c': case 'C': sortfunc = compare_nodes_channel; break;
	case 'b': case 'B': sortfunc = compare_nodes_bssid; break;
	}

	switch (c) {
	case 'n': case 'N':
	case 's': case 'S':
	case 't': case 'T':
	case 'c': case 'C':
	case 'b': case 'B':
		do_sort = c;
		/* fall thru */
	case '\r': case KEY_ENTER:
		delwin(sort_win);
		sort_win = NULL;
		update_display(NULL, NULL);
		return 1;
	}
	return 0;
}


static void
show_sort_win(void)
{
	if (sort_win == NULL) {
		sort_win = newwin(1, COLS-2, win_split - 2, 1);
		wattron(sort_win, BLACKONWHITE);
		mvwhline(sort_win, 0, 0, ' ', COLS);
		mvwprintw(sort_win, 0, 0, " -> Sort by s:SNR t:Time b:BSSID c:Channel n:Don't sort [current: %c]", do_sort);
		wrefresh(sort_win);
	}
}


/******************* WINDOWS *******************/

#define STAT_WIDTH 11
#define STAT_START 4

static void
update_status_win(struct packet_info* p)
{
	int sig, siga, noi = 0, bps, dps, pps, rps, bpsn, usen;
	float use, rpsp = 0.0;
	int max_stat_bar = stat_height - STAT_START;
	struct channel_info* chan = NULL;

	if (p != NULL)
		werase(stat_win);

	wattron(stat_win, WHITE);
	mvwvline(stat_win, 0, 0, ACS_VLINE, stat_height);

	get_per_second(stats.bytes, stats.duration, stats.packets, stats.retries,
		       &bps, &dps, &pps, &rps);
	bps *= 8;
	bpsn = normalize(bps, 32000000, max_stat_bar); //theoretical: 54000000

	use = dps * 1.0 / 10000; /* usec, in percent */
	usen = normalize(use, 100, max_stat_bar);

	if (pps)
		rpsp = rps * 100.0 / pps;

	ewma_add(&usen_avg, usen);
	ewma_add(&bpsn_avg, bpsn);

	if (p != NULL) {
		sig = normalize_db(-p->phy_signal, max_stat_bar);
		if (p->phy_noise)
			noi = normalize_db(-p->phy_noise, max_stat_bar);

		if (p->pkt_chan_idx > 0)
			chan = &spectrum[p->pkt_chan_idx];

		if (chan != NULL && chan->packets >= 8)
			siga = normalize_db(ewma_read(&chan->signal_avg),
					    max_stat_bar);
		else
			siga = sig;

		wattron(stat_win, GREEN);
		if (conf.have_noise) {
			mvwprintw(stat_win, 0, 1, "S/ :-%02d/", -p->phy_signal);
			wattron(stat_win, RED);
			wprintw(stat_win, "%02d", -p->phy_noise);
			mvwprintw(stat_win, 0, 3, "N");
		}
		else
			mvwprintw(stat_win, 0, 1, "Sig: %5d", p->phy_signal);

		signal_average_bar(stat_win, sig, siga, STAT_START, 2, stat_height, 2);

		if (noi) {
			wattron(stat_win, ALLRED);
			mvwvline(stat_win, noi + STAT_START, 2, '=', stat_height - noi);
			mvwvline(stat_win, noi + STAT_START, 3, '=', stat_height - noi);
		}
	}

	wattron(stat_win, CYAN);
	mvwprintw(stat_win, 1, 1, "bps:%6s", kilo_mega_ize(bps));
	general_average_bar(stat_win, bpsn, ewma_read(&bpsn_avg),
			    stat_height, 5, max_stat_bar, 2,
			    CYAN, ALLCYAN);

	wattron(stat_win, YELLOW);
	mvwprintw(stat_win, 2, 1, "Use:%5.1f%%", use);
	general_average_bar(stat_win, usen, ewma_read(&usen_avg),
			    stat_height, 8, max_stat_bar, 2,
			    YELLOW, ALLYELLOW);

	mvwprintw(stat_win, 3, 1, "Retry: %2.0f%%", rpsp);

	wnoutrefresh(stat_win);
}


#define COL_PKT		3
#define COL_CHAN	COL_PKT + 7
#define COL_SNR		COL_CHAN + 3
#define COL_RATE	COL_SNR + 3
#define COL_SOURCE	COL_RATE + 3
#define COL_STA		COL_SOURCE + 18
#define COL_BSSID	COL_STA + 2
#define COL_ENC		COL_BSSID + 20
#define COL_IP		COL_ENC + 2

static char spin[4] = {'/', '-', '\\', '|'};

static void
print_list_line(int line, struct node_info* n)
{
	struct packet_info* p = &n->last_pkt;

	if (n->pkt_types & PKT_TYPE_OLSR)
		wattron(list_win, GREEN);
	if (n->last_seen > (the_time.tv_sec - conf.node_timeout / 2))
		wattron(list_win, A_BOLD);
	else
		wattron(list_win, A_NORMAL);

	if (n->essid != NULL && n->essid->split > 0)
		wattron(list_win, RED);

	mvwprintw(list_win, line, 1, "%c", spin[n->pkt_count % 4]);

	mvwprintw(list_win, line, COL_PKT, "%.0f/%.0f%%",
		  n->pkt_count * 100.0 / stats.packets,
		  n->wlan_retries_all * 100.0 / n->pkt_count);

	mvwprintw(list_win, line, COL_SNR, "%2d", ewma_read(&n->phy_snr_avg));

	if (n->wlan_mode == WLAN_MODE_AP )
		mvwprintw(list_win, line, COL_STA,"A");
	else if (n->wlan_mode == WLAN_MODE_IBSS )
		mvwprintw(list_win, line, COL_STA, "I");
	else if (n->wlan_mode == WLAN_MODE_STA )
		mvwprintw(list_win, line, COL_STA, "S");
	else if (n->wlan_mode == WLAN_MODE_PROBE )
		mvwprintw(list_win, line, COL_STA, "P");

	mvwprintw(list_win, line, COL_ENC, n->wlan_wep ? "W" : "");

	mvwprintw(list_win, line, COL_RATE, "%2d", p->phy_rate/2);
	mvwprintw(list_win, line, COL_SOURCE, "%s", ether_sprintf(p->wlan_src));
	mvwprintw(list_win, line, COL_BSSID, "(%s)", ether_sprintf(n->wlan_bssid));

	if (n->wlan_channel)
		mvwprintw(list_win, line, COL_CHAN, "%2d", n->wlan_channel );

	if (n->pkt_types & PKT_TYPE_IP)
		mvwprintw(list_win, line, COL_IP, "%s", ip_sprintf(n->ip_src));

	if (n->pkt_types & PKT_TYPE_OLSR)
		wprintw(list_win, " OLSR%s N:%d %s",
			n->pkt_types & PKT_TYPE_OLSR_LQ ? "_LQ" : "",
			n->olsr_neigh,
			n->pkt_types & PKT_TYPE_OLSR_GW ? "GW" : "");

	if (n->pkt_types & PKT_TYPE_BATMAN)
		wprintw(list_win, " BAT");

	if (n->pkt_types & (PKT_TYPE_MESHZ))
		wprintw(list_win, " MC");

	wattroff(list_win, A_BOLD);
	wattroff(list_win, GREEN);
	wattroff(list_win, RED);
}


static void
update_list_win(void)
{
	struct node_info* n;
	int line = 0, nadd = 0;

	werase(list_win);
	wattron(list_win, WHITE);
	box(list_win, 0 , 0);
	mvwprintw(list_win, 0, COL_PKT, "Pk/Re%%");
	mvwprintw(list_win, 0, COL_CHAN, "CH");
	mvwprintw(list_win, 0, COL_SNR, "SN");
	mvwprintw(list_win, 0, COL_RATE, "RT");
	mvwprintw(list_win, 0, COL_SOURCE, "SOURCE");
	mvwprintw(list_win, 0, COL_STA, "M");
	mvwprintw(list_win, 0, COL_BSSID, "(BSSID)");
	mvwprintw(list_win, 0, COL_ENC, "E");
	mvwprintw(list_win, 0, COL_IP, "IP/Mesh");

	/* reuse bottom line for information on other win */
	mvwprintw(list_win, win_split - 1, 0, "CH-Sig");
	if (conf.have_noise) {
		wprintw(list_win, "/No");
		nadd = 3;
	}
	wprintw(list_win, "-RT-SOURCE");
	mvwprintw(list_win, win_split - 1, 28 + nadd, "(BSSID)");
	mvwprintw(list_win, win_split - 1, 48 + nadd, "TYPE");
	mvwprintw(list_win, win_split - 1, 55 + nadd, "INFO");
	mvwprintw(list_win, win_split - 1, COLS-10, "LiveStatus");

	if (sortfunc)
		listsort(&nodes, sortfunc);

	list_for_each_entry(n, &nodes, list) {
		line++;
		if (line >= win_split - 1)
			break; /* prevent overdraw of last line */
		print_list_line(line, n);
	}

	if (essids.split_active > 0) {
		wattron(list_win, WHITEONRED);
		mvwhline(list_win, win_split - 2, 1, ' ', COLS - 2);
		print_centered(list_win, win_split - 2, COLS - 2,
			"*** IBSS SPLIT DETECTED!!! ESSID '%s' %d nodes ***",
			essids.split_essid->essid, essids.split_essid->num_nodes);
		wattroff(list_win, WHITEONRED);
	}

	wnoutrefresh(list_win);
}


void
update_dump_win(struct packet_info* p)
{
	if (!p) {
		redrawwin(dump_win);
		wnoutrefresh(dump_win);
		return;
	}

	wattron(dump_win, get_packet_type_color(p->wlan_type));

	if (p->pkt_types & PKT_TYPE_IP)
		wattron(dump_win, A_BOLD);

	wprintw(dump_win, "\n%02d ", p->phy_chan);
	wprintw(dump_win, "-%02d", -p->phy_signal);
	if (conf.have_noise)
		wprintw(dump_win, "/%02d ", -p->phy_noise);
	else
		wprintw(dump_win, " ");
	wprintw(dump_win, "%2d ", p->phy_rate/2);
	wprintw(dump_win, "%s ", ether_sprintf(p->wlan_src));
	wprintw(dump_win, "(%s) ", ether_sprintf(p->wlan_bssid));

	if (p->wlan_retry)
		wprintw(dump_win, "[r]");

	if (p->pkt_types & PKT_TYPE_OLSR) {
		wprintw(dump_win, "%-7s%s ", "OLSR", ip_sprintf(p->ip_src));
		switch (p->olsr_type) {
			case HELLO_MESSAGE: wprintw(dump_win, "HELLO"); break;
			case TC_MESSAGE: wprintw(dump_win, "TC"); break;
			case MID_MESSAGE: wprintw(dump_win, "MID");break;
			case HNA_MESSAGE: wprintw(dump_win, "HNA"); break;
			case LQ_HELLO_MESSAGE: wprintw(dump_win, "LQ_HELLO"); break;
			case LQ_TC_MESSAGE: wprintw(dump_win, "LQ_TC"); break;
			default: wprintw(dump_win, "(%d)", p->olsr_type);
		}
	}
	else if (p->pkt_types & PKT_TYPE_BATMAN) {
		wprintw(dump_win, "%-7s%s", "BAT", ip_sprintf(p->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(p->ip_dst));
	}
	else if (p->pkt_types & PKT_TYPE_MESHZ) {
		wprintw(dump_win, "%-7s%s",
			p->tcpudp_port == 9256 ? "MC_NBR" : "MC_RT",
			ip_sprintf(p->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(p->ip_dst));
	}
	else if (p->pkt_types & PKT_TYPE_UDP) {
		wprintw(dump_win, "%-7s%s", "UDP", ip_sprintf(p->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(p->ip_dst));
	}
	else if (p->pkt_types & PKT_TYPE_TCP) {
		wprintw(dump_win, "%-7s%s", "TCP", ip_sprintf(p->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(p->ip_dst));
	}
	else if (p->pkt_types & PKT_TYPE_ICMP) {
		wprintw(dump_win, "%-7s%s", "PING", ip_sprintf(p->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(p->ip_dst));
	}
	else if (p->pkt_types & PKT_TYPE_IP) {
		wprintw(dump_win, "%-7s%s", "IP", ip_sprintf(p->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(p->ip_dst));
	}
	else if (p->pkt_types & PKT_TYPE_ARP) {
		wprintw(dump_win, "%-7s", "ARP", ip_sprintf(p->ip_src));
	}
	else {
		wprintw(dump_win, "%-7s", get_packet_type_name(p->wlan_type));

		switch (p->wlan_type & IEEE80211_FCTL_FTYPE) {
		case IEEE80211_FTYPE_DATA:
			if ( p->wlan_wep == 1)
				wprintw(dump_win, "ENCRYPTED");
			break;
		case IEEE80211_FTYPE_CTL:
			switch (p->wlan_type & IEEE80211_FCTL_STYPE) {
			case IEEE80211_STYPE_CTS:
			case IEEE80211_STYPE_RTS:
			case IEEE80211_STYPE_ACK:
				wprintw(dump_win, "%s", ether_sprintf(p->wlan_dst));
				break;
			}
			break;
		case IEEE80211_FTYPE_MGMT:
			switch (p->wlan_type & IEEE80211_FCTL_STYPE) {
			case IEEE80211_STYPE_BEACON:
			case IEEE80211_STYPE_PROBE_RESP:
				wprintw(dump_win, "'%s' %llx", p->wlan_essid,
					p->wlan_tsf);
				break;
			case IEEE80211_STYPE_PROBE_REQ:
				wprintw(dump_win, "'%s'", p->wlan_essid);
				break;
			}
		}
	}
	wattroff(dump_win, A_BOLD);
}


void
update_main_win(struct packet_info *p)
{
	update_list_win();
	update_status_win(p);
	update_dump_win(p);
	wnoutrefresh(dump_win);
	if (sort_win != NULL) {
		redrawwin(sort_win);
		wnoutrefresh(sort_win);
	}
}


int
main_input(char key)
{
	if (sort_win != NULL)
		return sort_input(key);

	switch(key) {
	case 'o': case 'O':
		show_sort_win();
		return 1;
	}
	return 0;
}


void
init_display_main(void)
{
	win_split = LINES / 2 + 1;
	stat_height = LINES - win_split - 1;

	list_win = newwin(win_split, COLS, 0, 0);
	scrollok(list_win, FALSE);

	stat_win = newwin(stat_height, STAT_WIDTH, win_split, COLS - STAT_WIDTH);
	scrollok(stat_win, FALSE);

	dump_win = newwin(stat_height, COLS - STAT_WIDTH, win_split, 0);
	scrollok(dump_win, TRUE);

	ewma_init(&usen_avg, 1024, 8);
	ewma_init(&bpsn_avg, 1024, 8);
}


void
resize_display_main(void)
{
	win_split = LINES / 2 + 1;
	stat_height = LINES - win_split - 1;
	wresize(list_win, win_split, COLS);
	wresize(dump_win, stat_height, COLS - STAT_WIDTH);
	mvwin(dump_win, win_split, 0);
	wresize(stat_win, stat_height, STAT_WIDTH);
	mvwin(stat_win, win_split, COLS - STAT_WIDTH);
}
