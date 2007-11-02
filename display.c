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
#include <curses.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include "display.h"
#include "util.h"
#include "ieee80211.h"
#include "olsr_header.h"

WINDOW *dump_win = NULL;
WINDOW *list_win = NULL;
WINDOW *stat_win = NULL;
WINDOW *essid_win = NULL;
WINDOW *filter_win = NULL;
WINDOW *hist_win = NULL;

static void update_dump_win(struct packet_info* pkt);
static void update_stat_win(struct packet_info* pkt, int node_number);
static void update_list_win(void);
static void update_essid_win(void);
static void update_hist_win(void);
static void display_essid_win(void);
static void display_filter_win(void);
static void display_hist_win(void);

static int do_sort=0;

extern char* ifname;
extern unsigned char filtermac[6];
extern int do_filter;

struct node_info* sort_nodes[MAX_NODES];

struct timeval last_time;


static inline void print_centered(WINDOW* win, int line, int cols, char* str) {
	mvwprintw(win, line, cols/2 - strlen(str)/2, str);
}


void
init_display(void)
{
	initscr();
	start_color();                  /* Start the color functionality */
	keypad(stdscr, TRUE);
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();         /* echo input - in color */
	nodelay(stdscr,TRUE);
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_RED, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);
	init_pair(5, COLOR_BLUE, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_WHITE);

	/* COLOR_BLACK COLOR_RED COLOR_GREEN COLOR_YELLOW COLOR_BLUE
	COLOR_MAGENTA COLOR_CYAN COLOR_WHITE */

#define WHITE	COLOR_PAIR(1)
#define GREEN	COLOR_PAIR(2)
#define RED	COLOR_PAIR(3)
#define CYAN	COLOR_PAIR(4)
#define BLUE	COLOR_PAIR(5)
#define BLACKONWHITE	COLOR_PAIR(6)

	erase();

	wattron(stdscr, BLACKONWHITE);
	mvwhline(stdscr, LINES-1, 0, ' ', COLS);

	mvwprintw(stdscr, LINES-1, 0, "[HORST] q:Quit p:Pause s:Sort f:Filter h:History e:ESSIDs a:Stats i:Info ?:Help");
	if (arphrd == 803)
		mvwprintw(stdscr, LINES-1, COLS-14, "%s: RADIOTAP", ifname);
	else if (arphrd == 802)
		mvwprintw(stdscr, LINES-1, COLS-14, "%s: PRISM2", ifname);
	else if (arphrd == 801)
		mvwprintw(stdscr, LINES-1, COLS-14, "%s: 802.11", ifname);
	else
		mvwprintw(stdscr, LINES/2-1, COLS-14, "%s: UNSUPP", ifname);

	wattroff(stdscr, BLACKONWHITE);
	refresh();

	list_win = newwin(LINES/2+1, COLS, 0, 0);
	scrollok(list_win,FALSE);

	stat_win = newwin(LINES/2-2, 15, LINES/2+1, COLS-15);
	scrollok(stat_win,FALSE);

	dump_win = newwin(LINES/2-2, COLS-15, LINES/2+1, 0);
	scrollok(dump_win,TRUE);

	update_display(NULL,-1);
}


void update_display(struct packet_info* pkt, int node_number) {
	struct timeval the_time;
	gettimeofday( &the_time, NULL );

	/* update only every 100ms (10 frames per sec should be enough) */
	if (the_time.tv_sec == last_time.tv_sec &&
	   (the_time.tv_usec - last_time.tv_usec) < 100000 ) {
		/* just add the line to dump win so we dont loose it */
		update_dump_win(pkt);
		return;
	}

	last_time = the_time;

	if (essid_win!=NULL)
		update_essid_win();
	else if (hist_win!=NULL)
		update_hist_win();
	else {
		update_list_win();
		update_stat_win(pkt, node_number);
		update_dump_win(pkt);
	}
	/* only one redraw */
	doupdate();
}


