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

/******************* ESSID *******************/

#include <stdlib.h>

#include <uwifi/node.h>
#include <uwifi/util.h>
#include <uwifi/wlan_util.h>
#include <uwifi/essid.h>

#include "display.h"
#include "main.h"
#include "hutil.h"

void update_essid_win(WINDOW *win)
{
	int i;
	int line = 1;
	struct essid_info* e;
	struct uwifi_node* n;

	werase(win);
	wattron(win, WHITE);
	wattroff(win, A_BOLD);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " ESSIDs ");

	mvwprintw(win, line++, 3, "NO. MODE SOURCE            (BSSID)             TSF              (BINT) CH Sig E IP");

	list_for_each(&essids, e, list) {
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
		list_for_each(&e->nodes, n, essid_nodes) {
			if (line > LINES-3)
				break;

			if (n->last_seen > (time_mono.tv_sec - conf.node_timeout / 2))
				wattron(win, A_BOLD);
			else
				wattroff(win, A_BOLD);
			mvwprintw(win, line, 3, "%2d. %-4s %-17s", i++,
				wlan_mode_string(n->wlan_mode),
				mac_name_lookup(n->wlan_src, 0));
			wprintw(win, " " MAC_FMT, MAC_PAR(n->wlan_bssid));
			wprintw(win, " %016llx", n->wlan_tsf);
			wprintw(win, " (%d)", n->wlan_bintval);
			if (n->wlan_bintval < 1000)
				wprintw(win, " ");
			wprintw(win, " %2d", n->wlan_channel);
			wprintw(win, " %3d", n->phy_sig_last);
			wprintw(win, " %s", n->wlan_wep ? "W" : " ");
			if (n->pkt_types & PKT_TYPE_IP)
				wprintw(win, " %s", ip_sprintf(n->ip_src));
			line++;
		}
	}
	wnoutrefresh(win);
}
