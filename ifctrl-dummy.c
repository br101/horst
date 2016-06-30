/* horst - Highly Optimized Radio Scanning Tool
 *
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

#include "ifctrl.h"
#include "main.h"

bool ifctrl_init(void)
{
	return true;
};

void ifctrl_finish(void)
{
};

bool ifctrl_iwadd_monitor(__attribute__((unused)) const char *interface,
			  __attribute__((unused))const char *monitor_interface)
{
	printlog("add monitor: not implemented");
	return false;
};

bool ifctrl_iwdel(__attribute__((unused)) const char *interface)
{
	printlog("iwdel: not implemented");
	return false;
};

bool ifctrl_iwset_monitor(__attribute__((unused)) const char *interface)
{
	printlog("set monitor: not implemented");
	return false;
};

bool ifctrl_iwset_freq(__attribute__((unused)) const char *const interface,
		       __attribute__((unused)) unsigned int freq,
		       __attribute__((unused)) enum chan_width width,
		       __attribute__((unused)) unsigned int center1)
{
	printlog("set freq: not implemented");
	return false;
};

bool ifctrl_iwget_interface_info(__attribute__((unused)) const char *interface)
{
	printlog("get interface info: not implemented");
	return false;
};

bool ifctrl_iwget_freqlist(__attribute__((unused)) int phy,
			   __attribute__((unused)) struct channel_list* channels)
{
	printlog("get freqlist: not implemented");
	return false;
};

bool ifctrl_is_monitor(void)
{
	return true;
};
