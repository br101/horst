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

/******************* HELP *******************/

#include <stdlib.h>

#include "display.h"
#include "main.h"
#include "wlan_util.h"

void update_help_win(WINDOW *win)
{
	int i, t, col, l;

	werase(win);
	wattron(win, WHITE);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " Help ");
	print_centered(win, 2, COLS, "HORST - Horsts OLSR Radio Scanning Tool (or)");
	print_centered(win, 3, COLS, "HORST - Highly Optimized Radio Scanning Tool");

	print_centered(win, 5, COLS, "Version " VERSION " (build date " __DATE__ " " __TIME__ ")");
	print_centered(win, 6, COLS, "(C) 2005-2016 Bruno Randolf, Licensed under the GPLv2");

	mvwprintw(win, 8, 2, "Known IEEE802.11 Packet Types:");

	for (t = 0; t < WLAN_NUM_TYPES; t++) {
		wattron(win, A_BOLD);
		if (t == 0) {
			l = 10;
			col = 4;
			mvwprintw(win, l++, 2, "MANAGEMENT FRAMES");
		}
		else if (t == 1) {
			l++;
			mvwprintw(win, l++, 2, "CONTROL FRAMES");

		} else {
			l = 10;
			col = 47;
			mvwprintw(win, l++, 45, "DATA FRAMES");
		}
		wattroff(win, A_BOLD);

		for (i = 0; i < WLAN_NUM_STYPES; i++) {
			if (stype_names[t][i].c != '-')
				mvwprintw(win, l++, col, "%c  %-6s  %s",
					  stype_names[t][i].c,
					  stype_names[t][i].name,
					  stype_names[t][i].desc);
		}
	}

	wattron(win, WHITE);
	print_centered(win, 39, COLS, "For more info read the man page or check http://br1.einfach.org/horst/");

	wrefresh(win);
}
