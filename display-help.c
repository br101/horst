/******************* HELP *******************/

#include <stdlib.h>

#include "display.h"
#include "main.h"
#include "util.h"


void
update_help_win(WINDOW *win)
{
	int i, l;
	char c;

	werase(win);
	wattron(win, WHITE);
	box(win, 0 , 0);
	print_centered(win, 0, COLS, " Help ");
	print_centered(win, 2, COLS, "HORST - Horsts OLSR Radio Scanning Tool");
	print_centered(win, 3, COLS, "Version " VERSION " (build date " __DATE__ " " __TIME__ ")");

	mvwprintw(win, 5, 2, "(C) 2005-2010 Bruno Randolf, Licensed under the GPLv2");

	mvwprintw(win, 7, 2, "Known IEEE802.11 Packet Types:");
	l = 9;
	/* this is weird but it works */
	mvwprintw(win, l++, 2, "MANAGEMENT FRAMES");
	for (i = 0x00; i <= 0xD0; i = i + 0x10) {
		c = get_packet_type_char(i);
		if (c != '?')
			mvwprintw(win, l++, 4, "%c %s", c, get_packet_type_name(i));
	}
	l = 9;
	mvwprintw(win, l++, 25, "CONTROL FRAMES");
	for (i = 0xa4; i <= 0xF4; i = i + 0x10) {
		c = get_packet_type_char(i);
		if (c != '?')
			mvwprintw(win, l++, 27, "%c %s", c, get_packet_type_name(i));
	}
	l = 9;
	mvwprintw(win, l++, 50, "DATA FRAMES");
	for (i = 0x08; i <+ 0xF8; i = i + 0x10) {
		c = get_packet_type_char(i);
		if (c != '?')
			mvwprintw(win, l++, 52, "%c %s", c, get_packet_type_name(i));
	}

	mvwprintw(win, ++l, 2, "For more info read the README or check http://br1.einfach.org/horst/");

	wrefresh(win);
}
