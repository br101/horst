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
#include <ctype.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include "display.h"
#include "util.h"
#include "ieee80211.h"
#include "olsr_header.h"

static void show_window(char which);
static void display_filter_win(void);

static void update_dump_win(struct packet_info* pkt);
static void update_stat_win(struct packet_info* pkt, int node_number);
static void update_list_win(void);
static void update_show_win(void);
static void update_essid_win(void);
static void update_hist_win(void);
static void update_statistics_win(void);
static void update_help_win(void);
static void update_detail_win(void);

static WINDOW *dump_win = NULL;
static WINDOW *list_win = NULL;
static WINDOW *stat_win = NULL;
static WINDOW *filter_win = NULL;
static WINDOW *show_win = NULL;
static char show_win_current;
static int do_sort=0;
static struct node_info* sort_nodes[MAX_NODES];
static struct timeval last_time;

extern struct config conf;
extern struct statistics stats;


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

	mvwprintw(stdscr, LINES-1, 0, "[HORST] q:Quit p:Pause s:Sort f:Filter h:History e:ESSIDs a:Stats d:Details ?:Help");
	if (conf.arphrd == 803)
		mvwprintw(stdscr, LINES-1, COLS-14, "%s: RADIOTAP", conf.ifname);
	else if (conf.arphrd == 802)
		mvwprintw(stdscr, LINES-1, COLS-14, "%s: PRISM2", conf.ifname);
	else if (conf.arphrd == 801)
		mvwprintw(stdscr, LINES-1, COLS-14, "%s: 802.11", conf.ifname);
	else
		mvwprintw(stdscr, LINES/2-1, COLS-14, "%s: UNSUPP", conf.ifname);

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
			convert_string_to_mac(buffer, conf.filtermac);
			if (conf.filtermac[0] || conf.filtermac[1] || conf.filtermac[2] ||
				conf.filtermac[3] || conf.filtermac[4] || conf.filtermac[5])
				conf.do_filter = 1;
			else
				conf.do_filter = 0;
			conf.paused = 0;
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
			conf.paused = conf.paused ? 0 : 1;
			break;
		case 'q': case 'Q':
			finish_all(0);
		case 's': case 'S':
			do_sort = do_sort ? 0 : 1;
			break;
		case 'e': case 'E':
		case 'h': case 'H':
		case 'd': case 'D':
		case 'a': case 'A':
		case '?':
			show_window(tolower(key));
			break;
		case 'f': case 'F':
			if (filter_win == NULL)
				display_filter_win();
			else {
				delwin(filter_win);
				filter_win = NULL;
			}
			break;
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
show_window(char which)
{
	if (show_win != NULL && show_win_current == which) {
		delwin(show_win);
		show_win = NULL;
		show_win_current = 0;
		return;
	}
	if (show_win == NULL) {
		show_win = newwin(LINES-1, COLS, 0, 0);
		scrollok(show_win, FALSE);
	}
	show_win_current = which;
	update_show_win();
}


static void
display_filter_win()
{
	//char buf[255];
	conf.paused = 1;
	filter_win = newwin(7, 25, LINES/2-2, COLS/2-15);
	box(filter_win, 0 , 0);
	mvwprintw(filter_win,0,2," Enter Filter MAC ");
	if (conf.do_filter)
		mvwprintw(filter_win,2,2, "%s", ether_sprintf(conf.filtermac));
	else
		mvwprintw(filter_win,2,2, "  :  :  :  :  :  ");
	wmove(filter_win,2,2);

	scrollok(filter_win,FALSE);

#if 0
	echo();
	nodelay(filter_win,FALSE);
	getnstr(buf,255);
	mvwprintw(filter_win,1,20,"%s",buf);
	nodelay(filter_win,TRUE);
	noecho();
#endif
	wrefresh(filter_win);
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

	if (show_win != NULL)
		update_show_win();
	else {
		update_list_win();
		update_stat_win(pkt, node_number);
		update_dump_win(pkt);
	}
	/* only one redraw */
	doupdate();
}


