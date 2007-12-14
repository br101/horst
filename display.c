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
static void show_sort_win(void);

static void update_filter_win(void);

static void update_dump_win(struct packet_info* pkt);
static void update_status_win(struct packet_info* pkt, int node_number);
static void update_list_win(void);
static void update_show_win(void);
static void update_essid_win(void);
static void update_hist_win(void);
static void update_statistics_win(void);
static void update_help_win(void);
static void update_detail_win(void);
static void update_mini_status(void);

static WINDOW *dump_win = NULL;
static WINDOW *list_win = NULL;
static WINDOW *stat_win = NULL;
static WINDOW *filter_win = NULL;
static WINDOW *show_win = NULL;
static WINDOW *small_win = NULL;

static char show_win_current;
static int do_sort = 'n';
static struct node_info* sort_nodes[MAX_NODES];

static struct timeval last_time;
static struct timeval the_time;

extern struct config conf;
extern struct statistics stats;


static inline int
bytes_per_second(unsigned int bytes) {
	static unsigned long last_bytes;
	static struct timeval last_bps;
	static int bps;
	/* reacalculate only every second or more */
	if (the_time.tv_sec > last_bps.tv_sec) {
		bps = (1.0*(bytes - last_bytes)) / (int)(the_time.tv_sec - last_bps.tv_sec);
		last_bps.tv_sec = the_time.tv_sec;
		last_bytes = bytes;
	}
	return bps;
}

static inline int
duration_per_second(unsigned int duration) {
	static unsigned long last_dur;
	static struct timeval last;
	static int dps;
	/* reacalculate only every second or more */
	if (the_time.tv_sec > last.tv_sec) {
		dps = (1.0*(duration - last_dur)) / (int)(the_time.tv_sec - last.tv_sec);
		last.tv_sec = the_time.tv_sec;
		last_dur = duration;
	}
	return dps;
}

static inline void
print_centered(WINDOW* win, int line, int cols, char* str) {
	mvwprintw(win, line, cols / 2 - strlen(str) / 2, str);
}


void
init_display(void)
{
	initscr();
	start_color();                  /* Start the color functionality */
	keypad(stdscr, TRUE);
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();
	nodelay(stdscr,TRUE);
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_RED, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);
	init_pair(5, COLOR_BLUE, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_WHITE);
	init_pair(7, COLOR_MAGENTA, COLOR_BLACK);

	init_pair(8, COLOR_GREEN, COLOR_GREEN);
	init_pair(9, COLOR_RED, COLOR_RED);
	init_pair(10, COLOR_BLUE, COLOR_BLUE);
	init_pair(11, COLOR_CYAN, COLOR_CYAN);
	init_pair(12, COLOR_YELLOW, COLOR_BLACK);
	init_pair(13, COLOR_YELLOW, COLOR_YELLOW);

	/* COLOR_BLACK COLOR_RED COLOR_GREEN COLOR_YELLOW COLOR_BLUE
	COLOR_MAGENTA COLOR_CYAN COLOR_WHITE */

#define WHITE		COLOR_PAIR(1)
#define GREEN		COLOR_PAIR(2)
#define RED		COLOR_PAIR(3)
#define CYAN		COLOR_PAIR(4)
#define BLUE		COLOR_PAIR(5)
#define BLACKONWHITE	COLOR_PAIR(6)
#define MAGENTA		COLOR_PAIR(7)
#define ALLGREEN	COLOR_PAIR(8)
#define ALLRED		COLOR_PAIR(9)
#define ALLBLUE		COLOR_PAIR(10)
#define ALLCYAN		COLOR_PAIR(11)
#define YELLOW		COLOR_PAIR(12)
#define ALLYELLOW	COLOR_PAIR(13)

	erase();

	wattron(stdscr, BLACKONWHITE);
	mvwhline(stdscr, LINES-1, 0, ' ', COLS);

	mvwprintw(stdscr, LINES-1, 0, "[HORST] ");
#define KEYMARK A_UNDERLINE
	attron(KEYMARK); printw("Q"); attroff(KEYMARK); printw("uit ");
	attron(KEYMARK); printw("P"); attroff(KEYMARK); printw("ause ");
	attron(KEYMARK); printw("S"); attroff(KEYMARK); printw("ort ");
	attron(KEYMARK); printw("F"); attroff(KEYMARK); printw("ilter ");
	attron(KEYMARK); printw("H"); attroff(KEYMARK); printw("istory ");
	attron(KEYMARK); printw("E"); attroff(KEYMARK); printw("SSIDs St");
	attron(KEYMARK); printw("a"); attroff(KEYMARK); printw("ts ");
	attron(KEYMARK); printw("R"); attroff(KEYMARK); printw("eset ");
	attron(KEYMARK); printw("D"); attroff(KEYMARK); printw("etails ");
	attron(KEYMARK); printw("?"); attroff(KEYMARK); printw("Help");
#undef KEYMARK
	mvwprintw(stdscr, LINES-1, COLS-13, "%s", conf.ifname);

	wattroff(stdscr, BLACKONWHITE);
	refresh();

	list_win = newwin(LINES/2+1, COLS, 0, 0);
	scrollok(list_win,FALSE);

	stat_win = newwin(LINES/2-2, 14, LINES/2+1, COLS-14);
	scrollok(stat_win,FALSE);

	dump_win = newwin(LINES/2-2, COLS-14, LINES/2+1, 0);
	scrollok(dump_win,TRUE);

	update_display(NULL,-1);
}


