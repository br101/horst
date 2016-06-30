/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2015 Tuomas Räsänen <tuomasjjrasanen@tjjr.fi>
 * Copyright (C) 2015-2016 Bruno Randolf <br1@einfach.org>
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

#ifndef IFCTRL_H
#define IFCTRL_H

#include <stdbool.h>
#include "channel.h"

bool ifctrl_init();
void ifctrl_finish();

/**
 * ifctrl_iwadd_monitor() - add virtual 802.11 monitor interface
 *
 * @interface: the name of the interface the monitor interface is attached to
 * @monitor_interface: the name of the new monitor interface
 *
 * Return true on success, false on error.
 */
bool ifctrl_iwadd_monitor(const char *interface, const char *monitor_interface);

/**
 * ifctrl_iwdel() - delete 802.11 interface
 *
 * @interface: the name of the interface
 *
 * Return true on success, false on error.
 */
bool ifctrl_iwdel(const char *interface);

/**
 * ifctrl_flags() - set interface flags: up/down, promisc
 *
 * @interface: the name of the interface
 * @up: up or down
 * @promisc: promiscuous mode or not
 *
 * Return true on success, false on error.
 */
bool ifctrl_flags(const char *interface, bool up, bool promisc);

/**
 * ifctrl_iwset_monitor() - set 802.11 interface to monitor mode
 *
 * @interface: the name of the interface
 *
 * Return true on success, false on error.
 */
bool ifctrl_iwset_monitor(const char *interface);

bool ifctrl_iwset_freq(const char *const interface, unsigned int freq,
		       enum chan_width width, unsigned int center1);

bool ifctrl_iwget_interface_info(const char *interface);

bool ifctrl_iwget_freqlist(int phy, struct channel_list* channels);

bool ifctrl_is_monitor();

#endif
