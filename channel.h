/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2014 Bruno Randolf (br1@einfach.org)
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

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

int
change_channel(int idx);

int
auto_change_channel(void);

int
find_channel_index(int c);

int
find_channel_index_freq(int f);

void
get_current_channel(int mon);

void
init_channels(void);

#define CHANNEL_IDX_TO_CHAN(_i) (_i > 0 && _i < conf.num_channels && _i < MAX_CHANNELS ? channels[_i].chan : -1 )

#define CONF_CURRENT_CHANNEL (conf.channel_idx >= 0 && conf.channel_idx < MAX_CHANNELS ? \
	channels[conf.channel_idx].chan : 0)

#endif