static void
update_show_win() {
	if (show_win_current == 'e')
		update_essid_win();
	else if (show_win_current == 'h')
		update_hist_win();
	else if (show_win_current == 'a')
		update_statistics_win();
	else if (show_win_current == '?')
		update_help_win();
	else if (show_win_current == 'd')
		update_detail_win();
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
	if (n->last_seen > now - conf.node_timeout/2)
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
	mvwprintw(list_win, LINES/2, 56, "INFO");
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
		if (n->last_seen > now - conf.node_timeout) {
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
update_essid_win(void)
{
	int i, n;
	int line=1;
	struct node_info* node;

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " ESSIDs ");

	for (i=0; i<MAX_ESSIDS && essids[i].num_nodes>0; i++) {
		wattron(show_win, WHITE);
		mvwprintw(show_win, line, 2, "ESSID '%s'", essids[i].essid );
		if (essids[i].split > 0) {
			wattron(show_win, RED);
			wprintw(show_win, " *** SPLIT ***");
		}
		else
			wattron(show_win, GREEN);
		line++;
		for (n=0; n<essids[i].num_nodes && n<MAX_NODES; n++) {
			node = &nodes[essids[i].nodes[n]];
			mvwprintw(show_win, line, 3, "%2d. %s %s", n+1,
				node->wlan_mode == WLAN_MODE_AP ? "AP  " : "IBSS",
				ether_sprintf(node->last_pkt.wlan_src));
			wprintw(show_win, " BSSID (%s) ", ether_sprintf(node->wlan_bssid));
			wprintw(show_win,"TSF %08x:%08x", node->tsfh, node->tsfl);
			wprintw(show_win," CH %d", node->channel);
			wprintw(show_win," %ddB", node->snr);

			line++;
		}
		line++;
	}
	wnoutrefresh(show_win);
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

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Signal/Noise/Rate History ");
	mvwhline(show_win, SIGN_POS, 1, ACS_HLINE, col);
	mvwhline(show_win, SIGN_POS+2, 1, ACS_HLINE, col);
	mvwvline(show_win, 1, 4, ACS_VLINE, LINES-3);

	mvwprintw(show_win, 1, 1, "dBm");
	mvwprintw(show_win, normalize_db(30), 1, "-30");
	mvwprintw(show_win, normalize_db(40), 1, "-40");
	mvwprintw(show_win, normalize_db(50), 1, "-50");
	mvwprintw(show_win, normalize_db(60), 1, "-60");
	mvwprintw(show_win, normalize_db(70), 1, "-70");
	mvwprintw(show_win, normalize_db(80), 1, "-80");
	mvwprintw(show_win, normalize_db(90), 1, "-90");
	mvwprintw(show_win, SIGN_POS-1, 1, "-99");

	wattron(show_win, GREEN);
	mvwprintw(show_win, 1, col-6, "Signal");
	wattron(show_win, RED);
	mvwprintw(show_win, 2, col-5, "Noise");

	wattron(show_win, CYAN);
	mvwprintw(show_win, TYPE_POS, 1, "TYP");
	mvwprintw(show_win, 3, col-11, "Packet Type");

	wattron(show_win, BLUE);
	mvwprintw(show_win, 4, col-4, "Rate");
	mvwprintw(show_win, RATE_POS-9, 1, "54M");
	mvwprintw(show_win, RATE_POS-8, 1, "48M");
	mvwprintw(show_win, RATE_POS-7, 1, "36M");
	mvwprintw(show_win, RATE_POS-6, 1, "24M");
	mvwprintw(show_win, RATE_POS-5, 1, "18M");
	mvwprintw(show_win, RATE_POS-4, 1, "11M");
	mvwprintw(show_win, RATE_POS-3, 1, " 5M");
	mvwprintw(show_win, RATE_POS-2, 1, " 2M");
	mvwprintw(show_win, RATE_POS-1, 1, " 1M");

	i = hist.index-1;

	while (col>4 && hist.signal[i]!=0)
	{
		sig = normalize_db(-hist.signal[i]);
		noi = normalize_db(-hist.noise[i]);

		wattron(show_win, GREEN);
		mvwvline(show_win, sig, col, ACS_BLOCK, SIGN_POS-sig);

		wattron(show_win, RED);
		mvwvline(show_win, noi, col, ACS_BLOCK, SIGN_POS-noi);

		wattron(show_win, CYAN);
		mvwprintw(show_win, TYPE_POS, col, "%c", \
			get_packet_type_char(hist.type[i]));

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
		wattron(show_win, BLUE);
		mvwvline(show_win, RATE_POS-rat, col, 'x', rat);

		i--;
		col--;
		if (i < 0)
			i = MAX_HISTORY-1;
	}
	wnoutrefresh(show_win);
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
		wprintw(dump_win,"%-7s%s", "OLSR", ip_sprintf(pkt->ip_src));
		switch (pkt->olsr_type) {
			case HELLO_MESSAGE: wprintw(dump_win,"HELLO"); break;
			case TC_MESSAGE: wprintw(dump_win,"TC"); break;
			case MID_MESSAGE: wprintw(dump_win,"MID");break;
			case HNA_MESSAGE: wprintw(dump_win,"HNA"); break;
			case LQ_HELLO_MESSAGE: wprintw(dump_win,"LQ_HELLO"); break;
			case LQ_TC_MESSAGE: wprintw(dump_win,"LQ_TC"); break;
			default: wprintw(dump_win,"(%d)",pkt->olsr_type);
		}
	}
	else if (pkt->pkt_types & PKT_TYPE_IP) {
		wprintw(dump_win,"%-7s%s ", "IP", ip_sprintf(pkt->ip_src));
	}
	else {
		wprintw(dump_win,"%-7s", get_packet_type_name(pkt->wlan_type));

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


#define STAT_PACK_POS 11
#define STAT_BYTE_POS STAT_PACK_POS+9
#define STAT_BPP_POS STAT_BYTE_POS+9
#define STAT_PP_POS STAT_BPP_POS+6
#define STAT_BP_POS STAT_PP_POS+6
#define STAT_AIR_POS STAT_BP_POS+6
#define STAT_AIRG_POS STAT_AIR_POS+6

static void
update_statistics_win(void)
{
	int i;
	int line;
	float airtime;

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Packet Statistics ");

	if (stats.packets == 0) {
		wnoutrefresh(show_win);
		return; /* avoid floating point exceptions */
	}

	mvwprintw(show_win, 2, 2, "Packets: %d", stats.packets );
	mvwprintw(show_win, 3, 2, "Bytes:   %d", stats.bytes );
	mvwprintw(show_win, 4, 2, "Average: ~%d B/Pkt", stats.bytes/stats.packets);

	line = 6;
	mvwprintw(show_win, line, STAT_PACK_POS, " Packets");
	mvwprintw(show_win, line, STAT_BYTE_POS, "   Bytes");
	mvwprintw(show_win, line, STAT_BPP_POS, "~B/P");
	mvwprintw(show_win, line, STAT_PP_POS, "Pkts%%");
	mvwprintw(show_win, line, STAT_BP_POS, "Byte%%");
	wattron(show_win, A_BOLD);
	mvwprintw(show_win, line, STAT_AIR_POS, "\"AirTime%%\"");
	mvwprintw(show_win, line++, 2, "RATE");
	wattroff(show_win, A_BOLD);
	mvwhline(show_win, line++, 2, '-', COLS-4);
	for (i=1; i<MAX_RATES; i++) {
		if (stats.packets_per_rate[i] > 0) {
			wattron(show_win, A_BOLD);
			mvwprintw(show_win, line, 4, "%2dM", i);
			wattroff(show_win, A_BOLD);
			mvwprintw(show_win, line, STAT_PACK_POS, "%8d", stats.packets_per_rate[i]);
			mvwprintw(show_win, line, STAT_BYTE_POS, "%8d", stats.bytes_per_rate[i]);
			mvwprintw(show_win, line, STAT_BPP_POS, "%4d",
				stats.bytes_per_rate[i]/stats.packets_per_rate[i]);
			mvwprintw(show_win, line, STAT_PP_POS, "%2.1f",
				(stats.packets_per_rate[i]*1.0/stats.packets)*100);
			mvwprintw(show_win, line, STAT_BP_POS, "%2.1f",
				(stats.bytes_per_rate[i]*1.0/stats.bytes)*100);
			wattron(show_win, A_BOLD);
			airtime = ((stats.bytes_per_rate[i]*1.0/stats.bytes)*100)/i;
			mvwprintw(show_win, line, STAT_AIR_POS, "%2.1f", airtime);
			mvwhline(show_win, line, STAT_AIRG_POS, '*', fnormalize(airtime, 100.0, COLS-55-2));
			wattroff(show_win, A_BOLD);
			line++;
		}
	}

	line++;
	mvwprintw(show_win, line, STAT_PACK_POS, " Packets");
	mvwprintw(show_win, line, STAT_BYTE_POS, "   Bytes");
	mvwprintw(show_win, line, STAT_BPP_POS, "~B/P");
	mvwprintw(show_win, line, STAT_PP_POS, "Pkts%%");
	mvwprintw(show_win, line, STAT_BP_POS, "Byte%%");
	wattron(show_win, A_BOLD);
	mvwprintw(show_win, line, STAT_AIR_POS, "\"AirTime%%\"");
	mvwprintw(show_win, line++, 2, "TYPE");
	wattroff(show_win, A_BOLD);
	mvwhline(show_win, line++, 2, '-', COLS-4);
	for (i=0; i<MAX_FSTYPE; i++) {
		if (stats.packets_per_type[i] > 0) {
			wattron(show_win, A_BOLD);
			mvwprintw(show_win, line, 4, "%s", get_packet_type_name(i));
			wattroff(show_win, A_BOLD);
			mvwprintw(show_win, line, STAT_PACK_POS, "%8d", stats.packets_per_type[i]);
			mvwprintw(show_win, line, STAT_BYTE_POS, "%8d", stats.bytes_per_type[i]);
			mvwprintw(show_win, line, STAT_BPP_POS, "%4d",
				stats.bytes_per_type[i]/stats.packets_per_type[i]);
			mvwprintw(show_win, line, STAT_PP_POS, "%2.1f",
				(stats.packets_per_type[i]*1.0/stats.packets)*100);
			mvwprintw(show_win, line, STAT_BP_POS, "%2.1f",
				(stats.bytes_per_type[i]*1.0/stats.bytes)*100);
			wattron(show_win, A_BOLD);
			airtime = (stats.airtime_per_type[i]*1.0/stats.airtimes)*100;
			mvwprintw(show_win, line, STAT_AIR_POS, "%2.1f", airtime);
			mvwhline(show_win, line, STAT_AIRG_POS, '*', fnormalize(airtime, 100.0, COLS-55-2));
			wattroff(show_win, A_BOLD);
			line++;
		}
	}

	wnoutrefresh(show_win);
}


static void
update_help_win(void)
{
	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Help ");
	print_centered(show_win, 2, COLS, "HORST - Horsts OLSR Radio Scanning Tool");
	print_centered(show_win, 3, COLS, "Version " VERSION " (build date " BUILDDATE ")");

	print_centered(show_win, 5, COLS, "(C) 2005-2007 Bruno Randolf");

	print_centered(show_win, 6, COLS, "Licensed under the GPL");

	wnoutrefresh(show_win);
}


static void
update_detail_win(void)
{
	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Detailed Node List ");


	wnoutrefresh(show_win);
}
