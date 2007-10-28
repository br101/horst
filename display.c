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
#include <curses.h>
#include <string.h>
#include <sys/socket.h>

#include "display.h"
#include "ieee80211_header.h"
#include "olsr_header.h"

WINDOW *dump_win;
WINDOW *dump_win_box;
WINDOW *list_win;
WINDOW *stat_win;
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

void convert_string_to_mac(const char* string, unsigned char* mac);
static inline int normalize(int val, float max_val, int max);


static inline void print_centered(WINDOW* win, int line, char* str) {
	mvwprintw(win, line, COLS/2 - strlen(str)/2, str);
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

	/* COLOR_BLACK COLOR_RED COLOR_GREEN COLOR_YELLOW COLOR_BLUE 
	COLOR_MAGENTA COLOR_CYAN COLOR_WHITE */

#define WHITE	COLOR_PAIR(1)
#define GREEN	COLOR_PAIR(2)
#define RED	COLOR_PAIR(3)
#define CYAN	COLOR_PAIR(4)
#define BLUE	COLOR_PAIR(5)


	print_centered(stdscr, 0, "HORST - Horsts OLSR Radio Scanning Tool");
	refresh();

	list_win = newwin(LINES/2-1, COLS, 1, 0);
	scrollok(list_win,FALSE);
	wrefresh(list_win);

	stat_win = newwin(LINES/2, 15, LINES/2, COLS-15);
	scrollok(stat_win,FALSE);
	wrefresh(stat_win);

	dump_win_box = newwin(LINES/2, COLS-15, LINES/2, 0);
	scrollok(dump_win_box,FALSE);
	wrefresh(dump_win);

	dump_win = newwin(LINES/2-2, COLS-15-2, LINES/2+1, 1);
	scrollok(dump_win,TRUE);
	wrefresh(dump_win);

	update_display(NULL,-1);
}


void update_display(struct packet_info* pkt, int node_number) {
	if (essid_win!=NULL)
		update_essid_win();
	else if (hist_win!=NULL)
		update_hist_win();
	else {
		update_dump_win(pkt);
		update_list_win();
		update_stat_win(pkt, node_number);
	}
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
			break;//return; // dont redraw
		case 'h': case 'H':
			if (hist_win == NULL)
				display_hist_win();
			else {
				delwin(hist_win);
				hist_win = NULL;
			}
			break;//return; // dont redraw

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
	if (!paused)
		update_display(NULL,-1);
}


static void
update_stat_win(struct packet_info* pkt, int node_number)
{
	// repaint everything every time
	werase(stat_win);
	wattron(stat_win, WHITE);
	box(stat_win, 0 , 0);
	mvwprintw(stat_win,0,2," Status ");
	mvwprintw(stat_win,LINES/2-1,2,ifname);

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

	wrefresh(stat_win);
}


static int
compare_nodes_snr(const void *p1, const void *p2)
{
	struct node_info* n1 = (struct node_info*)p1;
	struct node_info* n2 = (struct node_info*)p2;

	if (n1->last_pkt.snr > n2->last_pkt.snr)
		return -1;
	else if (n1->last_pkt.snr == n2->last_pkt.snr)
		return 0;
	else
		return 1;
}


#define COL_IP 1
#define COL_SNR 17
#define COL_RATE 26
#define COL_SOURCE 29
#define COL_STA 47
#define COL_BSSID 49
#define COL_LQ 69
#define COL_OLSR 81
#define COL_TSF 93

