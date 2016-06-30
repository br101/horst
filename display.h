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

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <curses.h>

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
#define WHITEONRED	COLOR_PAIR(14)

#define CHECKED(_exp) (_exp) ? '*' : ' '

#define FILTER_WIN_WIDTH	56
#define FILTER_WIN_HEIGHT	35

#define CHANNEL_WIN_WIDTH	41
#define CHANNEL_WIN_HEIGHT	31

struct packet_info;
struct node_info;

void get_per_second(unsigned long bytes, unsigned long duration,
		    unsigned long packets, unsigned long retries,
		    int *bps, int *dps, int *pps, int *rps);
void __attribute__ ((format (printf, 4, 5)))
print_centered(WINDOW* win, int line, int cols, const char *fmt, ...);
int get_packet_type_color(int type);
void signal_average_bar(WINDOW *win, int sig, int siga, int y, int x,
			int height, int width);
void general_average_bar(WINDOW *win, int val, int avg, int y, int x,
			 int width, short color, short color_avg);
void update_display(struct packet_info* pkg);
void update_display_clock(void);
void display_log(const char *string);
void handle_user_input(void);
void init_display(void);
void finish_display(void);
void display_clear(void);

/* main windows are special */
void init_display_main(void);
void clear_display_main(void);
void update_main_win(struct packet_info *pkt);
void update_dump_win(struct packet_info* pkt);
bool main_input(int c);
void print_dump_win(const char *str, int refresh);
void resize_display_main(void);

/* smaller config windows */
void update_filter_win(WINDOW *win);
void update_channel_win(WINDOW *win);
bool filter_input(WINDOW *win, int c);
bool channel_input(WINDOW *win, int c);

/* "standard" windows */
void update_spectrum_win(WINDOW *win);
void update_statistics_win(WINDOW *win);
void update_essid_win(WINDOW *win);
void update_history_win(WINDOW *win);
void update_help_win(WINDOW *win);
bool spectrum_input(WINDOW *win, int c);

#endif