void
finish_display(int sig)
{
	endwin();
}


#define MAC_COL 30

void
filter_input(int c)
{
	char buf[18];
	int i;

	switch (c) {
	case 'm':
		TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_MGMT);
		if (conf.filter_pkt & PKT_TYPE_MGMT)
			conf.filter_pkt |= PKT_TYPE_ALL_MGMT;
		else
			conf.filter_pkt &= ~PKT_TYPE_ALL_MGMT;
		break;
	case 'b': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_BEACON); break;
	case 'p': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_PROBE); break;
	case 'a': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_ASSOC); break;
	case 'u': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_AUTH); break;
	case 'c':
		TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_CTRL);
		if (conf.filter_pkt & PKT_TYPE_CTRL)
			conf.filter_pkt |= PKT_TYPE_ALL_CTRL;
		else
			conf.filter_pkt &= ~PKT_TYPE_ALL_CTRL;
		break;
	case 'r': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_CTS | PKT_TYPE_RTS); break;
	case 'k': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_ACK); break;
	case 'd':
		TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_DATA);
		if (conf.filter_pkt & PKT_TYPE_DATA)
			conf.filter_pkt |= PKT_TYPE_ALL_DATA;
		else
			conf.filter_pkt &= ~PKT_TYPE_ALL_DATA;
		break;
	case 'n': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_NULL); break;
	case 'R': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_ARP); break;
	case 'P': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_ICMP); break;
	case 'I': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_IP); break;
	case 'U': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_UDP); break;
	case 'T': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_TCP); break;
	case 'O': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_OLSR|PKT_TYPE_OLSR_LQ|PKT_TYPE_OLSR_GW); break;
	case 'B': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_BATMAN); break;

	case 'q': case 'Q':
		finish_all(0);

	case '\r': case KEY_ENTER:
		conf.paused = 0;
		delwin(filter_win);
		filter_win = NULL;
		update_display(NULL, -1);
		return;

	case 's':
		echo();
		print_centered(filter_win, 24, 57, "[ Enter new BSSID and ENTER ]");
		mvwgetnstr(filter_win, 5, MAC_COL + 7, buf, 17);
		noecho();
		convert_string_to_mac(buf, conf.filterbssid);
		break;

	case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
		i = c - '1';
		echo();
		mvwprintw(filter_win, 24, 15, "[ Enter new MAC %d and ENTER ]", i+1);
		mvwgetnstr(filter_win, 9 + i, MAC_COL + 7, buf, 17);
		noecho();
		convert_string_to_mac(buf, conf.filtermac[i]);
		break;

	case 'o':
		conf.do_filter = conf.do_filter ? 0 : 1;
		break;
	}

	/* sanity checks */
	/* if one of the individual mgmt frames is deselected we dont want to see all mgmt frames */
	if ((conf.filter_pkt & PKT_TYPE_ALL_MGMT) != PKT_TYPE_ALL_MGMT)
		conf.filter_pkt = conf.filter_pkt & ~PKT_TYPE_MGMT;
	/* same for ctl */
	if ((conf.filter_pkt & PKT_TYPE_ALL_CTRL) != PKT_TYPE_ALL_CTRL)
		conf.filter_pkt = conf.filter_pkt & ~PKT_TYPE_CTRL;
	/* same for data */
	if ((conf.filter_pkt & PKT_TYPE_ALL_DATA) != PKT_TYPE_ALL_DATA)
		conf.filter_pkt = conf.filter_pkt & ~PKT_TYPE_DATA;

	/* recalculate filter flag */
	conf.do_macfilter = 0;
	for (i = 0; i < MAX_FILTERMAC; i++) {
		if (MAC_NOT_EMPTY(conf.filtermac[i]))
			conf.do_macfilter = 1;
	}

	update_filter_win();
}


void
sort_input(int c)
{
	switch(c) {
	case 'n': case 'N':
	case 's': case 'S':
	case 't': case 'T':
		do_sort = c;
		/* fall thru */
	case '\r': case KEY_ENTER:
		delwin(small_win);
		small_win = NULL;
		conf.paused = 0;
		update_display(NULL, -1);
		break;
	case 'q': case 'Q':
		finish_all(0);
	}
}


