/* olsr scanning tool
 *
 * Copyright (C) 2005  Bruno Randolf
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

#include "main.h"

void update_display(struct packet_info* pkg);
void handle_user_input();

void dump_packet(const unsigned char* buf, int len);
const char* ether_sprintf(const unsigned char *mac);
const char* ip_sprintf(const unsigned int ip);

void init_display(void);
void finish_display(int sig);

#endif
