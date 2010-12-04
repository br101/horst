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

/******************* ESSID *******************/

#include <stdlib.h>

#include "display.h"
#include "main.h"
#include "util.h"


void
update_essid_win(WINDOW *win)
{
	int i;
	int line = 1;
	struct essid_info* e;
	struct node_info* node;

	werase(win);
	wattron(win, WHITE);
	wattroff(win, A_BOLD);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " ESSIDs ");

	mvwprintw(win, line++, 3, "NO. MODE SOURCE            (BSSID)             TSF              (BINT) CH SNR  E IP");

	list_for_each_entry(e, &essids.list, list) {
		if (line > LINES-3)
			break;

		wattron(win, WHITE | A_BOLD);
		mvwprintw(win, line, 2, "ESSID '%s'", e->essid );
		if (e->split > 0) {
			wattron(win, RED);
			wprintw(win, " *** SPLIT ***");
		}
		else
			wattron(win, GREEN);
		line++;

		i = 1;
		list_for_each_entry(node, &e->nodes, essid_nodes) {
			if (line > LINES-3)
				break;

			if (node->last_seen > (the_time.tv_sec - conf.node_timeout / 2))
				wattron(win, A_BOLD);
			else
				wattroff(win, A_BOLD);
			mvwprintw(win, line, 3, "%2d. %s %s", i++,
				node->wlan_mode == WLAN_MODE_AP ? "AP  " : "IBSS",
				ether_sprintf(node->last_pkt.wlan_src));
			wprintw(win, " (%s)", ether_sprintf(node->wlan_bssid));
			wprintw(win, " %016llx", node->wlan_tsf);
			wprintw(win, " (%d)", node->wlan_bintval);
			if (node->wlan_bintval < 1000)
				wprintw(win, " ");
			wprintw(win, " %2d", node->wlan_channel);
			wprintw(win, " %2ddB", node->last_pkt.phy_snr);
			wprintw(win, " %s", node->wlan_wep ? "W" : " ");
			if (node->pkt_types & PKT_TYPE_IP)
				wprintw(win, " %s", ip_sprintf(node->ip_src));
			line++;
		}
	}
	wnoutrefresh(win);
}
