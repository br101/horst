#include <stdlib.h>

#include "main.h"
#include "util.h"
#include "ieee80211_util.h"
#include "wext.h"


static struct timeval last_channelchange;
extern int mon; /* monitoring socket */

void
auto_change_channel(int mon)
{
	int new_chan;

	if (the_time.tv_sec == last_channelchange.tv_sec &&
	    (the_time.tv_usec - last_channelchange.tv_usec) < conf.channel_time)
		return; /* too early */

	if (conf.do_change_channel) {
		new_chan = conf.current_channel + 1;
		if (new_chan >= conf.num_channels || new_chan >= MAX_CHANNELS ||
		    (conf.channel_max && new_chan >= conf.channel_max))
			new_chan = 0;

		if (wext_set_channel(mon, conf.ifname, channels[new_chan].freq) == 0)
			printlog("auto change channel could not set channel");
		else
			conf.current_channel = new_chan;
	}

	/* also if channel was not changed, keep stats only for every channel_time.
	 * display code uses durations_last to get a more stable view */
	if (conf.current_channel >= 0) {
		spectrum[conf.current_channel].durations_last =
				spectrum[conf.current_channel].durations;
		spectrum[conf.current_channel].durations = 0;
		ewma_add(&spectrum[conf.current_channel].durations_avg,
			 spectrum[conf.current_channel].durations_last);
	}

	last_channelchange = the_time;
}


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


void
get_current_channel(int mon)
{
	int i, freq, ch;

	/* get current channel &  map to our channel array */
	freq = wext_get_freq(mon, conf.ifname);
	if (freq == 0)
		return;

	ch = ieee80211_frequency_to_channel(freq);
	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++)
		if (channels[i].chan == ch)
			break;

	if (i < MAX_CHANNELS)
		conf.current_channel = i;
	DEBUG("***%d\n", conf.current_channel);
}


void
change_channel(int c)
{
	int i;

	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++) {
		if (channels[i].chan == c) {
			if (wext_set_channel(mon, conf.ifname, channels[i].freq) == 0) {
				printlog("ERROR: could not set channel %d",
					 channels[i].chan);
				return;
			}
			conf.current_channel = i;
			break;
		}
	}
}