void
finish_display(int sig)
{
	endwin();
}


void filter_input(int c)
{
	static int pos = 0;
	static char buffer[18];

	switch(c) {
		case 'q': case '\r': case KEY_ENTER:
			buffer[18] = '\0';
			convert_string_to_mac(buffer, filtermac);
			if (filtermac[0] || filtermac[1] || filtermac[2] ||
				filtermac[3] || filtermac[4] || filtermac[5])
				do_filter = 1;
			else
				do_filter = 0;
			paused = 0;
			delwin(filter_win);
			filter_win = NULL;
			pos = 0;
			update_display(NULL,-1);
			break;
		case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':case '0':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case ':':
			if (pos<18) {
				buffer[pos] = c;
				pos++;
				wechochar(filter_win, c);
			}
			break;
		case KEY_BACKSPACE:
			break;
	}
}


void
handle_user_input()
{
	int key;

	key = getch();

	if (filter_win!=NULL) {
		filter_input(key);
		return;
	}

	switch(key) {
		case ' ': case 'p': case 'P':
			paused = paused ? 0 : 1;
			break;
		case 'c': case 'C':
			no_ctrl = no_ctrl ? 0 : 1;
			break;
		case 'o': case 'O':
			olsr_only = olsr_only ? 0 : 1;
			break;
		case 'q': case 'Q':
			finish_all(0);
		case 's': case 'S':
			do_sort = do_sort ? 0 : 1;
			break;
		case 'e': case 'E':
			if (essid_win == NULL)
				display_essid_win();
			else {
				delwin(essid_win);
				essid_win = NULL;
			}
			break;
		case 'h': case 'H':
			if (hist_win == NULL)
				display_hist_win();
			else {
				delwin(hist_win);
				hist_win = NULL;
			}
			break;
		case 'f': case 'F':
			if (filter_win == NULL)
				display_filter_win();
			else {
				delwin(filter_win);
				filter_win = NULL;
			}
			return; // dont redraw
		/* not yet:
		case 'c': case 'C':
			pause = 1;
			show_channel_win();
			break;
		*/
		case KEY_RESIZE: /* xterm window resize event */
			endwin();
			init_display();
			return;
		default:
			return;
	}
	update_display(NULL,-1);
}


static void
update_stat_win(struct packet_info* pkt, int node_number)
{
	// repaint everything every time
	werase(stat_win);
	wattron(stat_win, WHITE);
	mvwvline(stat_win, 0, 0, ACS_VLINE, LINES/2);
	mvwvline(stat_win, 0, 14, ACS_VLINE, LINES/2);

	wattron(stat_win, GREEN);

	if (pkt!=NULL)
	{
		int snr = pkt->snr;
		int max_bar = LINES/2-2;
		int min;
		int max;

		snr=normalize(snr,60.0,max_bar);

		mvwvline(stat_win, 1, 2, ' ', max_bar-snr);
		mvwvline(stat_win, 1, 3, ' ', max_bar-snr);
		if (node_number>=0 && nodes[node_number].snr_max>0) {
			wattron(stat_win, A_BOLD);
			mvwprintw(stat_win, LINES/2-2,8,"/%2d/%2d",
				  nodes[node_number].snr_max, nodes[node_number].snr_min);
			wattroff(stat_win, A_BOLD);

			max=normalize(nodes[node_number].snr_max,60.0,max_bar);
			min=normalize(nodes[node_number].snr_min,60.0,max_bar);
			if (max>1)
				mvwprintw(stat_win, LINES/2-max-1, 2, "--");
		}
		wattron(stat_win, A_BOLD);
		mvwvline(stat_win, max_bar-snr+1, 2, ACS_BLOCK, snr);
		mvwvline(stat_win, max_bar-snr+1, 3, ACS_BLOCK, snr);
		wattroff(stat_win, A_BOLD);

		mvwprintw(stat_win, LINES/2-6,6,"RATE:%2d", pkt->rate);
		mvwprintw(stat_win, LINES/2-5,6,"Sig/Noi");
		mvwprintw(stat_win, LINES/2-3,6,"SN/MX/MI");
		wattron(stat_win, A_BOLD);
		mvwprintw(stat_win, LINES/2-4,6,"%03d/%03d", pkt->signal, pkt->noise);
		mvwprintw(stat_win, LINES/2-2,6,"%2d", pkt->snr);
		wattroff(stat_win, A_BOLD);
	}

	wnoutrefresh(stat_win);
}


