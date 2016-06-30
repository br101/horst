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

#include "display.h"
#include "main.h"
#include "util.h"
#include "wlan_util.h"
#include "wlan80211.h"
#include "network.h"

#define MAC_COL 2
#define MODE_COL 30
#define SECOND_ROW 19
#define THIRD_ROW 23

void update_filter_win(WINDOW *win)
{
	int l, i, t, col = 2;

	box(win, 0 , 0);
	print_centered(win, 0, 57, " Edit Filters ");

	for (t = 0; t < WLAN_NUM_TYPES; t++) {
		l = 2;
		wattron(win, get_packet_type_color(WLAN_FRAME_FC(t, 0)));
		wattron(win, A_BOLD);
		if (t == 0)
			mvwprintw(win, l++, col, "m: [%c] Management", CHECKED(conf.filter_stype[t] & 0xffff));
		else if (t == 1)
			mvwprintw(win, l++, col, "c: [%c] Control", CHECKED(conf.filter_stype[t] & 0xffff));
		else
			mvwprintw(win, l++, col, "d: [%c] Data", CHECKED(conf.filter_stype[t] & 0xffff));

		wattroff(win, A_BOLD);
		for (i = 0; i < WLAN_NUM_STYPES; i++) {
			if (stype_names[t][i].c != '-')
				mvwprintw(win, l++, col, "%c: [%c] %s", stype_names[t][i].c,
					  CHECKED(conf.filter_stype[t] & BIT(i)),
					  stype_names[t][i].name);
		}
		col += 19;
	}

	l = 15;
	wattron(win, RED);
	mvwprintw(win, l++, 21, "*: [%c] Bad FCS", CHECKED(conf.filter_badfcs));
	wattroff(win, RED);
	wattron(win, A_BOLD);
	mvwprintw(win, l++, 21, "0: [%c] All Off", CHECKED(conf.filter_off));
	wattroff(win, A_BOLD);

	l = SECOND_ROW-1;
	wattron(win, A_BOLD);
	mvwprintw(win, l++, 2, "Higher Level Protocols");
	wattroff(win, A_BOLD);
	wattron(win, WHITE);
	mvwprintw(win, l++, 2, "r: [%c] ARP", CHECKED(conf.filter_pkt & PKT_TYPE_ARP));
	mvwprintw(win, l++, 2, "M: [%c] ICMP/PING", CHECKED(conf.filter_pkt & PKT_TYPE_ICMP));
	mvwprintw(win, l++, 2, "i: [%c] IP", CHECKED(conf.filter_pkt & PKT_TYPE_IP));
	l = SECOND_ROW;
	mvwprintw(win, l++, 21, "V: [%c] UDP", CHECKED(conf.filter_pkt & PKT_TYPE_UDP));
	mvwprintw(win, l++, 21, "W: [%c] TCP", CHECKED(conf.filter_pkt & PKT_TYPE_TCP));
	l = SECOND_ROW;
	mvwprintw(win, l++, 40, "I: [%c] OLSR", CHECKED(conf.filter_pkt & PKT_TYPE_OLSR));
	mvwprintw(win, l++, 40, "K: [%c] BATMAN", CHECKED(conf.filter_pkt & PKT_TYPE_BATMAN));
	mvwprintw(win, l++, 40, "Z: [%c] Meshz", CHECKED(conf.filter_pkt & PKT_TYPE_MESHZ));

	l = THIRD_ROW;
	wattron(win, A_BOLD);
	mvwprintw(win, l++, MAC_COL, "Source MAC Addresses");
	wattroff(win, A_BOLD);

	for (i = 0; i < MAX_FILTERMAC; i++) {
		mvwprintw(win, l++, MAC_COL, "%d: [%c] %s", i+1,
			  CHECKED(conf.filtermac_enabled[i]),
			  ether_sprintf(conf.filtermac[i]));
	}

	l = THIRD_ROW;
	wattron(win, A_BOLD);
	mvwprintw(win, l++, MODE_COL, "BSSID");
	wattroff(win, A_BOLD);
	mvwprintw(win, l++, MODE_COL, "_: [%c] %s",
		CHECKED(MAC_NOT_EMPTY(conf.filterbssid)), ether_sprintf(conf.filterbssid));

	l++;

	wattron(win, A_BOLD);
	mvwprintw(win, l++, MODE_COL, "Mode");
	wattroff(win, A_BOLD);
	mvwprintw(win, l++, MODE_COL, "!: [%c] Access Point", CHECKED(conf.filter_mode & WLAN_MODE_AP));
	mvwprintw(win, l++, MODE_COL, "@: [%c] Station", CHECKED(conf.filter_mode & WLAN_MODE_STA));
	mvwprintw(win, l++, MODE_COL, "#: [%c] IBSS (Ad-hoc)", CHECKED(conf.filter_mode & WLAN_MODE_IBSS));
	mvwprintw(win, l++, MODE_COL, "$: [%c] Probe Request", CHECKED(conf.filter_mode & WLAN_MODE_PROBE));
	mvwprintw(win, l++, MODE_COL, "%: [%c] WDS/4ADDR", CHECKED(conf.filter_mode & WLAN_MODE_4ADDR));
	mvwprintw(win, l++, MODE_COL, "^: [%c] Unknown", CHECKED(conf.filter_mode & WLAN_MODE_UNKNOWN));

	wattroff(win, WHITE);
	print_centered(win, ++l, FILTER_WIN_WIDTH, "[ Press key or ENTER ]");

	wrefresh(win);
}

