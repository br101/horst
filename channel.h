/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2015 Bruno Randolf (br1@einfach.org)
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

#include <stdbool.h>

#define MAX_BANDS		2
#define MAX_CHANNELS		64

/* channel to frequency mapping */
struct chan_freq {
	int chan;
	unsigned int freq;
};

struct band_info {
	int num_channels;
};

struct channel_list {
	struct chan_freq chan[MAX_CHANNELS];
	int num_channels;
	struct band_info band[MAX_BANDS];
	int num_bands;
};

bool
channel_change(int idx);

bool
channel_auto_change(void);

int
channel_find_index_from_chan(int c);

int
channel_find_index_from_freq(unsigned int f);

void
get_current_channel(int mon);

int
channel_get_chan_from_idx(int idx);

int
channel_get_current_chan();

int
channel_get_num_channels();

void
channel_set_num_channels(int i);

bool
channel_init(void);

struct chan_freq*
channel_get_struct(int idx);

void
channel_set(int idx, int chan, int freq);

long
channel_get_remaining_dwell_time(void);

#endif