static int
compare_nodes_snr(const void *p1, const void *p2)
{
	struct node_info* n1 = *(struct node_info**)p1;
	struct node_info* n2 = *(struct node_info**)p2;

	if (n1->last_pkt.snr > n2->last_pkt.snr)
		return -1;
	else if (n1->last_pkt.snr == n2->last_pkt.snr)
		return 0;
	else
		return 1;
}


#define COL_IP 3
#define COL_SNR 19
#define COL_RATE 28
#define COL_SOURCE 31
#define COL_STA 49
#define COL_BSSID 51
#define COL_OLSR 71
#define COL_TSF 82

static char spin[4] = {'/', '-', '\\', '|'};

static void
print_list_line(int line, struct node_info* n, time_t now)
{
	struct packet_info* p = &n->last_pkt;

	if (n->pkt_types & PKT_TYPE_OLSR)
		wattron(list_win, GREEN);
	if (n->last_seen > now - node_timeout/2)
		wattron(list_win,A_BOLD);
	else
		wattron(list_win,A_NORMAL);

	if (essids[n->essid].split>0)
		wattron(list_win,RED);

	mvwprintw(list_win, line, 1, "%c",
		spin[n->pkt_count%4]);

	mvwprintw(list_win,line,COL_SNR,"%2d/%2d/%2d",
		p->snr, n->snr_max, n->snr_min);

	if (n->wlan_mode == WLAN_MODE_AP )
		mvwprintw(list_win,line,COL_STA,"A");
	else if (n->wlan_mode == WLAN_MODE_IBSS )
		mvwprintw(list_win,line,COL_STA,"I");
	else
		mvwprintw(list_win,line,COL_STA,"S");

	mvwprintw(list_win,line,COL_RATE,"%2d", p->rate);
	mvwprintw(list_win,line,COL_SOURCE,"%s", ether_sprintf(p->wlan_src));
	mvwprintw(list_win,line,COL_BSSID,"(%s)", ether_sprintf(n->wlan_bssid));
	if (n->pkt_types & PKT_TYPE_IP)
		mvwprintw(list_win,line,COL_IP,"%s", ip_sprintf(n->ip_src));
	if (n->pkt_types & PKT_TYPE_OLSR)
		mvwprintw(list_win, line, COL_OLSR, "N:%d", n->olsr_neigh);
	if (n->pkt_types & PKT_TYPE_OLSR_LQ)
		wprintw(list_win, "L");
	if (n->pkt_types & PKT_TYPE_OLSR_GW)
		wprintw(list_win, "G");
	mvwprintw(list_win,line,COL_TSF,"%08x", n->tsfh);

	if (n->channel)
		mvwprintw(list_win, line, COL_TSF+9, "%2d", n->channel );

	wattroff(list_win,A_BOLD);
	wattroff(list_win,GREEN);
	wattroff(list_win,RED);
}