bool filter_input(WINDOW *win, int c)
{
	char buf[18];
	int i, t;

	switch (c) {
	case 'm': TOGGLE_BITSET(conf.filter_stype[WLAN_FRAME_TYPE_MGMT], 0xffff, uint16_t); break;
	case 'c': TOGGLE_BITSET(conf.filter_stype[WLAN_FRAME_TYPE_CTRL], 0xffff, uint16_t); break;
	case 'd': TOGGLE_BITSET(conf.filter_stype[WLAN_FRAME_TYPE_DATA], 0xffff, uint16_t); break;

	case 'r': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_ARP); break;
	case 'M': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_ICMP); break;
	case 'i': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_IP); break;
	case 'V': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_UDP); break;
	case 'W': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_TCP); break;
	case 'I': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_OLSR); break;
	case 'K': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_BATMAN); break;
	case 'Z': TOGGLE_BIT(conf.filter_pkt, PKT_TYPE_MESHZ); break;

	case '!': TOGGLE_BIT(conf.filter_mode, WLAN_MODE_AP); break;
	case '@': TOGGLE_BIT(conf.filter_mode, WLAN_MODE_STA); break;
	case '#': TOGGLE_BIT(conf.filter_mode, WLAN_MODE_IBSS); break;
	case '$': TOGGLE_BIT(conf.filter_mode, WLAN_MODE_PROBE); break;
	case '%': TOGGLE_BIT(conf.filter_mode, WLAN_MODE_4ADDR); break;
	case '^': TOGGLE_BIT(conf.filter_mode, WLAN_MODE_UNKNOWN); break;

	case '_':
		echo();
		print_centered(win, FILTER_WIN_HEIGHT-1, FILTER_WIN_WIDTH,
			       "[ Enter new BSSID and ENTER ]");
		mvwprintw(win, THIRD_ROW + 1, MODE_COL + 4, ">");
		mvwgetnstr(win, THIRD_ROW + 1, MODE_COL + 7, buf, 17);
		noecho();
		convert_string_to_mac(buf, conf.filterbssid);
		break;

	case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
		i = c - '1';
		if (MAC_NOT_EMPTY(conf.filtermac[i]) && conf.filtermac_enabled[i]) {
			conf.filtermac_enabled[i] = 0;
		}
		else {
			echo();
			print_centered(win, FILTER_WIN_HEIGHT-1, FILTER_WIN_WIDTH,
				       "[ Enter new MAC %d and ENTER ]", i+1);
			mvwprintw(win, THIRD_ROW + 1 + i, MAC_COL + 4, ">");
			mvwgetnstr(win, THIRD_ROW + 1 + i, MAC_COL + 7, buf, 17);
			noecho();
			/* just enable old MAC if user pressed return only */
			if (*buf == '\0' && MAC_NOT_EMPTY(conf.filtermac[i]))
				conf.filtermac_enabled[i] = 1;
			else {
				convert_string_to_mac(buf, conf.filtermac[i]);
				if (MAC_NOT_EMPTY(conf.filtermac[i]))
					conf.filtermac_enabled[i] = true;
			}
		}
		break;

	case '0':
		conf.filter_off = conf.filter_off ? 0 : 1;
		break;

	case '*':
		conf.filter_badfcs = conf.filter_badfcs ? 0 : 1;
		break;

	default:
		for (t = 0; t < WLAN_NUM_TYPES; t++) {
			for (i = 0; i < WLAN_NUM_STYPES; i++) {
				if (stype_names[t][i].c == c) {
					TOGGLE_BIT(conf.filter_stype[t], BIT(i));
					goto out;
				}
			}
		}
		return false; // not found
	}

out:
	/* recalculate filter flag */
	conf.do_macfilter = 0;
	for (i = 0; i < MAX_FILTERMAC; i++) {
		if (conf.filtermac_enabled[i])
			conf.do_macfilter = 1;
	}

	net_send_filter_config();

	update_filter_win(win);
	return true;
}