void
handle_user_input()
{
	int key;

	key = getch();

	if (filter_win != NULL) {
		filter_input(key);
		return;
	}

	if (small_win != NULL) {
		sort_input(key);
		return;
	}

	switch(key) {
	case ' ': case 'p': case 'P':
		conf.paused = conf.paused ? 0 : 1;
		break;

	case 'q': case 'Q':
		finish_all(0);

	case 's': case 'S':
		if (show_win == NULL) { /* sort only makes sense in the main win */
			conf.paused = 1;
			show_sort_win();
		}
		break;

	case 'r': case 'R':
		memset(&nodes, 0, sizeof(nodes));
		memset(&essids, 0, sizeof(essids));
		memset(&splits, 0, sizeof(splits));
		memset(&hist, 0, sizeof(hist));
		memset(&stats, 0, sizeof(stats));
		gettimeofday(&stats.stats_time, NULL);
		break;

	case '?':
		conf.paused = conf.paused ? 0 : 1;
		/* fall thru */
	case 'e': case 'E':
	case 'h': case 'H':
	case 'd': case 'D':
	case 'a': case 'A':
		show_window(tolower(key));
		break;

	case 'f': case 'F':
		if (filter_win == NULL) {
			conf.paused = 1;
			filter_win = newwin(25, 57, LINES/2-15, COLS/2-15);
			scrollok(filter_win, FALSE);
			update_filter_win();
		}
		break;

	case KEY_RESIZE: /* xterm window resize event */
		endwin();
		init_display();
		return;

	default:
		return;
	}
	update_mini_status();
	update_display(NULL, -1);
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
show_sort_win(void)
{
	if (small_win == NULL) {
		small_win = newwin(1, COLS-2, LINES / 2 - 1, 1);
		wattron(small_win, BLACKONWHITE);
		mvwhline(small_win, 0, 0, ' ', COLS);
		mvwprintw(small_win, 0, 0, " -> Sort by s:SNR t:Time n:Don't sort [current: %c]", do_sort);
		wrefresh(small_win);
	}
}


#define CHECKED(_x) (conf.filter_pkt & (_x)) ? '*' : ' '
#define CHECK_ETHER(_mac) MAC_NOT_EMPTY(_mac) ? '*' : ' '

static void
update_filter_win()
{
	int l, i;

	box(filter_win, 0 , 0);
	print_centered(filter_win, 0, 57, " Edit Packet Filter ");

	mvwprintw(filter_win, 2, 2, "Show these Packet Types");

	l = 4;
	wattron(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, 2, "m: [%c] MANAGEMENT FRAMES", CHECKED(PKT_TYPE_MGMT));
	wattroff(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, 2, "b: [%c] Beacons", CHECKED(PKT_TYPE_BEACON));
	mvwprintw(filter_win, l++, 2, "p: [%c] Probe Req/Resp", CHECKED(PKT_TYPE_PROBE));
	mvwprintw(filter_win, l++, 2, "a: [%c] Association", CHECKED(PKT_TYPE_ASSOC));
	mvwprintw(filter_win, l++, 2, "u: [%c] Authentication", CHECKED(PKT_TYPE_AUTH));
	l++;
	wattron(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, 2, "c: [%c] CONTROL FRAMES", CHECKED(PKT_TYPE_CTRL));
	wattroff(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, 2, "r: [%c] CTS/RTS", CHECKED(PKT_TYPE_CTS | PKT_TYPE_RTS));
	mvwprintw(filter_win, l++, 2, "k: [%c] ACK", CHECKED(PKT_TYPE_ACK));
	l++;
	wattron(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, 2, "d: [%c] DATA FRAMES", CHECKED(PKT_TYPE_DATA));
	wattroff(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, 2, "n: [%c] Null Data", CHECKED(PKT_TYPE_NULL));
	mvwprintw(filter_win, l++, 2, "R: [%c] ARP", CHECKED(PKT_TYPE_ARP));
	mvwprintw(filter_win, l++, 2, "P: [%c] ICMP/PING", CHECKED(PKT_TYPE_ICMP));
	mvwprintw(filter_win, l++, 2, "I: [%c] IP", CHECKED(PKT_TYPE_IP));
	mvwprintw(filter_win, l++, 2, "U: [%c] UDP", CHECKED(PKT_TYPE_UDP));
	mvwprintw(filter_win, l++, 2, "T: [%c] TCP", CHECKED(PKT_TYPE_TCP));
	mvwprintw(filter_win, l++, 2, "O: [%c] OLSR", CHECKED(PKT_TYPE_OLSR));
	mvwprintw(filter_win, l++, 2, "B: [%c] BATMAN", CHECKED(PKT_TYPE_BATMAN));

	l = 4;
	wattron(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, MAC_COL, "BSSID");
	wattroff(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, MAC_COL, "s: [%c] %s",
		CHECK_ETHER(conf.filterbssid), ether_sprintf(conf.filterbssid));

	l++;
	mvwprintw(filter_win, l++, MAC_COL, "Show only these");
	wattron(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, MAC_COL, "Source MAC ADDRESSES");
	wattroff(filter_win, A_BOLD);

	for (i = 0; i < MAX_FILTERMAC; i++) {
		mvwprintw(filter_win, l++, MAC_COL, "%d: [%c] %s", i+1,
			CHECK_ETHER(conf.filtermac[i]), ether_sprintf(conf.filtermac[i]));
	}

	l++;
	wattron(filter_win, A_BOLD);
	mvwprintw(filter_win, l++, MAC_COL, "o: [%c] All Filters On/Off", conf.do_filter ? '*' : ' ' );
	wattroff(filter_win, A_BOLD);

	print_centered(filter_win, 24, 57, "[ Press key or ENTER ]");

	wrefresh(filter_win);
}


static void
update_time(time_t* sec)
{
	static char buf[9];
	strftime(buf, 9, "%H:%M:%S", localtime(sec));
	wattron(stdscr, BLACKONWHITE);
	mvwprintw(stdscr, LINES-1, COLS-9, "|%s", buf);
	wattroff(stdscr, BLACKONWHITE);
}


static void
update_mini_status(void)
{
	wattron(stdscr, BLACKONWHITE);
	mvwprintw(stdscr, LINES-1, COLS-17, conf.paused ? "P" : " ");
	mvwprintw(stdscr, LINES-1, COLS-15, conf.do_filter ? "F" : " ");
	wattroff(stdscr, BLACKONWHITE);
}


void
update_display(struct packet_info* pkt, int node_number)
{
	gettimeofday(&the_time, NULL);

	/* update only in specific intervals to save CPU time */
	if (the_time.tv_sec == last_time.tv_sec &&
	    (the_time.tv_usec - last_time.tv_usec) < conf.display_interval ) {
		/* just add the line to dump win so we dont loose it */
		if (show_win == NULL)
			update_dump_win(pkt);
		return;
	}

	if (the_time.tv_sec > last_time.tv_sec)
		update_time(&the_time.tv_sec);

	last_time = the_time;

	if (show_win != NULL)
		update_show_win();
	else if (small_win != NULL)
		wnoutrefresh(small_win);
	else if (filter_win != NULL)
		wnoutrefresh(filter_win);
	else {
		update_list_win();
		update_status_win(pkt, node_number);
		update_dump_win(pkt);
	}
	/* only one redraw */
	doupdate();
}


static void
update_show_win()
{
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


#define MAX_STAT_BAR LINES/2-6

static void
update_status_win(struct packet_info* pkt, int node_number)
{
	int sig, noi, max, rate, bps, bpsn, use, usen;

	werase(stat_win);
	wattron(stat_win, WHITE);
	mvwvline(stat_win, 0, 0, ACS_VLINE, LINES/2);
	mvwvline(stat_win, 0, 14, ACS_VLINE, LINES/2);

	bps = bytes_per_second(stats.bytes) * 8;
	bpsn = normalize(bps, 32000000, MAX_STAT_BAR); //theoretical: 54000000

	use = duration_per_second(stats.duration) * 1.0 / 10000; /* usec, in percent */
	usen = normalize(use, 100, MAX_STAT_BAR);

	if (pkt != NULL)
	{
		sig = normalize_db(-pkt->signal, MAX_STAT_BAR);
		noi = normalize_db(-pkt->noise, MAX_STAT_BAR);
		rate = normalize(pkt->rate, 108, MAX_STAT_BAR);

		if (node_number >= 0 && nodes[node_number].sig_max < 0) {
			max = normalize_db(-nodes[node_number].sig_max, MAX_STAT_BAR);
		}

		wattron(stat_win, GREEN);
		mvwprintw(stat_win, 0, 1, "SN:  %03d/", pkt->signal);
		if (max > 1)
			mvwprintw(stat_win, max, 2, "--");
		wattron(stat_win, ALLGREEN);
		mvwvline(stat_win, sig + 4, 2, ACS_BLOCK, MAX_STAT_BAR + 3 - sig);
		mvwvline(stat_win, sig + 4, 3, ACS_BLOCK, MAX_STAT_BAR + 3 - sig);

		wattron(stat_win, RED);
		mvwprintw(stat_win, 0, 10, "%03d", pkt->noise);
		wattron(stat_win, ALLRED);
		mvwvline(stat_win, noi + 4, 2, '=', MAX_STAT_BAR + 3 - noi);
		mvwvline(stat_win, noi + 4, 3, '=', MAX_STAT_BAR + 3 - noi);

		wattron(stat_win, BLUE);
		mvwprintw(stat_win, 1, 1, "PhyRate:  %2dM", pkt->rate/2);
		wattron(stat_win, ALLBLUE);
		mvwvline(stat_win, MAX_STAT_BAR + 4 - rate, 5, ACS_BLOCK, rate);
		mvwvline(stat_win, MAX_STAT_BAR + 4 - rate, 6, ACS_BLOCK, rate);
	}

	wattron(stat_win, CYAN);
	mvwprintw(stat_win, 2, 1, "b/sec: %6s", kilo_mega_ize(bps));
	wattron(stat_win, ALLCYAN);
	mvwvline(stat_win, MAX_STAT_BAR + 4 - bpsn, 8, ACS_BLOCK, bpsn);
	mvwvline(stat_win, MAX_STAT_BAR + 4 - bpsn, 9, ACS_BLOCK, bpsn);

	wattron(stat_win, YELLOW);
	mvwprintw(stat_win, 3, 1, "Usage:   %3d%%", use);
	wattron(stat_win, ALLYELLOW);
	mvwvline(stat_win, MAX_STAT_BAR + 4 - usen, 11, ACS_BLOCK, usen);
	mvwvline(stat_win, MAX_STAT_BAR + 4 - usen, 12, ACS_BLOCK, usen);
	wattroff(stat_win, ALLYELLOW);

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


static int
compare_nodes_time(const void *p1, const void *p2)
{
	struct node_info* n1 = *(struct node_info**)p1;
	struct node_info* n2 = *(struct node_info**)p2;

	if (n1->last_seen > n2->last_seen)
		return -1;
	else if (n1->last_seen == n2->last_seen)
		return 0;
	else
		return 1;
}


#define COL_IP 3
#define COL_SNR 19
#define COL_RATE 28
#define COL_SOURCE 31
#define COL_STA 49
#define COL_BSSID 52
#define COL_OLSR 72
#define COL_TSF 83

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

	if (essids[n->essid].split > 0)
		wattron(list_win, RED);

	mvwprintw(list_win, line, 1, "%c", spin[n->pkt_count % 4]);

	mvwprintw(list_win, line, COL_SNR, "%2d/%2d/%2d",
		p->snr, n->snr_max, n->snr_min);

	if (n->wlan_mode == WLAN_MODE_AP )
		mvwprintw(list_win, line, COL_STA,"A");
	else if (n->wlan_mode == WLAN_MODE_IBSS )
		mvwprintw(list_win, line, COL_STA, "I");
	else if (n->wlan_mode == WLAN_MODE_STA )
		mvwprintw(list_win, line, COL_STA, "S");
	else if (n->wlan_mode == WLAN_MODE_PROBE )
		mvwprintw(list_win, line, COL_STA, "P");

	if (n->wep)
		wprintw(list_win, "e");

	mvwprintw(list_win, line, COL_RATE, "%2d", p->rate/2);
	mvwprintw(list_win, line, COL_SOURCE, "%s", ether_sprintf(p->wlan_src));
	mvwprintw(list_win, line, COL_BSSID, "(%s)", ether_sprintf(n->wlan_bssid));
	if (n->pkt_types & PKT_TYPE_IP)
		mvwprintw(list_win, line, COL_IP, "%s", ip_sprintf(n->ip_src));
	if (n->pkt_types & PKT_TYPE_OLSR)
		mvwprintw(list_win, line, COL_OLSR, "N:%d", n->olsr_neigh);
	if (n->pkt_types & PKT_TYPE_OLSR_LQ)
		wprintw(list_win, "L");
	if (n->pkt_types & PKT_TYPE_OLSR_GW)
		wprintw(list_win, "G");

	if (n->pkt_types & PKT_TYPE_BATMAN)
		wprintw(list_win, " B");

	mvwprintw(list_win, line, COL_TSF, "%08x", n->tsf >> 32);

	if (n->channel)
		mvwprintw(list_win, line, COL_TSF + 9, "%2d", n->channel );

	wattroff(list_win, A_BOLD);
	wattroff(list_win, GREEN);
	wattroff(list_win, RED);
}


static void
update_list_win(void)
{
	int i;
	int num_nodes;
	struct node_info* n;
	int line = 0;

	werase(list_win);
	wattron(list_win, WHITE);
	box(list_win, 0 , 0);
	mvwprintw(list_win, 0, COL_SNR, "SN/MX/MI");
	mvwprintw(list_win, 0, COL_RATE, "RT");
	mvwprintw(list_win, 0, COL_SOURCE, "SOURCE");
	mvwprintw(list_win, 0, COL_STA, "Me");
	mvwprintw(list_win, 0, COL_BSSID, "(BSSID)");
	mvwprintw(list_win, 0, COL_IP, "IP");
	mvwprintw(list_win, 0, COL_OLSR, "MESH");
	mvwprintw(list_win, 0, COL_TSF, "TSF High");
	mvwprintw(list_win, 0, COL_TSF+9, "CH");

	/* reuse bottom line for information on other win */
	mvwprintw(list_win, LINES/2, 0, "Sig/Noi-RT-SOURCE");
	mvwprintw(list_win, LINES/2, 29, "(BSSID)");
	mvwprintw(list_win, LINES/2, 49, "TYPE");
	mvwprintw(list_win, LINES/2, 56, "INFO");
	mvwprintw(list_win, LINES/2, COLS-12, "LiveStatus");

	/* create an array of node pointers to make sorting independent */
	for (i = 0; i < MAX_NODES && nodes[i].status == 1; i++)
		sort_nodes[i] = &nodes[i];

	num_nodes = i;

	if (do_sort == 's') /* sort by SNR */
		qsort(sort_nodes, num_nodes, sizeof(struct node_info*), compare_nodes_snr);
	else if (do_sort == 't')  /* sort by last seen */
		qsort(sort_nodes, num_nodes, sizeof(struct node_info*), compare_nodes_time);

	for (i = 0; i < num_nodes; i++) {
		n = sort_nodes[i];
		if (n->last_seen > (the_time.tv_sec - conf.node_timeout)) {
			line++;
			/* Prevent overdraw of last line */
			if (line < LINES/2)
				print_list_line(line , n);
		}
	}

	if (splits.count > 0) {
		wattron(list_win, RED);
		mvwprintw(list_win, LINES / 2 - 2, 10, " *** IBSS SPLIT DETECTED!!! ESSID ");
		wprintw(list_win, "'%s' ", essids[splits.essid[0]].essid);
		wprintw(list_win, "%d nodes *** ", essids[splits.essid[0]].num_nodes);
		wattroff(list_win, RED);
	}
	wnoutrefresh(list_win);
}


static void
update_essid_win(void)
{
	int i, n;
	int line = 1;
	struct node_info* node;

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " ESSIDs ");

	for (i = 0; i < MAX_ESSIDS && essids[i].num_nodes > 0; i++) {
		wattron(show_win, WHITE);
		mvwprintw(show_win, line, 2, "ESSID '%s'", essids[i].essid );
		if (essids[i].split > 0) {
			wattron(show_win, RED);
			wprintw(show_win, " *** SPLIT ***");
		}
		else
			wattron(show_win, GREEN);
		line++;
		for (n = 0; n < essids[i].num_nodes && n < MAX_NODES; n++) {
			node = &nodes[essids[i].nodes[n]];
			mvwprintw(show_win, line, 3, "%2d. %s %s", n+1,
				node->wlan_mode == WLAN_MODE_AP ? "AP  " : "IBSS",
				ether_sprintf(node->last_pkt.wlan_src));
			wprintw(show_win, " BSSID (%s) ", ether_sprintf(node->wlan_bssid));
			wprintw(show_win, "TSF %016llx", node->tsf);
			wprintw(show_win, " CH %d", node->channel);
			wprintw(show_win, " %ddB", node->snr);
			wprintw(show_win, " %s", node->wep ? "WEP" : "OPEN");
			if (node->pkt_types & PKT_TYPE_IP)
				wprintw(show_win, " %s", ip_sprintf(node->ip_src));
			line++;
		}
		line++;
	}
	wnoutrefresh(show_win);
}


#define SIGN_POS LINES-14
#define TYPE_POS SIGN_POS+1
#define RATE_POS LINES-2

static void
update_hist_win(void)
{
	int i;
	int col = COLS-2;
	int sig, noi, rat;

	if (col > MAX_HISTORY)
		col = 4 + MAX_HISTORY;

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Signal/Noise/Rate History ");
	mvwhline(show_win, SIGN_POS, 1, ACS_HLINE, col);
	mvwhline(show_win, SIGN_POS+2, 1, ACS_HLINE, col);
	mvwvline(show_win, 1, 4, ACS_VLINE, LINES-3);

	mvwprintw(show_win, 1, 1, "dBm");
	mvwprintw(show_win, normalize_db(30, SIGN_POS), 1, "-30");
	mvwprintw(show_win, normalize_db(40, SIGN_POS), 1, "-40");
	mvwprintw(show_win, normalize_db(50, SIGN_POS), 1, "-50");
	mvwprintw(show_win, normalize_db(60, SIGN_POS), 1, "-60");
	mvwprintw(show_win, normalize_db(70, SIGN_POS), 1, "-70");
	mvwprintw(show_win, normalize_db(80, SIGN_POS), 1, "-80");
	mvwprintw(show_win, normalize_db(90, SIGN_POS), 1, "-90");
	mvwprintw(show_win, SIGN_POS-1, 1, "-99");

	wattron(show_win, GREEN);
	mvwprintw(show_win, 1, col-6, "Signal");
	wattron(show_win, RED);
	mvwprintw(show_win, 2, col-5, "Noise");

	wattron(show_win, CYAN);
	mvwprintw(show_win, TYPE_POS, 1, "TYP");
	mvwprintw(show_win, 3, col-11, "Packet Type");

	wattron(show_win, A_BOLD);
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
	wattroff(show_win, A_BOLD);

	i = hist.index - 1;

	while (col > 4 && hist.signal[i] != 0)
	{
		sig = normalize_db(-hist.signal[i], SIGN_POS);
		noi = normalize_db(-hist.noise[i], SIGN_POS);

		wattron(show_win, ALLGREEN);
		mvwvline(show_win, sig, col, ACS_BLOCK, SIGN_POS-sig);

		wattron(show_win, ALLRED);
		mvwvline(show_win, noi, col, '=', SIGN_POS-noi);

		wattron(show_win, CYAN);
		mvwprintw(show_win, TYPE_POS, col, "%c", \
			get_packet_type_char(hist.type[i]));

		/* make rate table smaller by joining some values */
		switch (hist.rate[i]/2) {
			case 54: rat = 9; break;
			case 48: rat = 8; break;
			case 36: rat = 7; break;
			case 24: rat = 6; break;
			case 18: rat = 5; break;
			case 12: case 11: case 9: rat = 4; break;
			case 6: case 5: rat = 3; break;
			case 2: rat = 2; break;
			case 1: rat = 1; break;
			default: rat = 0;
		}
		wattron(show_win, A_BOLD);
		wattron(show_win, BLUE);
		mvwvline(show_win, RATE_POS - rat, col, 'x', rat);
		wattroff(show_win, A_BOLD);

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
		wprintw(dump_win, "\n%s", conf.paused ? "- PAUSED -" : "- RESUME -");
		wnoutrefresh(dump_win);
		return;
	}

	wattron(dump_win, CYAN);

	if (pkt->olsr_type > 0 && pkt->pkt_types & PKT_TYPE_OLSR)
		wattron(dump_win, A_BOLD);

	wprintw(dump_win, "\n%03d/%03d ", pkt->signal, pkt->noise);
	wprintw(dump_win, "%2d ", pkt->rate/2);
	wprintw(dump_win, "%s ", ether_sprintf(pkt->wlan_src));
	wprintw(dump_win, "(%s) ", ether_sprintf(pkt->wlan_bssid));

	if (pkt->pkt_types & PKT_TYPE_OLSR) {
		wprintw(dump_win, "%-7s%s ", "OLSR", ip_sprintf(pkt->ip_src));
		switch (pkt->olsr_type) {
			case HELLO_MESSAGE: wprintw(dump_win, "HELLO"); break;
			case TC_MESSAGE: wprintw(dump_win, "TC"); break;
			case MID_MESSAGE: wprintw(dump_win, "MID");break;
			case HNA_MESSAGE: wprintw(dump_win, "HNA"); break;
			case LQ_HELLO_MESSAGE: wprintw(dump_win, "LQ_HELLO"); break;
			case LQ_TC_MESSAGE: wprintw(dump_win, "LQ_TC"); break;
			default: wprintw(dump_win, "(%d)", pkt->olsr_type);
		}
	}
	else if (pkt->pkt_types & PKT_TYPE_UDP) {
		wprintw(dump_win, "%-7s%s", "UDP", ip_sprintf(pkt->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(pkt->ip_dst));
	}
	else if (pkt->pkt_types & PKT_TYPE_TCP) {
		wprintw(dump_win, "%-7s%s", "TCP", ip_sprintf(pkt->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(pkt->ip_dst));
	}
	else if (pkt->pkt_types & PKT_TYPE_ICMP) {
		wprintw(dump_win, "%-7s%s", "PING", ip_sprintf(pkt->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(pkt->ip_dst));
	}
	else if (pkt->pkt_types & PKT_TYPE_IP) {
		wprintw(dump_win, "%-7s%s", "IP", ip_sprintf(pkt->ip_src));
		wprintw(dump_win, " -> %s", ip_sprintf(pkt->ip_dst));
	}
	else if (pkt->pkt_types & PKT_TYPE_ARP) {
		wprintw(dump_win, "%-7s", "ARP", ip_sprintf(pkt->ip_src));
	}
	else {
		wprintw(dump_win, "%-7s", get_packet_type_name(pkt->wlan_type));

		switch (pkt->wlan_type & IEEE80211_FCTL_FTYPE) {
		case IEEE80211_FTYPE_DATA:
			if ( current_packet.wlan_wep == 1)
				wprintw(dump_win, "ENCRYPTED");
			break;
		case IEEE80211_FTYPE_CTL:
			switch (pkt->wlan_type & IEEE80211_FCTL_STYPE) {
			case IEEE80211_STYPE_CTS:
			case IEEE80211_STYPE_RTS:
			case IEEE80211_STYPE_ACK:
				wprintw(dump_win, "%s", ether_sprintf(pkt->wlan_dst));
				break;
			}
			break;
		case IEEE80211_FTYPE_MGMT:
			switch (current_packet.wlan_type & IEEE80211_FCTL_STYPE) {
			case IEEE80211_STYPE_BEACON:
			case IEEE80211_STYPE_PROBE_RESP:
				wprintw(dump_win, "'%s' %llx", pkt->wlan_essid,
					pkt->wlan_tsf);
				break;
			case IEEE80211_STYPE_PROBE_REQ:
				wprintw(dump_win, "'%s'", pkt->wlan_essid);
				break;
			}
		}
	}
	wattroff(dump_win,A_BOLD);
	wnoutrefresh(dump_win);
}


#define STAT_PACK_POS 9
#define STAT_BYTE_POS (STAT_PACK_POS + 9)
#define STAT_BPP_POS (STAT_BYTE_POS + 9)
#define STAT_PP_POS (STAT_BPP_POS + 6)
#define STAT_BP_POS (STAT_PP_POS + 6)
#define STAT_AIR_POS (STAT_BP_POS + 6)
#define STAT_AIRG_POS (STAT_AIR_POS + 6)

static void
update_statistics_win(void)
{
	int i;
	int line;
	int bps, dps;
	float duration;

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Packet Statistics ");

	if (stats.packets == 0) {
		wnoutrefresh(show_win);
		return; /* avoid floating point exceptions */
	}

	mvwprintw(show_win, 2, 2, "Packets: %d",stats.packets );
	mvwprintw(show_win, 3, 2, "Bytes:   %s (%d)",  kilo_mega_ize(stats.bytes), stats.bytes );
	mvwprintw(show_win, 4, 2, "Average: ~%d B/Pkt", stats.bytes/stats.packets);

	bps = bytes_per_second(stats.bytes) * 8;
	mvwprintw(show_win, 2, 40, "bit/sec: %s (%d)", kilo_mega_ize(bps), bps);

	dps = duration_per_second(stats.duration);
	mvwprintw(show_win, 3, 40, "Duration:   %3.1f%% (%d)", dps * 1.0 / 10000, dps ); /* usec in % */

	line = 6;
	mvwprintw(show_win, line, STAT_PACK_POS, " Packets");
	mvwprintw(show_win, line, STAT_BYTE_POS, "   Bytes");
	mvwprintw(show_win, line, STAT_BPP_POS, "~B/P");
	mvwprintw(show_win, line, STAT_PP_POS, "Pkts%%");
	mvwprintw(show_win, line, STAT_BP_POS, "Byte%%");
	wattron(show_win, A_BOLD);
	mvwprintw(show_win, line, STAT_AIR_POS, "Duration%%");
	mvwprintw(show_win, line++, 2, "RATE");
	wattroff(show_win, A_BOLD);
	mvwhline(show_win, line++, 2, '-', COLS-4);
	for (i = 1; i < MAX_RATES; i++) {
		if (stats.packets_per_rate[i] > 0) {
			wattron(show_win, A_BOLD);
			mvwprintw(show_win, line, 2, "%3dM", i/2);
			wattroff(show_win, A_BOLD);
			mvwprintw(show_win, line, STAT_PACK_POS, "%8d",
				stats.packets_per_rate[i]);
			mvwprintw(show_win, line, STAT_BYTE_POS, "%8s",
				kilo_mega_ize(stats.bytes_per_rate[i]));
			mvwprintw(show_win, line, STAT_BPP_POS, "%4d",
				stats.bytes_per_rate[i] / stats.packets_per_rate[i]);
			mvwprintw(show_win, line, STAT_PP_POS, "%2.1f",
				stats.packets_per_rate[i] * 100.0 / stats.packets);
			mvwprintw(show_win, line, STAT_BP_POS, "%2.1f",
				stats.bytes_per_rate[i] * 100.0 / stats.bytes);
			wattron(show_win, A_BOLD);
			duration = stats.duration_per_rate[i] * 100.0 / stats.duration;
			mvwprintw(show_win, line, STAT_AIR_POS, "%2.1f", duration);
			mvwhline(show_win, line, STAT_AIRG_POS, '*',
				normalize(duration, 100, COLS - STAT_AIRG_POS - 2));
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
	mvwprintw(show_win, line, STAT_AIR_POS, "Duration%%");
	mvwprintw(show_win, line++, 2, "TYPE");
	wattroff(show_win, A_BOLD);
	mvwhline(show_win, line++, 2, '-', COLS - 4);
	for (i = 0; i < MAX_FSTYPE; i++) {
		if (stats.packets_per_type[i] > 0) {
			wattron(show_win, A_BOLD);
			mvwprintw(show_win, line, 2, "%s", get_packet_type_name(i));
			wattroff(show_win, A_BOLD);
			mvwprintw(show_win, line, STAT_PACK_POS, "%8d",
				stats.packets_per_type[i]);
			mvwprintw(show_win, line, STAT_BYTE_POS, "%8s",
				kilo_mega_ize(stats.bytes_per_type[i]));
			mvwprintw(show_win, line, STAT_BPP_POS, "%4d",
				stats.bytes_per_type[i] / stats.packets_per_type[i]);
			mvwprintw(show_win, line, STAT_PP_POS, "%2.1f",
				stats.packets_per_type[i] * 100.0 / stats.packets);
			mvwprintw(show_win, line, STAT_BP_POS, "%2.1f",
				stats.bytes_per_type[i] * 100.0 / stats.bytes);
			wattron(show_win, A_BOLD);
			if (stats.duration > 0)
				duration = stats.duration_per_type[i] * 100.0 / stats.duration;
			else
				duration = 100.0;
			mvwprintw(show_win, line, STAT_AIR_POS, "%2.1f", duration);
			mvwhline(show_win, line, STAT_AIRG_POS, '*',
				normalize(duration, 100, COLS - STAT_AIRG_POS - 2));
			wattroff(show_win, A_BOLD);
			line++;
		}
	}
	wnoutrefresh(show_win);
}


static void
update_help_win(void)
{
	int i, l;
	char c;

	werase(show_win);
	wattron(show_win, WHITE);
	box(show_win, 0 , 0);
	print_centered(show_win, 0, COLS, " Help ");
	print_centered(show_win, 2, COLS, "HORST - Horsts OLSR Radio Scanning Tool");
	print_centered(show_win, 3, COLS, "Version " VERSION " (build date " __DATE__ " " __TIME__ ")");

	mvwprintw(show_win, 5, 2, "(C) 2005-2007 Bruno Randolf, Licensed under the GPLv2");

	mvwprintw(show_win, 7, 2, "Known IEEE802.11 Packet Types:");
	l = 9;
	/* this is weird but it works */
	mvwprintw(show_win, l++, 2, "MANAGEMENT FRAMES");
	for (i = 0x00; i <= 0xD0; i = i + 0x10) {
		c = get_packet_type_char(i);
		if (c != '?')
			mvwprintw(show_win, l++, 4, "%c %s", c, get_packet_type_name(i));
	}
	l = 9;
	mvwprintw(show_win, l++, 25, "CONTROL FRAMES");
	for (i = 0xa4; i <= 0xF4; i = i + 0x10) {
		c = get_packet_type_char(i);
		if (c != '?')
			mvwprintw(show_win, l++, 27, "%c %s", c, get_packet_type_name(i));
	}
	l = 9;
	mvwprintw(show_win, l++, 50, "DATA FRAMES");
	for (i = 0x08; i <+ 0xF8; i = i + 0x10) {
		c = get_packet_type_char(i);
		if (c != '?')
			mvwprintw(show_win, l++, 52, "%c %s", c, get_packet_type_name(i));
	}

	mvwprintw(show_win, ++l, 2, "For more info read the README or check http://br1.einfach.org/horst/");

	wrefresh(show_win);
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
