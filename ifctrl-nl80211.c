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

#define _GNU_SOURCE	/* necessary for libnl-tiny */

#include <errno.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <netlink/attr.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>

#include <linux/nl80211.h>

#include "ifctrl.h"
#include "main.h"
#include "ieee80211_util.h"

#ifndef NL80211_GENL_NAME
#define NL80211_GENL_NAME "nl80211"
#endif

static struct nl_sock *sock       = NULL;
static struct nl_cache *cache	   = NULL;
static struct genl_family *family = NULL;

bool ifctrl_init() {
	int err = -1;
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

	return true;
out:
	genl_family_put(family);
	nl_cache_free(cache);
	nl_socket_free(sock);
	return false;
}

void ifctrl_finish() {
	nl_socket_free(sock);
	genl_family_put(family);
	nl_cache_free(cache);
}

static bool ifctrl_nl_prepare(struct nl_msg **const msgp,
                             const enum nl80211_commands cmd,
                             const char *const interface)
{
	struct nl_msg *msg = NULL;

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "%s\n", "failed to allocate netlink message");
		return false;
	}

	if (!genlmsg_put(msg, 0, 0, genl_family_get_id(family), 0, 0 /*flags*/, cmd, 0)) {
		fprintf(stderr, "%s\n", "failed to add generic netlink headers");
		goto err;
	}

	if (interface) { //TODO: PHY commands don't need interface name but wiphy index
		unsigned int if_index = if_nametoindex(interface);
		int err;
		if (!if_index) {
			fprintf(stderr, "interface %s does not exist\n", interface);
			goto err;
		}

		err = nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
		if (err) {
			nl_perror(err, "failed to add ifindex to netlink message");
			goto err;
		}
	}

	*msgp = msg;
	return true;

err:
	nlmsg_free(msg);
	return false;
}

static bool ifctrl_nl_send(struct nl_sock *const sock, struct nl_msg *const msg)
{
	int err;

	err = nl_send_auto_complete(sock, msg);
	nlmsg_free(msg);

	if (err > 0)
		err = nl_wait_for_ack(sock);

	if (!err)
		return true;

	nl_perror(err, "failed to send a netlink message");
	return false;
}

static int nl80211_ack_cb(__attribute__((unused)) struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0; /* set "ACK" */
	return NL_STOP;
}

static int nl80211_finish_cb(__attribute__((unused)) struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0; /* set "ACK" */
	return NL_SKIP;
}

static int nl80211_err_cb(__attribute__((unused)) struct sockaddr_nl *nla,
			  struct nlmsgerr *nlerr, __attribute__((unused)) void *arg)
{
	int *ret = arg;
	*ret = nlerr->error; /* set error code */
	return NL_STOP;
}

static bool ifctrl_nl_send_recv(struct nl_sock *const sock, struct nl_msg *const msg,
			   int (*cb_func)(struct nl_msg *, void *), void* cb_arg)
{
	int err;
	struct nl_cb *cb;

	err = nl_send_auto_complete(sock, msg);
	nlmsg_free(msg);

	if (err < 0) {
		nl_perror(err, "failed to send netlink message");
		return false;
	}

	/* set up callback */
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb_func != NULL)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, cb_func, cb_arg);

	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, nl80211_ack_cb, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, nl80211_finish_cb, &err);
	nl_cb_err(cb, NL_CB_CUSTOM, nl80211_err_cb, &err);

	/*
	 * wait for reply message *and* ACK, or error
	 *
	 * Note that err is set by the handlers above. This is done because we
	 * receive two netlink messages, one with the result (and handled by
	 * cb_func) and another one with ACK. We are only done when we received
	 * the ACK or an error!
	 */
	err = 1;
	while (err > 0)
		nl_recvmsgs(sock, cb);

	nl_cb_put(cb);

	return err < 0 ? false : true;
}

bool ifctrl_iwadd_monitor(const char *const interface,
                         const char *const monitor_interface)
{
	struct nl_msg *msg;
	int err;

	if (!ifctrl_nl_prepare(&msg, NL80211_CMD_NEW_INTERFACE, interface))
		return false;

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

	return ifctrl_nl_send(sock, msg); /* frees msg */
err:
	nlmsg_free(msg);
	return false;
}

bool ifctrl_iwdel(const char *const interface)
{
	struct nl_msg *msg;

	if (!ifctrl_nl_prepare(&msg, NL80211_CMD_DEL_INTERFACE, interface))
		return false;

	return ifctrl_nl_send(sock, msg); /* frees msg */
}

