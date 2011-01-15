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

/******************* POOR MAN's "SPECTRUM ANALYZER" *******************/

#include <stdlib.h>

#include "display.h"
#include "main.h"
#include "util.h"

#define CH_SPACE	6
#define SPEC_POS_Y	1
#define SPEC_HEIGHT	(LINES - SPEC_POS_X - 2)
#define SPEC_POS_X	6

static unsigned int show_nodes;

void
update_spectrum_win(WINDOW *win)
{
	int i, sig, sig_avg, siga, use, usen, nnodes;
	struct chan_node *cn;
	const char *id;

	werase(win);
	wattron(win, WHITE);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " \"Spectrum Analyzer\" ");

	mvwprintw(win, SPEC_HEIGHT+2, 1, "CHA");
	wattron(win, GREEN);
	mvwprintw(win, SPEC_HEIGHT+3, 1, "Sig");
	wattron(win, BLUE);
	mvwprintw(win, SPEC_HEIGHT+4, 1, "Nod");
	wattron(win, YELLOW);
	mvwprintw(win, SPEC_HEIGHT+5, 1, "Use");
	wattroff(win, YELLOW);

	mvwprintw(win, SPEC_POS_Y+1, 1, "dBm");
	for(i = -30; i > -100; i -= 10) {
		sig = normalize_db(-i, SPEC_HEIGHT);
		mvwprintw(win, SPEC_POS_Y+sig, 1, "%d", i);
	}
	mvwhline(win, SPEC_HEIGHT+1, 1, ACS_HLINE, COLS-2);
	mvwvline(win, SPEC_POS_Y, 4, ACS_VLINE, LINES-SPEC_POS_Y-2);

	for (i = 0; i < conf.num_channels; i++) {
		mvwprintw(win, SPEC_HEIGHT+2, SPEC_POS_X+CH_SPACE*i, "%02d", channels[i].chan);

		sig_avg = iir_average_get(spectrum[i].signal_avg);
		wattron(win, GREEN);
		mvwprintw(win,  SPEC_HEIGHT+3, SPEC_POS_X+CH_SPACE*i, "%d", spectrum[i].signal);
		wattron(win, BLUE);
		mvwprintw(win,  SPEC_HEIGHT+4, SPEC_POS_X+CH_SPACE*i, "%d", spectrum[i].num_nodes);
		wattroff(win, BLUE);

		if (spectrum[i].signal != 0) {
			sig = normalize_db(-spectrum[i].signal, SPEC_HEIGHT);
			wattron(win, ALLGREEN);
			mvwvline(win, SPEC_POS_Y+sig, SPEC_POS_X+CH_SPACE*i, ACS_BLOCK,
				SPEC_HEIGHT - sig);
			if (!show_nodes)
				mvwvline(win, SPEC_POS_Y+sig, SPEC_POS_X+CH_SPACE*i+1, ACS_BLOCK,
					SPEC_HEIGHT - sig);
		}

		if (spectrum[i].packets > 8 && sig_avg != 0) {
			siga = normalize_db(-sig_avg, SPEC_HEIGHT);
			if (siga > 1) {
				wattron(win, A_BOLD);
				mvwvline(win, SPEC_POS_Y+siga, SPEC_POS_X+CH_SPACE*i, '=',
				SPEC_HEIGHT - siga);
				wattroff(win, A_BOLD);
			}
		}
		wattroff(win, ALLGREEN);

		use = (spectrum[i].durations - spectrum[i].durations_last)
				* 1.0 / 1000;
		wattron(win, YELLOW);
		mvwprintw(win,  SPEC_HEIGHT+5, SPEC_POS_X+CH_SPACE*i, "%d", use);
		wattroff(win, YELLOW);

		if (show_nodes) {
			wattron(win, BLUE);
			list_for_each_entry(cn, &spectrum[i].nodes, chan_list) {
				if (cn->packets >= 8)
					sig = normalize_db(-iir_average_get(cn->sig_avg),
						SPEC_HEIGHT);
				else
					sig = normalize_db(-cn->sig, SPEC_HEIGHT);
				if (cn->node->ip_src) {
					wattron(win, A_BOLD);
					id = ip_sprintf_short(cn->node->ip_src);
				}
				else
					id = ether_sprintf_short(cn->node->last_pkt.wlan_src);
				mvwprintw(win, SPEC_POS_Y+sig, SPEC_POS_X+CH_SPACE*i+1, "%s", id);
				if (cn->node->ip_src)
					wattroff(win, A_BOLD);
			}
			wattroff(win, BLUE);
		}
		else {
			nnodes = spectrum[i].num_nodes;
			wattron(win, ALLBLUE);
			mvwvline(win, SPEC_POS_Y+SPEC_HEIGHT-nnodes, SPEC_POS_X+CH_SPACE*i+2,
				ACS_BLOCK, nnodes);
			wattroff(win, ALLBLUE);
#if 0
			mvwprintw(win, 11, SPEC_POS_X+CH_SPACE*i, "%d", spectrum[i].durations);
			mvwprintw(win, 12, SPEC_POS_X+CH_SPACE*i, "%d", spectrum[i].durations_last);
#endif
			usen = normalize(use, conf.channel_time, SPEC_HEIGHT);
			wattron(win, ALLYELLOW);
			mvwvline(win, SPEC_POS_Y+SPEC_HEIGHT-usen, SPEC_POS_X+CH_SPACE*i+3,
				ACS_BLOCK, usen);
			wattroff(win, ALLYELLOW);
		}
	}

	wnoutrefresh(win);
}


int
spectrum_input(WINDOW *win, int c)
{
	char buf[6];
	int x;

	switch (c) {
	case 'c': case 'C':
		conf.do_change_channel = conf.do_change_channel ? 0 : 1;
		break;

	case 'm': case 'M':
		conf.do_change_channel = 0;
		echo();
		curs_set(1);
		mvwgetnstr(win, 5, 29, buf, 2);
		curs_set(0);
		noecho();
		sscanf(buf, "%d", &x);
		if (x > 0)
			change_channel(x);
		break;

	case 'd': case 'D':
		echo();
		curs_set(1);
		mvwgetnstr(win, 4, 25, buf, 6);
		curs_set(0);
		noecho();
		sscanf(buf, "%d", &x);
		conf.channel_time = x*1000;
		break;

	case 'u': case 'U':
		echo();
		curs_set(1);
		mvwgetnstr(win, 4, 51, buf, 6);
		curs_set(0);
		noecho();
		sscanf(buf, "%d", &x);
		conf.channel_max = x;
		break;

	case 'n': case 'N':
		show_nodes = show_nodes ? 0 : 1;
		break;

	default:
		return 0; /* didn't handle input */
	}
	return 1;
}
