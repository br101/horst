/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2015 Tuomas Räsänen <tuomasjjrasanen@tjjr.fi>
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

/**
 * ifctrl_iwadd_monitor() - add virtual 802.11 monitor interface
 *
 * @interface: the name of the interface the monitor interface is attached to
 * @monitor_interface: the name of the new monitor interface
 *
 * Return 0 on success, -1 on error.
 */
int ifctrl_iwadd_monitor(const char *interface, const char *monitor_interface);

/**
 * ifctrl_iwdel() - delete 802.11 interface
 *
 * @interface: the name of the interface
 *
 * Return 0 on success, -1 on error.
 */
int ifctrl_iwdel(const char *interface);

/**
 * ifctrl_ifup() - bring interface up
 *
 * @interface: the name of the interface
 *
 * Return 0 on success, -1 on error.
 */
int ifctrl_ifup(const char *interface);

/**
 * ifctrl_ifdown() - take interface down
 *
 * @interface: the name of the interface
 *
 * Return 0 on success, -1 on error.
 */
int ifctrl_ifdown(const char *interface);

/**
 * ifctrl_iwset_monitor() - set 802.11 interface to monitor mode
 *
 * @interface: the name of the interface
 *
 * Return 0 on success, -1 on error.
 */
int ifctrl_iwset_monitor(const char *interface);

#endif
