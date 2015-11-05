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

#include <stdlib.h>

#include "main.h"
#include "util.h"
#include "ifctrl.h"
#include "channel.h"


static struct timeval last_channelchange;
static struct channel_list channels;

long
channel_get_remaining_dwell_time(void)
{
	if (!conf.do_change_channel)
		return LONG_MAX;

	return conf.channel_time
		- the_time.tv_sec * 1000000
		- the_time.tv_usec
		+ last_channelchange.tv_sec * 1000000
		+ last_channelchange.tv_usec;
}


static struct band_info
channel_get_band_from_idx(int idx) {
	int b = idx - channels.band[0].num_channels < 0 ? 0 : 1;
	return channels.band[b];
}


static int get_center_freq_ht40(unsigned int freq, bool upper) {
	unsigned int center = 0;
	/*
	 * For HT40 we have a channel offset of 20 MHz, and the
	 * center frequency is in the middle: +/- 10 MHz, depending
	 * on HT40+ or HT40- and whether the channel exists
	 */
	if (upper && channel_find_index_from_freq(freq + 20) != -1)
		center = freq + 10;
	else if (!upper && channel_find_index_from_freq(freq - 20) != -1)
		center = freq - 10;
	return center;
}


static int get_center_freq_vht(unsigned int freq, enum chan_width width) {
	unsigned int center1 = 0;
	switch(width) {
		case CHAN_WIDTH_80:
			/*
			 * VHT80 channels are non-overlapping and the primary
			 * channel can be on any HT20/40 channel in the range
			 */
			if (freq >= 5180 && freq <= 5240)
				center1 = 5210;
			else if (freq >= 5260 && freq <= 5320)
				center1 = 5290;
			else if (freq >= 5500 && freq <= 5560)
				center1 = 5530;
			else if (freq >= 5580 && freq <= 5640)
				center1 = 5610;
			else if (freq >= 5660 && freq <= 5720)
				center1 = 5690;
			else if (freq >= 5745 && freq <= 5805)
				center1 = 5775;
			break;
		case CHAN_WIDTH_160:
			/*
			 * There are only two possible VHT160 channels
			 */
			if (freq >= 5180 && freq <= 5320)
				center1 = 5250;
			else if (freq >= 5180 && freq <= 5320)
				center1 = 5570;
			break;
		case CHAN_WIDTH_8080:
			printlog("VHT80+80 not supported");
			break;
		default:
			printlog("%s is not VHT", channel_get_width_string(width));
	}
	return center1;
}


const char* channel_get_width_string(enum chan_width w) {
	switch (w) {
		case CHAN_WIDTH_UNSPEC: return "";
		case CHAN_WIDTH_20: return "HT20";
		case CHAN_WIDTH_40: return "HT40";
		case CHAN_WIDTH_80: return "VHT80";
		case CHAN_WIDTH_160: return "VHT160";
		case CHAN_WIDTH_8080: return "VHT80+80";
	}
	return "";
}

const char* channel_get_width_string_short(enum chan_width w, bool ht40p) {
	switch (w) {
		case CHAN_WIDTH_UNSPEC: return "";
		case CHAN_WIDTH_20: return "20";
		case CHAN_WIDTH_40: return ht40p ? "40+" : "40-";
		case CHAN_WIDTH_80: return "80";
		case CHAN_WIDTH_160: return "160";
		case CHAN_WIDTH_8080: return "80+80";
	}
	return "";
}

/* Note: ht40plus is only used for HT40 channel width, to distinguish between
 * HT40+ and HT40- */
bool
channel_change(int idx, enum chan_width width, bool ht40plus)
{
	unsigned int center1 = 0;

	if (width == CHAN_WIDTH_UNSPEC)
		width = channel_get_band_from_idx(idx).max_chan_width;

	switch (width) {
		case CHAN_WIDTH_20:
			break;
		case CHAN_WIDTH_40:
			center1 = get_center_freq_ht40(channels.chan[idx].freq, ht40plus);
			break;
		case CHAN_WIDTH_80:
		case CHAN_WIDTH_160:
			center1 = get_center_freq_vht(channels.chan[idx].freq, width);
			break;
		default:
			printlog("%s not implemented", channel_get_width_string(width));
			break;
	}

	/* only HT20 does not need additional center freq, otherwise we fail here
	 * quietly because the scanning code sometimes tries invalid HT40+/- channels */
	if (width != CHAN_WIDTH_20 && center1 == 0)
		return false;

	if (!ifctrl_iwset_freq(conf.ifname, channels.chan[idx].freq, width, center1)) {
		printlog("ERROR: Failed to set CH %d (%d MHz) %s%c center %d",
			channels.chan[idx].chan, channels.chan[idx].freq,
			channel_get_width_string(width),
			center1 > channels.chan[idx].freq ? '+' : '-',
			center1);
		return false;
	}

	printlog("Set CH %d (%d MHz) %s%c center %d",
		channels.chan[idx].chan, channels.chan[idx].freq,
		channel_get_width_string(width),
		center1 > channels.chan[idx].freq ? '+' : '-',
		center1);

	conf.channel_idx = idx;
	last_channelchange = the_time;
	return true;
}


