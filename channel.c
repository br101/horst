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

#include <stdlib.h>

#include "main.h"
#include "util.h"
#include "ifctrl.h"
#include "channel.h"
#include "ieee80211_util.h"
#include "wlan_util.h"


static struct timespec last_channelchange;
static struct channel_list channels;

uint32_t channel_get_remaining_dwell_time(void)
{
	if (!conf.do_change_channel)
		return UINT32_MAX;

	int64_t ret = (int64_t)conf.channel_time
		- (the_time.tv_sec -  last_channelchange.tv_sec) * 1000000
		- (the_time.tv_nsec - last_channelchange.tv_nsec) / 1000;

	if (ret < 0)
		return 0;
	else if (ret > UINT32_MAX)
		return UINT32_MAX;
	else
		return ret;
}


static struct band_info channel_get_band_from_idx(int idx)
{
	int b = idx - channels.band[0].num_channels < 0 ? 0 : 1;
	return channels.band[b];
}


static int get_center_freq_ht40(unsigned int freq, bool upper)
{
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


static int get_center_freq_vht(unsigned int freq, enum chan_width width)
{
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
			printlog("%s is not VHT", channel_width_string(width, -1));
	}
	return center1;
}


const char* channel_width_string(enum chan_width w, int ht40p)
{
	switch (w) {
		case CHAN_WIDTH_UNSPEC: return "?";
		case CHAN_WIDTH_20_NOHT: return "20 (no HT)";
		case CHAN_WIDTH_20: return "HT20";
		case CHAN_WIDTH_40: return ht40p < 0 ? "HT40" : ht40p ? "HT40+" : "HT40-";
		case CHAN_WIDTH_80: return "VHT80";
		case CHAN_WIDTH_160: return "VHT160";
		case CHAN_WIDTH_8080: return "VHT80+80";
	}
	return "";
}

const char* channel_width_string_short(enum chan_width w, int ht40p)
{
	switch (w) {
		case CHAN_WIDTH_UNSPEC: return "?";
		case CHAN_WIDTH_20_NOHT: return "20g";
		case CHAN_WIDTH_20: return "20";
		case CHAN_WIDTH_40: return ht40p < 0 ? "40" : ht40p ? "40+" : "40-";
		case CHAN_WIDTH_80: return "80";
		case CHAN_WIDTH_160: return "160";
		case CHAN_WIDTH_8080: return "80+80";
	}
	return "";
}

/* Note: ht40plus is only used for HT40 channel width, to distinguish between
 * HT40+ and HT40- */
bool channel_change(int idx, enum chan_width width, bool ht40plus)
{
	unsigned int center1 = 0;

	if (width == CHAN_WIDTH_UNSPEC)
		width = channel_get_band_from_idx(idx).max_chan_width;

	switch (width) {
		case CHAN_WIDTH_20_NOHT:
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
			printlog("%s not implemented", channel_width_string(width, -1));
			break;
	}

	/* only 20 MHz channels don't need additional center freq, otherwise we fail here
	 * quietly because the scanning code sometimes tries invalid HT40+/- channels */
	if (center1 == 0 && !(width == CHAN_WIDTH_20_NOHT || width == CHAN_WIDTH_20))
		return false;

	if (!ifctrl_iwset_freq(conf.ifname, channels.chan[idx].freq, width, center1)) {
		printlog("ERROR: Failed to set CH %d (%d MHz) %s center %d",
			channels.chan[idx].chan, channels.chan[idx].freq,
			channel_width_string(width, ht40plus),
			center1);
		return false;
	}

	printlog("Set CH %d (%d MHz) %s center %d after %ldms",
		channels.chan[idx].chan, channels.chan[idx].freq,
		channel_width_string(width, ht40plus),
		 center1, (the_time.tv_sec - last_channelchange.tv_sec) * 1000
		 + (the_time.tv_nsec - last_channelchange.tv_nsec) / 1000000);

	conf.channel_idx = idx;
	conf.channel_width = width;
	conf.channel_ht40plus = ht40plus;
	conf.max_phy_rate = get_phy_thruput(width, channel_get_band_from_idx(idx).streams_rx);
	last_channelchange = the_time;
	return true;
}

bool channel_auto_change(void)
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
					new_idx++;
				conf.channel_ht40plus = !conf.channel_ht40plus; // toggle
			} else {
				new_idx++;
			}

			if (new_idx >= channels.num_channels ||
			    new_idx >= MAX_CHANNELS ||
			    (conf.channel_max &&
			     channel_get_chan(new_idx) > conf.channel_max)) {
				new_idx = 0;
				max_width = channel_get_band_from_idx(new_idx).max_chan_width;
				conf.channel_ht40plus = true;
			}

			ret = channel_change(new_idx, max_width, conf.channel_ht40plus);

		/* try setting different channels in case we get errors only
		 * on some channels (e.g. ipw2200 reports channel 14 but cannot
		 * be set to use it). stop if we tried all channels */
		} while (ret != 1 && new_idx != start_idx);
	}

	return ret;
}