static void
update_list_win(void)
{
	int i;
	int num_nodes;
	struct node_info* n;
	int line=0;
	time_t now;
	now = time(NULL);

	werase(list_win);
	wattron(list_win, WHITE);
	box(list_win, 0 , 0);
	mvwprintw(list_win,0,COL_SNR,"SN/MX/MI");
	mvwprintw(list_win,0,COL_RATE,"RT");
	mvwprintw(list_win,0,COL_SOURCE,"SOURCE");
	mvwprintw(list_win,0,COL_STA,"T");
	mvwprintw(list_win,0,COL_BSSID,"(BSSID)");
	mvwprintw(list_win,0,COL_IP,"IP");
	mvwprintw(list_win,0,COL_OLSR,"OLSR");
	mvwprintw(list_win,0,COL_TSF,"TSF High");
	mvwprintw(list_win,0,COL_TSF+9,"CH");

	/* reuse bottom line for information on other win */
	mvwprintw(list_win, LINES/2, 1, "Si/Noi-RT-SOURCE");
	mvwprintw(list_win, LINES/2, 29, "(BSSID)");
	mvwprintw(list_win, LINES/2, 49, "TYPE");
	mvwprintw(list_win, LINES/2, 57, "INFO");
	mvwprintw(list_win, LINES/2, COLS-11, "Status");

	/* create an array of node pointers to make sorting independent */
	for (i=0; i<MAX_NODES && nodes[i].status == 1; i++)
		sort_nodes[i] = &nodes[i];

	num_nodes = i;

	if (do_sort) {
		/* sort by SNR */
		qsort(sort_nodes, num_nodes, sizeof(struct node_info*), compare_nodes_snr);
	}

	for (i=0; i<num_nodes; i++) {
		n = sort_nodes[i];
		if (n->last_seen > now - node_timeout) {
			line++;
			/* Prevent overdraw of last line */
			if (line < LINES/2)
				print_list_line(line,n,now);
		}
	}

	if (splits.count>0) {
		wattron(list_win, RED);
		mvwprintw(list_win,LINES/2-2,10," *** IBSS SPLIT DETECTED!!! ESSID ");
		wprintw(list_win,"'%s' ", essids[splits.essid[0]].essid);
		wprintw(list_win,"%d nodes *** ",
			essids[splits.essid[0]].num_nodes);
		wattroff(list_win, RED);
	}

	wnoutrefresh(list_win);
}



static void
display_essid_win()
{
	essid_win = newwin(LINES-1, 90, 0, 0);
	scrollok(essid_win,FALSE);
	update_essid_win();
}


static void
update_essid_win(void)
{
	int i, n;
	int line=1;
	struct node_info* node;

	werase(essid_win);
	wattron(essid_win, WHITE);
	box(essid_win, 0 , 0);
	print_centered(essid_win, 0, 85, " ESSIDs ");

	for (i=0; i<MAX_ESSIDS && essids[i].num_nodes>0; i++) {
		wattron(essid_win, WHITE);
		mvwprintw(essid_win, line, 2, "ESSID '%s'", essids[i].essid );
		if (essids[i].split > 0) {
			wattron(essid_win, RED);
			wprintw(essid_win, "*** SPLIT!!! ***");
		}
		else
			wattron(essid_win, GREEN);
		line++;
		for (n=0; n<essids[i].num_nodes && n<MAX_NODES; n++) {
			node = &nodes[essids[i].nodes[n]];
			mvwprintw(essid_win, line, 3, "%2d. %s %s", n+1,
				node->wlan_mode == WLAN_MODE_AP ? "AP  " : "IBSS",
				ether_sprintf(node->last_pkt.wlan_src));
			wprintw(essid_win, " BSSID (%s) ", ether_sprintf(node->wlan_bssid));
			wprintw(essid_win,"TSF %08x:%08x", node->tsfh, node->tsfl);
			wprintw(essid_win," CH %d", node->channel);
			wprintw(essid_win," %ddB", node->snr);

			line++;
		}
		line++;
	}
	wnoutrefresh(essid_win);
}


static void
display_hist_win()
{
	hist_win = newwin(LINES-1, COLS, 0, 0);
	scrollok(hist_win,FALSE);
	update_hist_win();
}


#define normalize_db(val) \
	normalize(val-20, 80.0, SIGN_POS)

#define SIGN_POS LINES-14
#define TYPE_POS SIGN_POS+1
#define RATE_POS LINES-2

