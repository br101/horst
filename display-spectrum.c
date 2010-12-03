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

/******************* SPECTRUM ANALYZER *******************/

#include <stdlib.h>

#include "display.h"
#include "main.h"
#include "util.h"

#define CH_SPACE	5
#define SPEC_HEIGHT	(LINES - 11)
#define SPEC_POS_Y	7
#define SPEC_POS_X	6


void
update_spectrum_win(WINDOW *win)
{
	int i, sig, sig_avg, siga;
	struct chan_node *cn;
	const char *id;

	werase(win);
	wattron(win, WHITE);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " \"Spectrum Analyzer\" ");

	mvwprintw(win, 2, 2, "Current Channel:");
	mvwprintw(win, 2, 19, "%d   ", channels[conf.current_channel].chan);
	mvwprintw(win, 3, 2, "c: [%c] Automatically Change Channel", conf.do_change_channel ? '*' : ' ');
	mvwprintw(win, 4, 2, "d: Channel Dwell Time: %d ms", conf.channel_time/1000);
	mvwprintw(win, 5, 2, "m: Manually Enter Channel:      ");

	mvwprintw(win, SPEC_POS_Y+1, 1, "dBm");
	for(i = -30; i > -100; i -= 10) {
		sig = normalize_db(-i, SPEC_HEIGHT);
		mvwprintw(win, SPEC_POS_Y+sig, 1, "%d", i);
	}
	mvwhline(win, LINES-4, 1, ACS_HLINE, COLS-2);
	mvwvline(win, SPEC_POS_Y, 4, ACS_VLINE, LINES-SPEC_POS_Y-2);

	for (i = 0; i < conf.num_channels; i++) {
		sig_avg = iir_average_get(spectrum[i].signal_avg);
		mvwprintw(win, 7, SPEC_POS_X+CH_SPACE*i, "%d", spectrum[i].num_nodes);
		mvwprintw(win, 8, SPEC_POS_X+CH_SPACE*i, "%d", spectrum[i].signal);
		if (spectrum[i].packets > 8)
			mvwprintw(win, 9, SPEC_POS_X+CH_SPACE*i, "%d", sig_avg);

		mvwprintw(win, LINES-3, SPEC_POS_X+CH_SPACE*i, "%02d", channels[i].chan);

		if (spectrum[i].signal != 0) {
			sig = normalize_db(-spectrum[i].signal, SPEC_HEIGHT);
			wattron(win, ALLGREEN);
			mvwvline(win, SPEC_POS_Y+sig, SPEC_POS_X+CH_SPACE*i, ACS_BLOCK,
				SPEC_HEIGHT - sig);
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
		/* show nodes */
		list_for_each_entry(cn, &spectrum[i].nodes, chan_list) {
			if (cn->packets >= 8)
				sig = normalize_db(-iir_average_get(cn->sig_avg), SPEC_HEIGHT);
			else
				sig = normalize_db(-cn->sig, SPEC_HEIGHT);
			if (cn->node->ip_src)
				id = ip_sprintf_short(cn->node->ip_src);
			else
				id = ether_sprintf_short(cn->node->last_pkt.wlan_src);
			mvwprintw(win, SPEC_POS_Y+sig, SPEC_POS_X+CH_SPACE*i, "%s", id);
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

	default:
		return 0; /* didn't handle input */
	}
	return 1;
}
