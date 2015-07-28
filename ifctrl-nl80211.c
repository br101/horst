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

#include <errno.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <netlink/attr.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>

#include <linux/nl80211.h>

#include "ifctrl.h"

static int ifctrl_nl_prepare(struct nl_sock **const sockp,
                             struct nl_msg **const msgp,
                             const enum nl80211_commands cmd,
                             const char *const interface)
{
	struct nl_msg *msg	   = NULL;
	struct nl_sock *sock       = NULL;
	struct nl_cache *cache	   = NULL;
	struct genl_family *family = NULL;
	int err		           = -1;
	unsigned int if_index;

	if_index = if_nametoindex(interface);
	if (!if_index) {
		fprintf(stderr, "interface %s does not exist\n", interface);
		goto out;
	}

	sock = nl_socket_alloc();
	if (!sock) {
		fprintf(stderr, "%s\n", "failed to allocate a netlink socket");
		goto out;
	}

	err = genl_connect(sock);
	if (err) {
		nl_perror(err, "failed to make a generic netlink connection");
		goto out;
	}

	err = genl_ctrl_alloc_cache(sock, &cache);
	if (err) {
		nl_perror(err, "failed to allocate a netlink controller cache");
		goto out;
	}

	family = genl_ctrl_search_by_name(cache, NL80211_GENL_NAME);
	if (!family) {
		fprintf(stderr, "%s\n", "failed to find a generic netlink "
                        "family object");
		goto out;
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "%s\n", "failed to allocate a netlink message");
		goto out;
	}

	if (!genlmsg_put(msg, 0, 0, genl_family_get_id(family), 0, 0, cmd, 0)) {
		fprintf(stderr, "%s\n", "failed to add generic netlink headers "
                        "to a nelink message");
		goto out;
	}

	err = nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
	if (err) {
		nl_perror(err, "failed to add interface index attribute to a "
                          "netlink message");
		goto out;
	}

	err = 0;
out:
	genl_family_put(family);
	nl_cache_free(cache);

	if (err) {
		nlmsg_free(msg);
		nl_socket_free(sock);
		return -1;
	}

	*sockp = sock;
	*msgp = msg;

	return 0;
}

static int ifctrl_nl_send(struct nl_sock *const sock, struct nl_msg *const msg)
{
	int err;

	err = nl_send_sync(sock, msg); /* frees nl_msg */
	nl_socket_free(sock);

	if (!err)
		return 0;

	nl_perror(err, "failed to send a netlink message");
	return -1;
}

int ifctrl_iwadd_monitor(const char *const interface, 
                         const char *const monitor_interface)
{
	struct nl_sock *sock;
	struct nl_msg *msg;
	int err;

	if (ifctrl_nl_prepare(&sock, &msg, NL80211_CMD_NEW_INTERFACE, interface))
		return -1;

	err = nla_put_string(msg, NL80211_ATTR_IFNAME, monitor_interface);
	if (err) {
		nl_perror(err, "failed to add interface name attribute to a "
                          "netlink message");
		goto err;
	}

	err = nla_put_u32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);
	if (err) {
		nl_perror(err, "failed to add interface type attribute to a "
                          "netlink message");
		goto err;
	}

	return ifctrl_nl_send(sock, msg); /* frees sock and msg */
err:
	nlmsg_free(msg);
	nl_socket_free(sock);
	return -1;
}

int ifctrl_iwdel(const char *const interface)
{
	struct nl_sock *sock;
	struct nl_msg *msg;

	if (ifctrl_nl_prepare(&sock, &msg, NL80211_CMD_DEL_INTERFACE, interface))
		return -1;

	return ifctrl_nl_send(sock, msg); /* frees sock and msg */
}

static int ifctrl_ifupdown(const char *const interface, char up)
{
	int fd;
	struct ifreq ifreq;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		return -1;

	memset(&ifreq, 0, sizeof(ifreq));
	strncpy(ifreq.ifr_name, interface, IF_NAMESIZE - 1);

	if (ioctl(fd, SIOCGIFFLAGS, &ifreq) == -1) {
		int orig_errno = errno;
		close(fd);
		errno = orig_errno;
		return -1;
	}

	if (up)
		ifreq.ifr_flags |= IFF_UP;
	else
		ifreq.ifr_flags &= ~IFF_UP;

	if (ioctl(fd, SIOCSIFFLAGS, &ifreq) == -1) {
		int orig_errno = errno;
		close(fd);
		errno = orig_errno;
		return -1;
	}

	if (close(fd))
		return -1;

	return 0;
}

int ifctrl_ifdown(const char *const interface)
{
	return ifctrl_ifupdown(interface, 0);
}

int ifctrl_ifup(const char *const interface)
{
	return ifctrl_ifupdown(interface, 1);
}

int ifctrl_iwset_monitor(const char *const interface)
{
	struct nl_sock *sock;
	struct nl_msg *msg;
	int err;

	if (ifctrl_nl_prepare(&sock, &msg, NL80211_CMD_SET_INTERFACE, interface))
		return -1;

	err = nla_put_u32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);
	if (err) {
		nl_perror(err, "failed to add interface type attribute to a "
                          "netlink message");
		nlmsg_free(msg);
		nl_socket_free(sock);
		return -1;
	}

	return ifctrl_nl_send(sock, msg); /* frees msg and sock */
}