static void
update_hist_win(void)
{
	int i;
	int col=COLS-2;
	int sig, noi, rat;

	if (col>MAX_HISTORY)
		col = 4+MAX_HISTORY;

	werase(hist_win);
	wattron(hist_win, WHITE);
	box(hist_win, 0 , 0);
	print_centered(hist_win, 0, COLS, " Signal/Noise/Rate History ");
	mvwhline(hist_win, SIGN_POS, 1, ACS_HLINE, col);
	mvwhline(hist_win, SIGN_POS+2, 1, ACS_HLINE, col);
	mvwvline(hist_win, 1, 4, ACS_VLINE, LINES-3);

	mvwprintw(hist_win, 1, 1, "dBm");
	mvwprintw(hist_win, normalize_db(30), 1, "-30");
	mvwprintw(hist_win, normalize_db(40), 1, "-40");
	mvwprintw(hist_win, normalize_db(50), 1, "-50");
	mvwprintw(hist_win, normalize_db(60), 1, "-60");
	mvwprintw(hist_win, normalize_db(70), 1, "-70");
	mvwprintw(hist_win, normalize_db(80), 1, "-80");
	mvwprintw(hist_win, normalize_db(90), 1, "-90");
	mvwprintw(hist_win, SIGN_POS-1, 1, "-99");

	wattron(hist_win, GREEN);
	mvwprintw(hist_win, 1, col-6, "Signal");
	wattron(hist_win, RED);
	mvwprintw(hist_win, 2, col-5, "Noise");

	wattron(hist_win, CYAN);
	mvwprintw(hist_win, TYPE_POS, 1, "TYP");
	mvwprintw(hist_win, 3, col-11, "Packet Type");

	wattron(hist_win, BLUE);
	mvwprintw(hist_win, 4, col-4, "Rate");
	mvwprintw(hist_win, RATE_POS-9, 1, "54M");
	mvwprintw(hist_win, RATE_POS-8, 1, "48M");
	mvwprintw(hist_win, RATE_POS-7, 1, "36M");
	mvwprintw(hist_win, RATE_POS-6, 1, "24M");
	mvwprintw(hist_win, RATE_POS-5, 1, "18M");
	mvwprintw(hist_win, RATE_POS-4, 1, "11M");
	mvwprintw(hist_win, RATE_POS-3, 1, " 5M");
	mvwprintw(hist_win, RATE_POS-2, 1, " 2M");
	mvwprintw(hist_win, RATE_POS-1, 1, " 1M");

	i = hist.index-1;

	while (col>4 && hist.signal[i]!=0)
	{
		sig = normalize_db(-hist.signal[i]);
		noi = normalize_db(-hist.noise[i]);

		wattron(hist_win, GREEN);
		mvwvline(hist_win, sig, col, ACS_BLOCK, SIGN_POS-sig);

		wattron(hist_win, RED);
		mvwvline(hist_win, noi, col, ACS_BLOCK, SIGN_POS-noi);

		wattron(hist_win, CYAN);
		mvwprintw(hist_win, TYPE_POS, col, "%c", \
			get_paket_type_char(hist.type[i]));

		/* make rate table smaller by joining some values */
		switch (hist.rate[i]) {
			case 54: rat = 9; break;
			case 48: rat = 8; break;
			case 36: rat = 7; break;
			case 24: rat = 6; break;
			case 18: rat = 5; break;
			case 12: case 11: case 9: rat = 4; break;
			case 6: case 5: rat = 3; break;
			case 2: rat = 2; break;
			case 1: rat = 1; break;
		}
		wattron(hist_win, BLUE);
		mvwvline(hist_win, RATE_POS-rat, col, 'x', rat);

		i--;
		col--;
		if (i < 0)
			i = MAX_HISTORY-1;
	}
	wnoutrefresh(hist_win);
}


