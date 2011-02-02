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

/******************* HISTORY *******************/

#include <stdlib.h>

#include "display.h"
#include "main.h"
#include "util.h"

#define SIGN_POS LINES-17
#define TYPE_POS SIGN_POS+1
#define RATE_POS LINES-2


void
update_history_win(WINDOW *win)
{
	int i;
	int col = COLS-2;
	int sig, noi = 0, rat;

	if (col > MAX_HISTORY)
		col = 4 + MAX_HISTORY;

	werase(win);
	wattron(win, WHITE);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " Signal/Noise/Rate History ");
	mvwhline(win, SIGN_POS, 1, ACS_HLINE, col);
	mvwhline(win, SIGN_POS+2, 1, ACS_HLINE, col);
	mvwvline(win, 1, 4, ACS_VLINE, LINES-3);

	wattron(win, GREEN);
	mvwprintw(win, 2, 1, "dBm");
	mvwprintw(win, normalize_db(30, SIGN_POS - 1) + 1, 1, "-30");
	mvwprintw(win, normalize_db(40, SIGN_POS - 1) + 1, 1, "-40");
	mvwprintw(win, normalize_db(50, SIGN_POS - 1) + 1, 1, "-50");
	mvwprintw(win, normalize_db(60, SIGN_POS - 1) + 1, 1, "-60");
	mvwprintw(win, normalize_db(70, SIGN_POS - 1) + 1, 1, "-70");
	mvwprintw(win, normalize_db(80, SIGN_POS - 1) + 1, 1, "-80");
	mvwprintw(win, normalize_db(90, SIGN_POS - 1) + 1, 1, "-90");
	mvwprintw(win, SIGN_POS-1, 1, "-99");

	mvwprintw(win, 1, col-6, "Signal");
	wattron(win, RED);
	mvwprintw(win, 2, col-5, "Noise");

	wattron(win, CYAN);
	mvwprintw(win, TYPE_POS, 1, "TYP");
	mvwprintw(win, 3, col-11, "Packet Type");

	wattron(win, A_BOLD);
	wattron(win, BLUE);
	mvwprintw(win, 4, col-4, "Rate");
	mvwprintw(win, RATE_POS-12, 1, "54M");
	mvwprintw(win, RATE_POS-11, 1, "48M");
	mvwprintw(win, RATE_POS-10, 1, "36M");
	mvwprintw(win, RATE_POS-9, 1, "24M");
	mvwprintw(win, RATE_POS-8, 1, "18M");
	mvwprintw(win, RATE_POS-7, 1, "12M");
	mvwprintw(win, RATE_POS-6, 1, "11M");
	mvwprintw(win, RATE_POS-5, 1, " 9M");
	mvwprintw(win, RATE_POS-4, 1, " 6M");
	mvwprintw(win, RATE_POS-3, 1, "5.M");
	mvwprintw(win, RATE_POS-2, 1, " 2M");
	mvwprintw(win, RATE_POS-1, 1, " 1M");
	wattroff(win, A_BOLD);

	i = hist.index - 1;

	while (col > 4 && hist.signal[i] != 0)
	{
		sig = normalize_db(-hist.signal[i], SIGN_POS - 1);
		if (hist.noise[i])
			noi = normalize_db(-hist.noise[i], SIGN_POS - 1);

		wattron(win, ALLGREEN);
		mvwvline(win, sig + 1, col, ACS_BLOCK, SIGN_POS - sig - 1);

		if (hist.noise[i]) {
			wattron(win, ALLRED);
			mvwvline(win, noi + 1, col, '=', SIGN_POS - noi -1);
		}

		wattron(win, get_packet_type_color(hist.type[i]));
		mvwprintw(win, TYPE_POS, col, "%c", \
			get_packet_type_char(hist.type[i]));

		if (hist.retry[i])
			mvwprintw(win, TYPE_POS+1, col, "r");

		switch (hist.rate[i]/2) {
			case 54: rat = 12; break;
			case 48: rat = 11; break;
			case 36: rat = 10; break;
			case 24: rat = 9; break;
			case 18: rat = 8; break;
			case 12: rat = 7; break;
			case 11: rat = 6; break;
			case 9: rat = 5; break;
			case 6: rat = 4; break;
			case 5: rat = 3; break;
			case 2: rat = 2; break;
			case 1: rat = 1; break;
			default: rat = 0;
		}
		wattron(win, A_BOLD);
		wattron(win, BLUE);
		mvwvline(win, RATE_POS - rat, col, 'x', rat);
		wattroff(win, A_BOLD);

		i--;
		col--;
		if (i < 0)
			i = MAX_HISTORY-1;
	}
	wnoutrefresh(win);
}
