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

/******************* FILTER *******************/

#include <stdlib.h>

#include <uwifi/wlan_util.h>
#include <uwifi/log.h>

#include "display.h"
#include "main.h"
#include "network.h"

#define COL_BAND2 23

static bool set_ht40plus;

void update_channel_win(WINDOW *win)
{
	int l = 2;

	box(win, 0 , 0);
	print_centered(win, 0, 39, " Channel Settings ");

	wattron(win, WHITE);
	for (int b = 0; b < uwifi_channel_get_num_bands(&conf.intf.channels); b++) {
		const struct uwifi_band* bp = uwifi_channel_get_band(&conf.intf.channels, b);
		int c = uwifi_channel_idx_from_band_idx(&conf.intf.channels, b, 0);
		int col = uwifi_channel_get_chan(&conf.intf.channels, c) > 14 ? COL_BAND2 : 2;
		wattron(win, A_BOLD);
		mvwprintw(win, 2, col, "%s: %s",
			col == 2 ? "2.4GHz" : "5GHz",
			uwifi_channel_width_string(bp->max_chan_width));

		if (bp->streams_rx || bp->streams_tx)
			wprintw(win, " %dx%d", bp->streams_rx, bp->streams_tx);
		wattroff(win, A_BOLD);
		l = 3;
		for (int i = 0; (c = uwifi_channel_idx_from_band_idx(&conf.intf.channels, b, i)) != -1; i++) {
			if (c == conf.intf.channel_idx)
				wattron(win, CYAN);
			else
				wattron(win, WHITE);
			mvwprintw(win, l++,
				col,
				"%s", uwifi_channel_list_string(&conf.intf.channels, c));
		}
	}
	wattroff(win, WHITE);

	l = 18;
	wattron(win, A_BOLD);
	mvwprintw(win, l++, 2, "s: [%c] Scan",
		  CHECKED(conf.intf.channel_scan));
	wattroff(win, A_BOLD);
	mvwprintw(win, l++, 2, "d: Dwell: %d ms   ",
		  conf.intf.channel_time/1000);
	mvwprintw(win, l++, 2, "u: Upper limit: %d  ", conf.intf.channel_max);

	l++;
	wattron(win, A_BOLD);
	mvwprintw(win, l++, 2, "m: Set channel: %d  ", wlan_freq2chan(conf.intf.channel_set.freq));
	wattroff(win, A_BOLD);
	mvwprintw(win, l++, 2, "1: [%c] 20 (no HT)",
		CHECKED(conf.intf.channel_set.width == CHAN_WIDTH_20_NOHT));
	mvwprintw(win, l++, 2, "2: [%c] HT20",
		CHECKED(conf.intf.channel_set.width == CHAN_WIDTH_20));
	mvwprintw(win, l++, 2, "4: [%c] HT40-",
		CHECKED(conf.intf.channel_set.width == CHAN_WIDTH_40 && !set_ht40plus));
	mvwprintw(win, l++, 2, "5: [%c] HT40+",
		CHECKED(conf.intf.channel_set.width == CHAN_WIDTH_40 && set_ht40plus));
	mvwprintw(win, l++, 2, "8: [%c] VHT80",
		CHECKED(conf.intf.channel_set.width == CHAN_WIDTH_80));
	mvwprintw(win, l++, 2, "6: [%c] VHT160",
		CHECKED(conf.intf.channel_set.width == CHAN_WIDTH_160));

	print_centered(win, CHANNEL_WIN_HEIGHT-1, CHANNEL_WIN_WIDTH,
		       "[ Press keys and ENTER to apply ]");
	wrefresh(win);
}

bool channel_input(WINDOW *win, int c)
{
	char buf[6];
	int x;
	int new_idx = -1;

	switch (c) {
	case 's': case 'S':
		conf.intf.channel_scan = conf.intf.channel_scan ? 0 : 1;
		break;

	case 'd': case 'D':
		echo();
		curs_set(1);
		mvwgetnstr(win, 19, 12, buf, 6);
		curs_set(0);
		noecho();
		sscanf(buf, "%d", &x);
		conf.intf.channel_time = x*1000;
		break;

	case 'u': case 'U':
		echo();
		curs_set(1);
		mvwgetnstr(win, 20, 18, buf, 6);
		curs_set(0);
		noecho();
		sscanf(buf, "%d", &x);
		conf.intf.channel_max = x;
		break;

	case 'm': case 'M':
		echo();
		curs_set(1);
		mvwgetnstr(win, 22, 18, buf, 3);
		curs_set(0);
		noecho();
		sscanf(buf, "%d", &x);
		conf.intf.channel_set.freq = wlan_chan2freq(x);
		break;

	case '1':
		conf.intf.channel_set.width = CHAN_WIDTH_20_NOHT;
		break;

	case '2':
		conf.intf.channel_set.width = CHAN_WIDTH_20;
		break;

	case '4':
		conf.intf.channel_set.width = CHAN_WIDTH_40;
		set_ht40plus = false;
		break;

	case '5':
		conf.intf.channel_set.width = CHAN_WIDTH_40;
		set_ht40plus = true;
		break;

	case '8':
		conf.intf.channel_set.width = CHAN_WIDTH_80;
		break;

	case '6':
		conf.intf.channel_set.width = CHAN_WIDTH_160;
		break;

	case '\r': case KEY_ENTER: /* used to close win, too */
		new_idx = uwifi_channel_idx_from_freq(&conf.intf.channels, conf.intf.channel_set.freq);
		if ((new_idx >= 0 && new_idx != conf.intf.channel_idx) ||
		    conf.intf.channel_set.width != conf.intf.channel.width ||
		    set_ht40plus != uwifi_channel_is_ht40plus(&conf.intf.channel)) {
			uwifi_channel_fix_center_freq(&conf.intf.channel_set, set_ht40plus);
			/* some setting changed */
			if (conf.serveraddr[0] == '\0') {
				/* server */
				if (!uwifi_channel_change(&conf.intf, &conf.intf.channel_set)) {
					/* reset UI */
					conf.intf.channel_set = conf.intf.channel;
				} else {
					net_send_channel_config();
				}
			} else {
				/* client */
				conf.intf.channel_idx = new_idx;
				conf.intf.channel = conf.intf.channel_set;
				LOG_INF("Sending channel config to server");
				net_send_channel_config();
			}
		}
		return false;

	default:
		return false; /* didn't handle input */
	}

	update_channel_win(win);
	return true;
}