char* channel_get_string(int idx)
{
	static char buf[32];
	struct chan_freq* c = &channels.chan[idx];
	snprintf(buf, sizeof(buf), "%-3d: %d HT40%s%s", c->chan, c->freq,
			get_center_freq_ht40(c->freq, true) ? "+" : "",
			get_center_freq_ht40(c->freq, false) ? "-" : "");
	return buf;
}

bool channel_init(void)
{
	/* get available channels */
	ifctrl_iwget_freqlist(conf.if_phy, &channels);
	conf.channel_initialized = 1;

	printf("Got %d Bands, %d Channels:\n", channels.num_bands, channels.num_channels);
	for (int i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		printf("%s\n", channel_get_string(i));

	if (channels.num_bands <= 0 || channels.num_channels <= 0)
		return false;

	if (conf.channel_set_num > 0) {
		/* configured values */
		printf("Setting configured channel %d\n", conf.channel_set_num);
		int ini_idx = channel_find_index_from_chan(conf.channel_set_num);
		if (!channel_change(ini_idx, conf.channel_set_width, conf.channel_set_ht40plus))
			return false;
	} else {
		if (conf.if_freq <= 0) {
			/* this happens when we have not been able to change
			 * the original interface to monitor mode and we added
			 * an additional monitor (horstX) interface */
			printf("Could not get current channel of interface\n");
			conf.max_phy_rate = get_phy_thruput(channels.band[0].max_chan_width,
							    channels.band[0].streams_rx);
			return true; // not failure

		}
		conf.channel_idx = channel_find_index_from_freq(conf.if_freq);
		conf.channel_set_num = channel_get_chan(conf.channel_idx);

		/* try to set max width */
		struct band_info b = channel_get_band_from_idx(conf.channel_idx);
		if (conf.channel_width != b.max_chan_width) {
			printlog("Try to set max channel width %s",
				channel_width_string(b.max_chan_width, -1));
			// try both HT40+ and HT40- if necessary
			if (!channel_change(conf.channel_idx, b.max_chan_width, true) &&
			    !channel_change(conf.channel_idx, b.max_chan_width, false))
				return false;
		} else {
			conf.channel_set_width = conf.channel_width;
			conf.channel_set_ht40plus = conf.channel_ht40plus;
			conf.max_phy_rate = get_phy_thruput(conf.channel_width, b.streams_rx);
		}
	}
	return true;
}

int channel_find_index_from_chan(int c)
{
	int i = -1;
	for (i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		if (channels.chan[i].chan == c)
			return i;
	return -1;
}

int channel_find_index_from_freq(unsigned int f)
{
	int i = -1;
	for (i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		if (channels.chan[i].freq == f)
			return i;
	return -1;
}

int channel_get_chan(int i)
{
	if (i >= 0 && i < channels.num_channels && i < MAX_CHANNELS)
		return channels.chan[i].chan;
	else
		return -1;
}

int channel_get_freq(int idx)
{
	if (idx >= 0 && idx < channels.num_channels && idx < MAX_CHANNELS)
		return channels.chan[idx].freq;
	else
		return -1;
}

bool channel_list_add(int freq)
{
	if (channels.num_channels >=  MAX_CHANNELS)
		return false;

	channels.chan[channels.num_channels].chan = ieee80211_freq2channel(freq);
	channels.chan[channels.num_channels].freq = freq;
	channels.num_channels++;
	return true;
}

int channel_get_num_channels(void)
{
	return channels.num_channels;
}

int channel_get_num_bands(void)
{
	return channels.num_bands;
}

int channel_get_idx_from_band_idx(int band, int idx)
{
	if (band < 0 || band >= channels.num_bands)
		return -1;

	if (idx < 0 || idx >= channels.band[band].num_channels)
		return -1;

	if (band > 0)
		idx = idx + channels.band[0].num_channels;

	return idx;
}

const struct band_info* channel_get_band(int b)
{
	if (b < 0 || b > channels.num_bands)
		return NULL;
	return &channels.band[b];
}

bool channel_band_add(int num_channels, enum chan_width max_chan_width,
		      unsigned char streams_rx, unsigned char streams_tx)
{
	if (channels.num_bands >= MAX_BANDS)
		return false;

	channels.band[channels.num_bands].num_channels = num_channels;
	channels.band[channels.num_bands].max_chan_width = max_chan_width;
	channels.band[channels.num_bands].streams_rx = streams_rx;
	channels.band[channels.num_bands].streams_tx = streams_tx;
	channels.num_bands++;
	return true;
}
