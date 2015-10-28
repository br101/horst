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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>

#include "main.h"
#include "util.h"

extern int mon; /* monitoring socket */

bool
wext_set_freq(int fd, const char* devname, int freq)
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


int
wext_get_freq(int fd, const char* devname)
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


int
wext_get_channels(int fd, const char* devname,
		  struct chan_freq channels[MAX_CHANNELS])
{
	struct iwreq iwr;
	struct iw_range range;
	int i;

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

	if(range.we_version_compiled < 16) {
		printlog("ERROR: wext version %d too old to get channels",
			 range.we_version_compiled);
		return 0;
	}

	for(i = 0; i < range.num_frequency && i < MAX_CHANNELS; i++) {
		DEBUG("  Channel %.2d: %dMHz\n", range.freq[i].i, range.freq[i].m);
		channels[i].chan = range.freq[i].i;
		/* different drivers return different frequencies
		 * (e.g. ipw2200 vs mac80211) try to fix them up here */
		if (range.freq[i].m > 100000000)
			channels[i].freq = range.freq[i].m / 100000;
		else
			channels[i].freq = range.freq[i].m;
	}
	return range.num_frequency;
}


/*
 * ifctrl.h implementation
 */

bool ifctrl_init() {
	return true;
};

void ifctrl_finish() {
};

bool ifctrl_iwadd_monitor(__attribute__((unused)) const char *interface,
			 __attribute__((unused)) const char *monitor_interface) {
	printlog("add monitor: not supported with WEXT");
	return false;
}

bool ifctrl_iwdel(__attribute__((unused)) const char *interface) {
	printlog("del: not supported with WEXT");
	return false;
}

bool ifctrl_iwset_monitor(__attribute__((unused)) const char *interface) {
	printlog("set monitor: not supported with WEXT");
	return false;
}

bool ifctrl_iwset_freq(const char *interface, unsigned int freq) {
	if (wext_set_freq(mon, interface, freq))
		return true;
	return false;
}

bool ifctrl_iwget_interface_info(const char *interface) {
	conf.if_freq = wext_get_freq(mon, interface);
	if (conf.if_freq == 0)
		return false;
	return true;
}

bool ifctrl_iwget_freqlist(__attribute__((unused)) int phy, struct chan_freq* chan) {
	conf.num_channels = wext_get_channels(mon, conf.ifname, chan);
	if (conf.num_channels)
		return true;
	return false;
}

bool ifctrl_is_monitor() {
	return true; /* assume yes */
}