static void
display_filter_win()
{
	paused = 1;
	filter_win = newwin(7, 25, LINES/2-2, COLS/2-15);
	box(filter_win, 0 , 0);
	mvwprintw(filter_win,0,2," Enter Filter MAC ");
	if (do_filter)
		mvwprintw(filter_win,2,2, "%s", ether_sprintf(filtermac));
	else
		mvwprintw(filter_win,2,2, "  :  :  :  :  :  ");
	wmove(filter_win,2,2);
	scrollok(filter_win,FALSE);
	wrefresh(filter_win);
}


void
update_dump_win(struct packet_info* pkt)
{
	if (!pkt) {
		wprintw(dump_win, "\nPAUSED");
		wnoutrefresh(dump_win);
		return;
	}

	wattron(dump_win, CYAN);

	if (pkt->olsr_type>0 && pkt->pkt_types & PKT_TYPE_OLSR)
		wattron(dump_win,A_BOLD);

	wprintw(dump_win,"\n%03d/%03d ", pkt->signal, pkt->noise);
	wprintw(dump_win,"%2d ", pkt->rate);
	wprintw(dump_win,"%s ", ether_sprintf(pkt->wlan_src));
	wprintw(dump_win,"(%s) ", ether_sprintf(pkt->wlan_bssid));

	if (pkt->pkt_types & PKT_TYPE_OLSR) {
		wprintw(dump_win,"OLSR    %s ", ip_sprintf(pkt->ip_src));
		switch (pkt->olsr_type) {
			case HELLO_MESSAGE: wprintw(dump_win,"HELLO"); break;
			case TC_MESSAGE: wprintw(dump_win,"TC"); break;
			case MID_MESSAGE: wprintw(dump_win,"MID");break;
			case HNA_MESSAGE: wprintw(dump_win,"HNA"); break;
			case LQ_HELLO_MESSAGE: wprintw(dump_win,"LQ_HELLO"); break;
			case LQ_TC_MESSAGE: wprintw(dump_win,"LQ_TC"); break;
			default: wprintw(dump_win,"OLSR(%d)",pkt->olsr_type);
		}
	}
	else if (pkt->pkt_types & PKT_TYPE_IP) {
		wprintw(dump_win,"IP      %s ", ip_sprintf(pkt->ip_src));
	}
	else {
		wprintw(dump_win,"%-8s", get_paket_type_name(pkt->wlan_type));

		switch (pkt->wlan_type & IEEE80211_FCTL_FTYPE) {
		case IEEE80211_FTYPE_DATA:
			break;
		case IEEE80211_FTYPE_CTL:
			switch (pkt->wlan_type & IEEE80211_FCTL_STYPE) {
			case IEEE80211_STYPE_CTS:
			case IEEE80211_STYPE_RTS:
			case IEEE80211_STYPE_ACK:
				wprintw(dump_win,"%s", ether_sprintf(pkt->wlan_dst));
				break;
			}
			break;
		case IEEE80211_FTYPE_MGMT:
			switch (current_packet.wlan_type & IEEE80211_FCTL_STYPE) {
			case IEEE80211_STYPE_BEACON:
			case IEEE80211_STYPE_PROBE_RESP:
				wprintw(dump_win,"'%s' :%08x", pkt->wlan_essid,
					*(unsigned long*)(&pkt->wlan_tsf[0]));
				break;
			}
		}
	}
	wattroff(dump_win,A_BOLD);
	wnoutrefresh(dump_win);
}


#if 0 /* not used yet */
static void
show_channel_win()
{
	char buf[255];

	WINDOW* chan_win = newwin(3, 30, LINES/2-5, COLS/2-10);
	box(chan_win, 0 , 0);
	mvwprintw(chan_win,1,2,"enter channel number: ");
	wrefresh(chan_win);

	echo();
	nodelay(stdscr,FALSE);
	getnstr(buf,255);
	mvwprintw(chan_win,1,20,"%s",buf);
	nodelay(stdscr,TRUE);
	noecho();

	wrefresh(chan_win);
	delwin(chan_win);
	paused = 0;
}
#endif