static void
print_list_line(int line, int i, time_t now)
{
	struct packet_info* p = &nodes[i].last_pkt;

	if (nodes[i].pkt_types & PKT_TYPE_OLSR)
		wattron(list_win,A_UNDERLINE);
	if (nodes[i].last_seen > now - NODE_TIMEOUT/2)
		wattron(list_win,A_BOLD);
	else
		wattron(list_win,A_NORMAL);

	if (essids[nodes[i].essid].split>0)
		wattron(list_win,RED);

	mvwprintw(list_win,line,COL_SNR,"%2d/%2d/%2d",
		  p->snr, nodes[i].snr_max, nodes[i].snr_min);

	if (nodes[i].wlan_mode == WLAN_MODE_AP )
		mvwprintw(list_win,line,COL_STA,"A");
	else if (nodes[i].wlan_mode == WLAN_MODE_IBSS )
		mvwprintw(list_win,line,COL_STA,"I");
	else
		mvwprintw(list_win,line,COL_STA,"S");

	mvwprintw(list_win,line,COL_RATE,"%2d", p->rate);
	mvwprintw(list_win,line,COL_SOURCE,"%s", ether_sprintf(p->wlan_src));
	mvwprintw(list_win,line,COL_BSSID,"(%s)", ether_sprintf(nodes[i].wlan_bssid));
	if (nodes[i].pkt_types & PKT_TYPE_IP)
		mvwprintw(list_win,line,COL_IP,"%s", ip_sprintf(nodes[i].ip_src));
	if (nodes[i].pkt_types & PKT_TYPE_OLSR_LQ)
		mvwprintw(list_win,line,COL_LQ,"LQ");
	if (nodes[i].pkt_types & PKT_TYPE_OLSR_GW)
		mvwprintw(list_win,line,COL_LQ+3,"GW");
	if (nodes[i].pkt_types & PKT_TYPE_OLSR)
		mvwprintw(list_win,line,COL_LQ+6,"N:%d", nodes[i].olsr_neigh);
	mvwprintw(list_win,line,COL_OLSR,"%d/%d", nodes[i].olsr_count, nodes[i].pkt_count);
	mvwprintw(list_win,line,COL_TSF,"%08x", nodes[i].tsfh);

	wattroff(list_win,A_BOLD);
	wattroff(list_win,A_UNDERLINE);
	wattroff(list_win,RED);
}