bool ifctrl_iwset_monitor(const char *const interface)
{
	struct nl_msg *msg;
	int err;

	if (!ifctrl_nl_prepare(&msg, NL80211_CMD_SET_INTERFACE, interface))
		return false;

	err = nla_put_u32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);
	if (err) {
		nl_perror(err, "failed to add interface type attribute to a "
                          "netlink message");
		nlmsg_free(msg);
		return false;
	}

	return ifctrl_nl_send(sock, msg); /* frees msg */
}

bool ifctrl_iwset_freq(const char *const interface, unsigned int freq)
{
	struct nl_msg *msg;
	int err;

	if (!ifctrl_nl_prepare(&msg, NL80211_CMD_SET_WIPHY, interface))
		return false;

	err = nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	err = nla_put_u32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, NL80211_CHAN_NO_HT);

	if (err) {
		nl_perror(err, "failed to add interface type attribute to a "
                          "netlink message");
		nlmsg_free(msg);
		return false;
	}

	return ifctrl_nl_send(sock, msg); /* frees msg */
}

static struct nlattr** nl80211_parse(struct nl_msg *msg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	static struct nlattr *attr[NL80211_ATTR_MAX + 1];

	nla_parse(attr, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
	          genlmsg_attrlen(gnlh, 0), NULL);

	return attr;
}

static int nl80211_get_interface_info_cb(struct nl_msg *msg,
					 __attribute__((unused)) void *arg)
{
	struct nlattr **tb = nl80211_parse(msg);

	if (tb[NL80211_ATTR_WIPHY_FREQ])
		conf.if_freq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);

	if (tb[NL80211_ATTR_IFTYPE])
		conf.if_type = nla_get_u32(tb[NL80211_ATTR_IFTYPE]);

	if (tb[NL80211_ATTR_WIPHY])
		conf.if_phy = nla_get_u32(tb[NL80211_ATTR_WIPHY]);

	return NL_SKIP;
}

bool ifctrl_iwget_interface_info(const char *const interface)
{
	struct nl_msg *msg;
	bool ret;

	if (!ifctrl_nl_prepare(&msg, NL80211_CMD_GET_INTERFACE, interface))
		return false;

	ret = ifctrl_nl_send_recv(sock, msg, nl80211_get_interface_info_cb, NULL); /* frees msg */
	if (!ret)
		fprintf(stderr, "failed to get interface info");
	return ret;
}

static int nl80211_get_freqlist_cb(struct nl_msg *msg, void *arg)
{
	int bands_remain, freqs_remain, i = 0;

	struct nlattr **attr = nl80211_parse(msg);
	struct nlattr *bands[NL80211_BAND_ATTR_MAX + 1];
	struct nlattr *freqs[NL80211_FREQUENCY_ATTR_MAX + 1];
	struct nlattr *band, *freq;

	struct chan_freq* chan = arg;

	nla_for_each_nested(band, attr[NL80211_ATTR_WIPHY_BANDS], bands_remain)
	{
		nla_parse(bands, NL80211_BAND_ATTR_MAX,
		          nla_data(band), nla_len(band), NULL);

		nla_for_each_nested(freq, bands[NL80211_BAND_ATTR_FREQS], freqs_remain)
		{
			nla_parse(freqs, NL80211_FREQUENCY_ATTR_MAX,
			          nla_data(freq), nla_len(freq), NULL);

			if (!freqs[NL80211_FREQUENCY_ATTR_FREQ] ||
			    freqs[NL80211_FREQUENCY_ATTR_DISABLED])
				continue;

			chan[i].freq = nla_get_u32(freqs[NL80211_FREQUENCY_ATTR_FREQ]);
			chan[i].chan = ieee80211_freq2channel(chan[i].freq);

			if (++i >= MAX_CHANNELS)
				return NL_SKIP;
		}
	}

	conf.num_channels = i;

	return NL_SKIP;
}

bool ifctrl_iwget_freqlist(int phy, struct chan_freq chan[MAX_CHANNELS])
{
	struct nl_msg *msg;
	bool ret;

	if (!ifctrl_nl_prepare(&msg, NL80211_CMD_GET_WIPHY, NULL))
		return false;

	nla_put_u32(msg, NL80211_ATTR_WIPHY, phy);

	ret = ifctrl_nl_send_recv(sock, msg, nl80211_get_freqlist_cb, chan); /* frees msg */
	if (!ret)
		fprintf(stderr, "failed to get freqlist");
	return ret;
}

bool ifctrl_is_monitor() {
	return conf.if_type == NL80211_IFTYPE_MONITOR;
}
