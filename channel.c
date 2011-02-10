/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2011 Bruno Randolf (br1@einfach.org)
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

#include <stdlib.h>

#include "main.h"
#include "util.h"
#include "ieee80211_util.h"
#include "wext.h"


#if defined(__APPLE__)

int
change_channel(int idx)
{
	return 0;
}

int
auto_change_channel(int mon)
{
	return 0;
}

int
find_channel_index(int c)
{
	return -1;
}

void
get_current_channel(int mon)
{
}


#else


static struct timeval last_channelchange;
extern int mon; /* monitoring socket */


int
change_channel(int idx)
{
	if (wext_set_freq(mon, conf.ifname, channels[idx].freq) == 0) {
		printlog("ERROR: could not set channel %d", channels[idx].chan);
		return 0;
	}
	conf.current_channel = idx;
	return 1;
}


int
auto_change_channel(int mon)
{
	int new_chan;
	int ret = 1;
	int start_chan;

	if (the_time.tv_sec == last_channelchange.tv_sec &&
	    (the_time.tv_usec - last_channelchange.tv_usec) < conf.channel_time)
		return 0; /* too early */

	if (conf.do_change_channel) {
		start_chan = new_chan = conf.current_channel;
		do {
			new_chan = new_chan + 1;
			if (new_chan >= conf.num_channels ||
			    new_chan >= MAX_CHANNELS ||
			    (conf.channel_max && new_chan >= conf.channel_max))
				new_chan = 0;

			ret = change_channel(new_chan);

		/* try setting different channels in case we get errors only
		 * on some channels (e.g. ipw2200 reports channel 14 but cannot
		 * be set to use it). stop if we tried all channels */
		} while (ret != 1 && new_chan != start_chan);
	}

	last_channelchange = the_time;
	return ret;
}


int
find_channel_index(int c)
{
	int i = -1;
	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++)
		if (channels[i].chan == c)
			return i;
	return -1;
}


void
get_current_channel(int mon)
{
	int freq, ch;

	/* get current channel &  map to our channel array */
	freq = wext_get_freq(mon, conf.ifname);
	if (freq == 0)
		return;

	ch = ieee80211_frequency_to_channel(freq);
	ch = find_channel_index(ch);

	if (ch >= 0)
		conf.current_channel = ch;
	DEBUG("***%d\n", conf.current_channel);
}

#endif


void
init_channels(void)
{
	int i;

	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++) {
		INIT_LIST_HEAD(&spectrum[i].nodes);
		ewma_init(&spectrum[i].signal_avg, 1024, 8);
		ewma_init(&spectrum[i].durations_avg, 1024, 8);
	}
}