bool
channel_auto_change(void)
{
	int new_idx;
	bool ret = true;
	int start_idx;

	if (conf.channel_idx == -1)
		return false; /* The current channel is still unknown for some
			   * reason (mac80211 bug, busy physical interface,
			   * etc.), it will be fixed when the first packet
			   * arrives, see fixup_packet_channel().
			   *
			   * Without this return, horst would busy-loop forever
			   * (making the ui totally unresponsive) in the channel
			   * changing code below because start_idx would be -1
			   * as well. Short-circuit exiting here is quite
			   * logical though: it does not make any sense to scan
			   * channels as long as the channel module is not
			   * initialized properly. */

	if (channel_get_remaining_dwell_time() > 0)
		return false; /* too early */

	if (conf.do_change_channel) {
		start_idx = new_idx = conf.channel_idx;
		do {
			enum chan_width max_width = channel_get_band_from_idx(new_idx).max_chan_width;
			/*
			 * For HT40 we visit the same channel twice, once with HT40+
			 * and once with HT40-. This is necessary to see the HT40+/-
			 * data packets
			 */
			if (max_width == CHAN_WIDTH_40) {
				if (conf.channel_ht40plus)
					new_idx = new_idx + 1;
				conf.channel_ht40plus = !conf.channel_ht40plus; // toggle
			}

			if (new_idx >= channels.num_channels ||
			    new_idx >= MAX_CHANNELS ||
			    (conf.channel_max &&
			     channel_get_chan_from_idx(new_idx) > conf.channel_max))
				new_idx = 0;

			ret = channel_change(new_idx, max_width, conf.channel_ht40plus);

		/* try setting different channels in case we get errors only
		 * on some channels (e.g. ipw2200 reports channel 14 but cannot
		 * be set to use it). stop if we tried all channels */
		} while (ret != 1 && new_idx != start_idx);
	}

	return ret;
}


int
channel_get_current_chan() {
	return channel_get_chan_from_idx(conf.channel_idx);
}

char* channel_get_string(int idx) {
	static char buf[32];
	struct chan_freq* c = &channels.chan[idx];
	snprintf(buf, sizeof(buf), "%-3d: %d HT40%s%s", c->chan, c->freq,
			get_center_freq_ht40(c->freq, true) ? "+" : "",
			get_center_freq_ht40(c->freq, false) ? "-" : "");
	return buf;
}

bool
channel_init(void) {
	/* get available channels */
	ifctrl_iwget_freqlist(conf.if_phy, &channels);

	printf("Got %d Bands, %d Channels:\n", channels.num_bands, channels.num_channels);
	for (int i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		printf("%s\n", channel_get_string(i));

	conf.channel_idx = channel_find_index_from_freq(conf.if_freq);

	if (conf.channel_num_initial > 0) {
		int ini_idx = channel_find_index_from_chan(conf.channel_num_initial);
		if (!channel_change(ini_idx, conf.channel_width, conf.channel_ht40plus))
			return false;
	} else {
		conf.channel_num_initial = channel_get_chan_from_idx(conf.channel_idx);
	}
	conf.channel_initialized = 1;
	return true;
}


int
channel_find_index_from_chan(int c)
{
	int i = -1;
	for (i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		if (channels.chan[i].chan == c)
			return i;
	return -1;
}


int
channel_find_index_from_freq(unsigned int f)
{
	int i = -1;
	for (i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		if (channels.chan[i].freq == f)
			return i;
	return -1;
}


int
channel_get_chan_from_idx(int i) {
	if (i >= 0 && i < channels.num_channels && i < MAX_CHANNELS)
		return channels.chan[i].chan;
	else
		return -1;
}


struct chan_freq*
channel_get_struct(int i) {
	if (i < channels.num_channels && i < MAX_CHANNELS)
		return &channels.chan[i];
	return NULL;
}


bool
channel_list_add(int chan, int freq) {
	if (channels.num_channels >=  MAX_CHANNELS)
		return false;

	channels.chan[channels.num_channels].chan = chan;
	channels.chan[channels.num_channels].freq = freq;
	channels.num_channels++;
	return true;
}

int
channel_get_num_channels() {
	return channels.num_channels;
}

int
channel_get_num_bands() {
	return channels.num_bands;
}

int
channel_get_idx_from_band_idx(int band, int idx) {
	if (band < 0 || band >= channels.num_bands)
		return -1;

	if (idx < 0 || idx >= channels.band[band].num_channels)
		return -1;

	if (band > 0)
		idx = idx + channels.band[0].num_channels;

	return idx;
}

const char* channel_get_band_width_string(int b) {
	if (b < 0 || b > channels.num_bands)
		return NULL;

	return channel_get_width_string(channels.band[b].max_chan_width);
}
