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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>

#include "main.h"
#include "util.h"

extern int mon; /* monitoring socket */

static bool wext_set_freq(int fd, const char* devname, int freq)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, devname, IFNAMSIZ - 1);
	iwr.ifr_name[IFNAMSIZ - 1] = '\0';
	freq *= 100000;
	iwr.u.freq.m = freq;
	iwr.u.freq.e = 1;

	if (ioctl(fd, SIOCSIWFREQ, &iwr) < 0) {
		printlog("ERROR: wext set channel");
		return false;
	}
	return true;
}

static int wext_get_freq(int fd, const char* devname)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, devname, IFNAMSIZ - 1);
	iwr.ifr_name[IFNAMSIZ - 1] = '\0';

	if (ioctl(fd, SIOCGIWFREQ, &iwr) < 0)
		return 0;

	DEBUG("FREQ %d %d\n", iwr.u.freq.m, iwr.u.freq.e);

	return iwr.u.freq.m;
}

static int wext_get_channels(int fd, const char* devname,
		      struct channel_list* channels)
{
	struct iwreq iwr;
	struct iw_range range;
	int i;
	int band0cnt = 0;
	int band1cnt = 0;

	memset(&iwr, 0, sizeof(iwr));
	memset(&range, 0, sizeof(range));

	strncpy(iwr.ifr_name, devname, IFNAMSIZ - 1);
	iwr.ifr_name[IFNAMSIZ - 1] = '\0';
	iwr.u.data.pointer = (caddr_t) &range;
	iwr.u.data.length = sizeof(range);
	iwr.u.data.flags = 0;

	if (ioctl(fd, SIOCGIWRANGE, &iwr) < 0) {
		printlog("ERROR: wext get channel list");
		return 0;
	}

	if (range.we_version_compiled < 16) {
		printlog("ERROR: wext version %d too old to get channels",
			 range.we_version_compiled);
		return 0;
	}

	for (i = 0; i < range.num_frequency && i < MAX_CHANNELS; i++) {
		DEBUG("  Channel %.2d: %dMHz\n", range.freq[i].i, range.freq[i].m);
		channels->chan[i].chan = range.freq[i].i;
		/* different drivers return different frequencies
		 * (e.g. ipw2200 vs mac80211) try to fix them up here */
		if (range.freq[i].m > 100000000)
			channels->chan[i].freq = range.freq[i].m / 100000;
		else
			channels->chan[i].freq = range.freq[i].m;
		if (channels->chan[i].freq <= 2500)
			band0cnt++;
		else
			band1cnt++;
	}
	channels->num_channels = i;
	channels->num_bands = band1cnt > 0 ? 2 : 1;
	channels->band[0].num_channels = band0cnt;
	channels->band[1].num_channels = band1cnt;
	return i;
}

/*
 * ifctrl.h implementation
 */

bool ifctrl_init(void)
{
	return true;
};

void ifctrl_finish(void)
{
};

bool ifctrl_iwadd_monitor(__attribute__((unused)) const char *interface,
			 __attribute__((unused)) const char *monitor_interface)
{
	printlog("add monitor: not supported with WEXT");
	return false;
}

bool ifctrl_iwdel(__attribute__((unused)) const char *interface)
{
	printlog("del: not supported with WEXT");
	return false;
}

bool ifctrl_iwset_monitor(__attribute__((unused)) const char *interface)
{
	printlog("set monitor: not supported with WEXT");
	return false;
}


bool ifctrl_iwset_freq(const char *const interface,
		       unsigned int freq,
		       __attribute__((unused)) enum chan_width width,
		       __attribute__((unused)) unsigned int center1)
{
	if (wext_set_freq(mon, interface, freq))
		return true;
	return false;
}

bool ifctrl_iwget_interface_info(const char *interface)
{
	conf.if_freq = wext_get_freq(mon, interface);
	if (conf.if_freq == 0)
		return false;
	return true;
}

bool ifctrl_iwget_freqlist(__attribute__((unused)) int phy, struct channel_list* channels)
{
	if (wext_get_channels(mon, conf.ifname, channels))
		return true;
	return false;
}

bool ifctrl_is_monitor(void)
{
	return true; /* assume yes */
}