static void
update_list_win(void)
{
	int i;
	int line=0;
	time_t now;
	now = time(NULL);

	// repaint everything every time
	werase(list_win);
	wattron(list_win, WHITE);
	box(list_win, 0 , 0);
	mvwprintw(list_win,0,COL_SNR,"SN/MX/MI");
	mvwprintw(list_win,0,COL_RATE,"RT");
	mvwprintw(list_win,0,COL_SOURCE,"SOURCE");
	mvwprintw(list_win,0,COL_BSSID,"(BSSID)");
	mvwprintw(list_win,0,COL_IP,"IP");
	mvwprintw(list_win,0,COL_LQ,"LQ GW NEIGH");
	mvwprintw(list_win,0,COL_OLSR,"OLSR/COUNT");
	mvwprintw(list_win,0,COL_TSF,"TSF(High)");

	if (do_sort) {
		/* sort by SNR */
		qsort(nodes, MAX_NODES, sizeof(struct node_info), compare_nodes_snr);
	}

	for (i=0; i<MAX_NODES; i++) {
		if (nodes[i].status == 1
		    && nodes[i].last_seen > now - NODE_TIMEOUT) {
			line++;
			/* Prevent overdraw of last line */
			if (line < LINES/2-2)
				print_list_line(line,i,now);
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

	wrefresh(list_win);
}



static void
display_essid_win()
{
	essid_win = newwin(LINES-3, 80, 2, 2);
	scrollok(essid_win,FALSE);
	update_essid_win();
}


static void
update_essid_win(void)
{
	int i, n;
	int line=2;
	struct node_info* node;

	werase(essid_win);
	wattron(essid_win, WHITE);
	box(essid_win, 0 , 0);
	print_centered(essid_win, 0, " ESSIDs ");

	for (i=0; i<MAX_ESSIDS && essids[i].num_nodes>0; i++) {
		if (essids[i].split>0)
			wattron(essid_win, RED);
		else
			wattron(essid_win, GREEN);
		mvwprintw(essid_win, line, 2, "ESSID '%s' BSSID ", essids[i].essid );
		if (essids[i].split==0)
			wprintw(essid_win, "(%s)", ether_sprintf(nodes[essids[i].nodes[0]].wlan_bssid));
		else
			wprintw(essid_win, "*** SPLIT!!! ***");
		line++;
		for (n=0; n<essids[i].num_nodes && n<MAX_NODES; n++) {
			node = &nodes[essids[i].nodes[n]];
			if (node->wlan_mode == WLAN_MODE_AP )
				mvwprintw(essid_win,line,4, " AP");
			else if (node->wlan_mode == WLAN_MODE_IBSS )
				mvwprintw(essid_win,line,4," IBSS");

			mvwprintw(essid_win, line, 9, "%2d. %s", n+1,
				ether_sprintf(node->last_pkt.wlan_src));
			wprintw(essid_win, " bssid (%s) ", ether_sprintf(node->wlan_bssid));
			wprintw(essid_win,"TSF %08x:%08x", node->tsfh, node->tsfl);
			line++;
		}
		line++;
	}
	wrefresh(essid_win);
}


static void
display_hist_win()
{
	hist_win = newwin(LINES-2, COLS-4, 2, 2);
	scrollok(hist_win,FALSE);
	update_hist_win();
}


static inline int
normalize(int oval, float max_val, int max) {
	int val;
	val=(oval/max_val)*max;
	if (val>max)
		val=max; /* cap if still bigger */
	if (val==0 && oval > 0)
		val=1;
	return val;
}

#define normalize_db(val) \
	normalize(val, 99.0, SIGN_POS)

#define SIGN_POS LINES-20
#define TYPE_POS SIGN_POS+1
#define RATE_POS LINES-5

static void
update_hist_win(void)
{
	int i;
	int col=COLS-6;
	int sig, noi, rat;

	if (col>MAX_HISTORY)
		col = 4+MAX_HISTORY;

	werase(hist_win);
	wattron(hist_win, WHITE);
	box(hist_win, 0 , 0);
	print_centered(hist_win, 0, " Signal/Noise HISTORY ");
	mvwhline(hist_win, SIGN_POS, 1, ACS_HLINE, col);
	mvwvline(hist_win, 1, 4, ACS_VLINE, SIGN_POS+1);
	mvwhline(hist_win, RATE_POS, 1, ACS_HLINE, col);
	mvwvline(hist_win, RATE_POS-9, 4, ACS_VLINE, 10);

	mvwprintw(hist_win, 1, 1, "dBm");
	mvwprintw(hist_win, normalize_db(10), 1, "-10");
	mvwprintw(hist_win, normalize_db(20), 1, "-20");
	mvwprintw(hist_win, normalize_db(30), 1, "-30");
	mvwprintw(hist_win, normalize_db(40), 1, "-40");
	mvwprintw(hist_win, normalize_db(50), 1, "-50");
	mvwprintw(hist_win, normalize_db(60), 1, "-60");
	mvwprintw(hist_win, normalize_db(70), 1, "-70");
	mvwprintw(hist_win, normalize_db(80), 1, "-80");
	mvwprintw(hist_win, normalize_db(90), 1, "-90");
	mvwprintw(hist_win, SIGN_POS-1, 1, "-99");

	wattron(hist_win, GREEN);
	mvwprintw(hist_win, 2, col-6, "Signal");
	wattron(hist_win, RED);
	mvwprintw(hist_win, 3, col-5, "Noise");

	wattron(hist_win, CYAN);
	mvwprintw(hist_win, TYPE_POS, 1, "TYP");

	wattron(hist_win, BLUE);
	mvwprintw(hist_win, RATE_POS-9, 1, "54M");
	mvwprintw(hist_win, RATE_POS-8, 1, "46M");
	mvwprintw(hist_win, RATE_POS-7, 1, "36M");
	mvwprintw(hist_win, RATE_POS-6, 1, "30M");
	mvwprintw(hist_win, RATE_POS-5, 1, "24M");
	mvwprintw(hist_win, RATE_POS-4, 1, "11M");
	mvwprintw(hist_win, RATE_POS-3, 1, " 5M");
	mvwprintw(hist_win, RATE_POS-2, 1, " 2M");
	mvwprintw(hist_win, RATE_POS-1, 1, " 1M");

	i = hist.index-1;

	/* !!! assume history is always bigger than cols */

	while (col>4 && hist.signal[i]!=0)
	{
		sig = normalize_db(-hist.signal[i]);
		noi = normalize_db(-hist.noise[i]);
		rat = normalize(hist.rate[i], 54.0, 10);

		wattron(hist_win, GREEN);
		mvwvline(hist_win, sig, col, ACS_BLOCK, SIGN_POS-sig);

		wattron(hist_win, RED);
		mvwvline(hist_win, noi, col, ACS_BLOCK, SIGN_POS-noi);

		wattron(hist_win, BLUE);
		mvwvline(hist_win, RATE_POS-rat, col, 'x', rat);

		wattron(hist_win, CYAN);
		if (hist.type[i] == WLAN_FC_TYPE_MGMT)
			mvwprintw(hist_win, TYPE_POS, col, "M");
		else if (hist.type[i] == WLAN_FC_TYPE_CTRL)
			mvwprintw(hist_win, TYPE_POS, col, "C");
		else if (hist.type[i] == WLAN_FC_TYPE_DATA)
			mvwprintw(hist_win, TYPE_POS, col, "D");
		else
			mvwprintw(hist_win, TYPE_POS, col, "?");

		i--;
		col--;
		if (i < 0) 
			i = MAX_HISTORY-1;
	}
	wrefresh(hist_win);
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
	wattron(stat_win, WHITE);
	box(dump_win_box, 0 , 0);
	mvwprintw(dump_win_box,0,1,"Sig/Noi");
	mvwprintw(dump_win_box,0,9,"RT");
	mvwprintw(dump_win_box,0,12,"SOURCE");
	mvwprintw(dump_win_box,0,30,"(BSSID)");
	mvwprintw(dump_win_box,0,50,"TYPE");
	mvwprintw(dump_win_box,0,57,"INFO");
	mvwprintw(dump_win_box,LINES/2-1,2,"V" PACKAGE_VERSION " (built: " PACKAGE_BUILDDATE ")");
	if (arphrd == 803)
		mvwprintw(dump_win_box,LINES/2-1,COLS-25,"RADIOTAP");
	else if (arphrd == 802)
		mvwprintw(dump_win_box,LINES/2-1,COLS-25,"PRISM2");
	else if (arphrd == 801)
		mvwprintw(dump_win_box,LINES/2-1,COLS-26,"IEEE802.11");
	else
		mvwprintw(dump_win_box,LINES/2-1,COLS-25,"UNSUPP");

	if (!pkt) {
		wrefresh(dump_win_box);
		wrefresh(dump_win);
		return;
	}

	wattron(dump_win, CYAN);

	if (pkt->olsr_type>0 && pkt->pkt_types & PKT_TYPE_OLSR)
		wattron(dump_win,A_BOLD);

	wprintw(dump_win,"%03d/%03d ", pkt->signal, pkt->noise);
	wprintw(dump_win,"%2d ", pkt->rate);
	wprintw(dump_win,"%s ", ether_sprintf(pkt->wlan_src));
	wprintw(dump_win,"(%s) ", ether_sprintf(pkt->wlan_bssid));

	if (pkt->pkt_types & PKT_TYPE_OLSR) {
		wprintw(dump_win,"OLSR   %s ", ip_sprintf(pkt->ip_src));
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
		wprintw(dump_win,"IP     %s ", ip_sprintf(pkt->ip_src));
	}
	else if (WLAN_FC_TYPE_MGMT == pkt->wlan_type) {
		switch(pkt->wlan_stype) {
			case WLAN_FC_STYPE_ASSOC_REQ:
				wprintw(dump_win,"ASSOC_REQ");
				break;
			case WLAN_FC_STYPE_ASSOC_RESP:
				wprintw(dump_win,"ASSOC_RESP");
				break;
			case WLAN_FC_STYPE_REASSOC_REQ:
				wprintw(dump_win,"REASSOC_REQ");
				break;
			case WLAN_FC_STYPE_REASSOC_RESP:
				wprintw(dump_win,"REASSOC_RESP");
				break;
			case WLAN_FC_STYPE_PROBE_REQ:
				wprintw(dump_win,"PROBE_REQ");
				break;
			case WLAN_FC_STYPE_PROBE_RESP:
				wprintw(dump_win,"PROBE_RESP");
				break;
			case WLAN_FC_STYPE_BEACON:
				wprintw(dump_win,"BEACON '%s' %08x:%08x", pkt->wlan_essid,
					*(unsigned long*)(&pkt->wlan_tsf[4]),
					*(unsigned long*)(&pkt->wlan_tsf[0]));
				break;
			case WLAN_FC_STYPE_ATIM:
				wprintw(dump_win,"ATIM");
				break;
			case WLAN_FC_STYPE_DISASSOC:
				wprintw(dump_win,"DISASSOC");
				break;
			case WLAN_FC_STYPE_AUTH:
				wprintw(dump_win,"AUTH");
				break;
			case WLAN_FC_STYPE_DEAUTH:
				wprintw(dump_win,"DEAUTH");
				break;
			default:
				wprintw(dump_win,"MGMT(0x%02x)",pkt->wlan_stype);
				break;
		}
	}
	else if (WLAN_FC_TYPE_CTRL == pkt->wlan_type) {
		switch(pkt->wlan_stype) {
			case WLAN_FC_STYPE_PSPOLL:
				wprintw(dump_win,"PSPOLL");
				break;
			case WLAN_FC_STYPE_RTS:
				wprintw(dump_win,"RTS");
				break;
			case WLAN_FC_STYPE_CTS:
				wprintw(dump_win,"CTS");
				break;
			case WLAN_FC_STYPE_ACK:
				wprintw(dump_win,"ACK");
				break;
			case WLAN_FC_STYPE_CFEND:
				wprintw(dump_win,"CFEND");
				break;
			case WLAN_FC_STYPE_CFENDACK:
				wprintw(dump_win,"CFENDACK");
				break;
			default:
				wprintw(dump_win,"CTRL(0x%02x)",pkt->wlan_stype);
				break;
		}
	}
	else if (WLAN_FC_TYPE_DATA == pkt->wlan_type) {
		switch(pkt->wlan_stype) {
			case WLAN_FC_STYPE_DATA:
				wprintw(dump_win,"DATA");
				break;
			case WLAN_FC_STYPE_DATA_CFACK:
				wprintw(dump_win,"DATA_CFACK");
				break;
			case WLAN_FC_STYPE_DATA_CFPOLL:
				wprintw(dump_win,"DATA_CFPOLL");
				break;
			case WLAN_FC_STYPE_DATA_CFACKPOLL:
				wprintw(dump_win,"DATA_CFACKPOLL");
				break;
			case WLAN_FC_STYPE_NULLFUNC:
				wprintw(dump_win,"NULLFUNC");
				break;
			case WLAN_FC_STYPE_CFACK:
				wprintw(dump_win,"CFACK");
				break;
			case WLAN_FC_STYPE_CFPOLL:
				wprintw(dump_win,"CFPOLL");
				break;
			case WLAN_FC_STYPE_CFACKPOLL:
				wprintw(dump_win,"CFACKPOLL");
				break;
			default:
				wprintw(dump_win,"DATA(0x%02x)",pkt->wlan_stype);
				break;
		}
	}
	else {
		wprintw(dump_win,"UNK(%x,%x)", pkt->wlan_stype, pkt->wlan_type);
	}

	wprintw(dump_win,"\n");
	wattroff(dump_win,A_BOLD);
	wrefresh(dump_win);
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

	wrefresh(list_win);
}
#endif


void
dump_packet(const unsigned char* buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if ((i%2) == 0)
			DEBUG(" ");
		if ((i%16) == 0)
			DEBUG("\n");
		DEBUG("%02x", buf[i]);
	}
	DEBUG("\n");
}


const char*
ether_sprintf(const unsigned char *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}


const char*
ip_sprintf(const unsigned int ip)
{
	static char ipbuf[18];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d",
		cip[0], cip[1], cip[2], cip[3]);
	return ipbuf;
}
