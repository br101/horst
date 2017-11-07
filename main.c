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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <net/if.h>

#include <uwifi/packet_sock.h>
#include <uwifi/util.h>
#include <uwifi/wlan_util.h>
#include <uwifi/channel.h>
#include <uwifi/node.h>
#include <uwifi/ifctrl.h>
#include <uwifi/raw_parser.h>
#include <uwifi/netdev.h>
#include <uwifi/essid.h>
#include <uwifi/log.h>

#include "main.h"
#include "hutil.h"
#include "network.h"
#include "display.h"
#include "control.h"
#include "conf_options.h"
#include "ieee80211_duration.h"
#include "protocol_parser.h"

struct list_head essids;
struct history hist;
struct statistics stats;
struct channel_info spectrum[MAX_CHANNELS];
struct node_names_info node_names;

struct config conf;

struct timespec time_mono;
struct timespec time_real;

static FILE* DF = NULL;

/* receive packet buffer
 *
 * due to the way we receive packets the network (TCP connection) we have to
 * expect the reception of partial packet as well as the reception of several
 * packets at one. thus we implement a buffered receive where partially received
 * data stays in the buffer.
 *
 * we need two buffers: one for packet capture or receiving from the server and
 * another one for data the clients sends to the server.
 *
 * not sure if this is also an issue with local packet capture, but it is not
 * implemented there.
 *
 * size: max 80211 frame (2312) + space for prism2 header (144)
 * or radiotap header (usually only 26) + some extra */
static unsigned char buffer[2312 + 200];
static size_t buflen;

/* for packets from client to server */
static unsigned char cli_buffer[500];
static size_t cli_buflen;

/* for select */
static fd_set read_fds;
static fd_set write_fds;
static fd_set excpt_fds;

static volatile sig_atomic_t is_sigint_caught;

void __attribute__ ((format (printf, 2, 3)))
log_out(enum loglevel level, const char *fmt, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(&buf[1], 127, fmt, ap);
	va_end(ap);

	if (conf.quiet || conf.debug || !conf.display_initialized)
		fprintf(stderr, "%s\n", &buf[1]);
	else {
		/* fix up string for display log */
		buf[0] = '\n';
		display_log(level, buf);
	}
}

static void update_history(struct uwifi_packet* p)
{
	if (p->phy_signal == 0)
		return;

	hist.signal[hist.index] = p->phy_signal;
	hist.rate[hist.index] = p->phy_rate;
	hist.type[hist.index] = (p->phy_flags & PHY_FLAG_BADFCS) ? 1 : p->wlan_type;
	hist.retry[hist.index] = p->wlan_retry;

	hist.index++;
	if (hist.index == MAX_HISTORY)
		hist.index = 0;
}

static void update_statistics(struct uwifi_packet* p)
{
	int type = (p->phy_flags & PHY_FLAG_BADFCS) ? 1 : p->wlan_type;

	if (p->phy_rate_idx == 0)
		return;

	stats.packets++;
	stats.bytes += p->wlan_len;
	if (p->wlan_retry)
		stats.retries++;

	if (p->phy_rate_idx > 0 && p->phy_rate_idx < MAX_RATES) {
		stats.duration += p->pkt_duration;
		stats.packets_per_rate[p->phy_rate_idx]++;
		stats.bytes_per_rate[p->phy_rate_idx] += p->wlan_len;
		stats.duration_per_rate[p->phy_rate_idx] += p->pkt_duration;
	}

	if (type >= 0 && type < MAX_FSTYPE) {
		stats.packets_per_type[type]++;
		stats.bytes_per_type[type] += p->wlan_len;
		if (p->phy_rate_idx > 0 && p->phy_rate_idx < MAX_RATES)
			stats.duration_per_type[type] += p->pkt_duration;
	}
}

static void update_spectrum(struct uwifi_packet* p, struct uwifi_node* n)
{
	struct channel_info* chan;
	struct chan_node* cn;

	if (p->pkt_chan_idx < 0)
		return; /* chan not found */

	chan = &spectrum[p->pkt_chan_idx];
	chan->signal = p->phy_signal;
	chan->packets++;
	chan->bytes += p->wlan_len;
	chan->durations += p->pkt_duration;
	ewma_add(&chan->signal_avg, -chan->signal);

	if (!n) {
		LOG_DBG("spec no node");
		return;
	}

	/* add node to channel if not already there */
	list_for_each(&chan->nodes, cn, chan_list) {
		if (cn->node == n) {
			LOG_DBG("SPEC node found %p", cn->node);
			break;
		}
	}
	if (cn->node != n) {
		LOG_DBG("SPEC node adding %p", n);
		cn = malloc(sizeof(struct chan_node));
		cn->node = n;
		cn->chan = chan;
		ewma_init(&cn->sig_avg, 1024, 8);
		list_add_tail(&chan->nodes, &cn->chan_list);
		list_add_tail(&n->on_channels, &cn->node_list);
		chan->num_nodes++;
		n->num_on_channels++;
	}
	/* keep signal of this node as seen on this channel */
	cn->sig = p->phy_signal;
	ewma_add(&cn->sig_avg, -cn->sig);
	cn->packets++;
}

void update_spectrum_durations(void)
{
	/* also if channel was not changed, keep stats only for every channel_time.
	 * display code uses durations_last to get a more stable view */
	if (conf.intf.channel_idx >= 0) {
		spectrum[conf.intf.channel_idx].durations_last =
				spectrum[conf.intf.channel_idx].durations;
		spectrum[conf.intf.channel_idx].durations = 0;
		ewma_add(&spectrum[conf.intf.channel_idx].durations_avg,
			 spectrum[conf.intf.channel_idx].durations_last);
	}
}

static void write_to_file(struct uwifi_packet* p)
{
	char buf[40];
	int i;
	struct tm* ltm = localtime(&time_real.tv_sec);

	//timestamp, e.g. 2015-05-16 15:05:44.338806 +0300
	i = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
	i += snprintf(buf + i, sizeof(buf) - i, ".%06ld", (long)(time_real.tv_nsec / 1000));
	i += strftime(buf + i, sizeof(buf) - i, " %z", ltm);
	fprintf(DF, "%s, ", buf);

	fprintf(DF, "%s, " MAC_FMT ", ",
		wlan_get_packet_type_name(p->wlan_type), MAC_PAR(p->wlan_src));
	fprintf(DF, MAC_FMT ", ", MAC_PAR(p->wlan_dst));
	fprintf(DF, MAC_FMT ", ", MAC_PAR(p->wlan_bssid));
	fprintf(DF, "%x, %d, %d, %d, %d, ",
		p->pkt_types, p->phy_signal, p->wlan_len, p->phy_rate, p->phy_freq);
	fprintf(DF, "%016llx, ", (unsigned long long)p->wlan_tsf);
	fprintf(DF, "%s, %d, %d, %d, %d, %d, ",
		p->wlan_essid, p->wlan_mode, p->wlan_channel,
		p->wlan_wep, p->wlan_wpa, p->wlan_rsn);
	fprintf(DF, "%s, %s\n", ip_sprintf(p->ip_src), ip_sprintf(p->ip_dst));
	fflush(DF);
}

/* return true if packet is filtered */
static bool filter_packet(struct uwifi_packet* p)
{
	int i;

	if (conf.filter_off)
		return false;

	/* if packets with bad FCS are not filtered, still we can not trust any
	 * other header, so in any case return */
	if (p->phy_flags & PHY_FLAG_BADFCS) {
		if (!conf.filter_badfcs) {
			stats.filtered_packets++;
			return true;
		}
		return false;
	}

	/* filter by WLAN frame type and also type 3 which is not defined */
	i = WLAN_FRAME_TYPE(p->wlan_type);
	if (i == 3 || !(conf.filter_stype[i] & BIT(WLAN_FRAME_STYPE(p->wlan_type)))) {
		stats.filtered_packets++;
		return true;
	}

	/* filter by MODE (AP, IBSS, ...) this also filters packets where we
	 * cannot associate a mode (ACK, RTS/CTS) */
	if (conf.filter_mode != WLAN_MODE_ALL && ((p->wlan_mode & ~conf.filter_mode) || p->wlan_mode == 0)) {
		stats.filtered_packets++;
		return true;
	}

	/* filter higher level packet types */
	if (conf.filter_pkt != PKT_TYPE_ALL && (p->pkt_types & ~conf.filter_pkt)) {
		stats.filtered_packets++;
		return true;
	}

	/* filter BSSID */
	if (MAC_NOT_EMPTY(conf.filterbssid) &&
	    memcmp(p->wlan_bssid, conf.filterbssid, WLAN_MAC_LEN) != 0) {
		stats.filtered_packets++;
		return true;
	}

	/* filter MAC adresses */
	if (conf.do_macfilter) {
		for (i = 0; i < MAX_FILTERMAC; i++) {
			if (MAC_NOT_EMPTY(p->wlan_src) &&
			    conf.filtermac_enabled[i] &&
			    memcmp(p->wlan_src, conf.filtermac[i], WLAN_MAC_LEN) == 0) {
				return false;
			}
		}
		stats.filtered_packets++;
		return true;
	}

	return false;
}

void handle_packet(struct uwifi_packet* p)
{
	struct uwifi_node* n = NULL;

	/* filter on server side only */
	if (conf.serveraddr[0] == '\0' && filter_packet(p)) {
		if (!conf.quiet && !conf.paused && !conf.debug)
			update_display_clock();
		return;
	}

	uwifi_fixup_packet_channel(p, &conf.intf);

	if (cli_fd != -1)
		net_send_packet(p);

	if (conf.dumpfile[0] != '\0' && !conf.paused && DF != NULL)
		write_to_file(p);

	if (conf.paused)
		return;

	/* we can't trust any fields except phy_* of packets with bad FCS */
	if (!(p->phy_flags & PHY_FLAG_BADFCS)) {
		LOG_DBG("handle %s " MAC_FMT " (" MAC_FMT ")",
			wlan_get_packet_type_name(p->wlan_type),
			MAC_PAR(p->wlan_src), MAC_PAR(p->wlan_bssid));

		n = uwifi_node_update(p, &conf.intf.wlan_nodes);
		if (n)
			uwifi_nodes_find_ap(n, &conf.intf.wlan_nodes);

		p->pkt_duration = ieee80211_frame_duration(
				p->phy_flags & PHY_FLAG_MODE_MASK,
				p->wlan_len, p->phy_rate,
				p->phy_flags & PHY_FLAG_SHORTPRE,
				0 /*shortslot*/, p->wlan_type,
				p->wlan_qos_class,
				p->wlan_retries);
	}

	update_history(p);
	update_statistics(p);
	update_spectrum(p, n);
	uwifi_essids_update(&essids, p, n);

	if (!conf.quiet && !conf.debug)
		update_display(p);
}

static void local_receive_packet(int fd, unsigned char* buffer, size_t bufsize)
{
	struct uwifi_packet p;

	LOG_DBG("===============================================================================");

	ssize_t len = packet_socket_recv(fd, buffer, bufsize);
	if (len <= 0) {
		LOG_DBG("recv error");
		return;
	}

#if DEBUG
	if (conf.debug) {
		dump_hex(buffer, len, NULL);
	}
#endif
	memset(&p, 0, sizeof(p));

	if (!parse_packet(buffer, len, &p)) {
		LOG_DBG("parsing failed");
		return;
	}

	handle_packet(&p);
}

static void receive_any(const sigset_t *const waitmask)
{
	int ret, mfd;
	uint32_t usecs = UINT32_MAX;
	struct timespec ts;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&excpt_fds);
	if (!conf.quiet && !conf.debug)
		FD_SET(0, &read_fds);
	FD_SET(conf.intf.sock, &read_fds);
	if (srv_fd != -1)
		FD_SET(srv_fd, &read_fds);
	if (cli_fd != -1)
		FD_SET(cli_fd, &read_fds);
	if (ctlpipe != -1)
		FD_SET(ctlpipe, &read_fds);

	usecs = MIN(uwifi_channel_get_remaining_dwell_time(&conf.intf), 1000000);
	ts.tv_sec = usecs / 1000000;
	ts.tv_nsec = usecs % 1000000 * 1000;
	mfd = MAX(conf.intf.sock, srv_fd);
	mfd = MAX(mfd, ctlpipe);
	mfd = MAX(mfd, cli_fd) + 1;

	ret = pselect(mfd, &read_fds, &write_fds, &excpt_fds, &ts, waitmask);
	if (ret == -1 && errno == EINTR) /* interrupted */
		return;
	if (ret == 0) { /* timeout */
		if (!conf.quiet && !conf.debug)
			update_display_clock();
		return;
	}
	else if (ret < 0) /* error */
		err(1, "select()");

	/* stdin */
	if (FD_ISSET(0, &read_fds) && !conf.quiet && !conf.debug)
		handle_user_input();

	/* local packet or client */
	if (FD_ISSET(conf.intf.sock, &read_fds)) {
		if (conf.serveraddr[0] != '\0')
			net_receive(conf.intf.sock, buffer, &buflen, sizeof(buffer));
		else
			local_receive_packet(conf.intf.sock, buffer, sizeof(buffer));
	}

	/* server */
	if (srv_fd > -1 && FD_ISSET(srv_fd, &read_fds))
		net_handle_server_conn();

	/* from client to server */
	if (cli_fd > -1 && FD_ISSET(cli_fd, &read_fds))
		net_receive(cli_fd, cli_buffer, &cli_buflen, sizeof(cli_buffer));

	/* named pipe */
	if (ctlpipe > -1 && FD_ISSET(ctlpipe, &read_fds))
		control_receive_command();
}

void free_lists(void)
{
	struct chan_node *cn, *cn2;

	/* free channel nodes */
	for (int i = 0; i < uwifi_channel_get_num_channels(&conf.intf.channels); i++) {
		list_for_each_safe(&spectrum[i].nodes, cn, cn2, chan_list) {
			LOG_DBG("free chan_node %p", cn);
			list_del(&cn->chan_list);
			cn->chan->num_nodes--;
			free(cn);
		}
	}

	uwifi_nodes_free(&conf.intf.wlan_nodes);
	uwifi_essids_free(&essids);
}

static void exit_handler(void)
{
	free_lists();

	uwifi_fini(&conf.intf);

	if (conf.monitor_added)
		ifctrl_iwdel(conf.intf.ifname);

	if (DF != NULL) {
		fclose(DF);
		DF = NULL;
	}

	if (conf.allow_control)
		control_finish();

	if (!conf.debug)
		net_finish();

	if (!conf.quiet && !conf.debug)
		finish_display();

	ifctrl_finish();
}

static void sigint_handler(__attribute__((unused)) int sig)
{
	/* Only set an atomic flag here to keep processing in the interrupt
	 * context as minimal as possible (at least all unsafe functions are
	 * prohibited, see signal(7)). The flag is handled in the mainloop. */
	is_sigint_caught = 1;
}

static void sigpipe_handler(__attribute__((unused)) int sig)
{
	/* ignore signal here - we will handle it after write failed */
}

void init_spectrum(void)
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++) {
		list_head_init(&spectrum[i].nodes);
		ewma_init(&spectrum[i].signal_avg, 1024, 8);
		ewma_init(&spectrum[i].durations_avg, 1024, 8);
	}
}

static void mac_name_file_read(const char* filename)
{
	FILE* fp;
	char line[255];
	char macs[18];
	char name[18];
	int idx = 0;
	int n;

	if ((fp = fopen(filename, "r")) == NULL) {
		LOG_ERR("Could not open mac name file '%s'", filename);
		return;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		// first try dnsmasq dhcp.leases format
		n = sscanf(line, "%*s %17s %*s %17s", macs, name);
		if (n < 2) // if not MAC name
			n = sscanf(line, "%17s %17s", macs, name);
		if (n == 2) {
			convert_string_to_mac(macs, node_names.entry[idx].mac);
			strncpy(node_names.entry[idx].name, name, MAX_NODE_NAME_STRLEN);
			node_names.entry[idx].name[MAX_NODE_NAME_STRLEN] = '\0';
			idx++;
		}
	}

	fclose(fp);

	node_names.count = idx;

	for (n = 0; n < node_names.count; n++) {
		LOG_INF("MAC " MAC_FMT " = %s", MAC_PAR(node_names.entry[n].mac),
			 node_names.entry[n].name );
	}
}

const char* mac_name_lookup(const unsigned char* mac, int shorten_mac)
{
	int i;
	if (conf.mac_name_lookup) {
		for (i = 0; i < node_names.count; i++) {
			if (memcmp(node_names.entry[i].mac, mac, WLAN_MAC_LEN) == 0)
				return node_names.entry[i].name;
		}
	}
	return shorten_mac ? mac_sprint_short(mac) : mac_sprint(mac);
}

static void generate_mon_ifname(char *const buf, const size_t buf_size)
{
	unsigned int i;

	for (i=0;; ++i) {
		int len;

		len = snprintf(buf, buf_size, "horst%d", i);
		if (len < 0)
			err(1, "failed to generate monitor interface name");
		if ((unsigned int) len >= buf_size)
			errx(1, "failed to generate a sufficiently short "
			     "monitor interface name");
		if (!if_nametoindex(buf))
			break;  /* interface does not exist yet, done */
	}
}

int main(int argc, char** argv)
{
	sigset_t workmask;
	sigset_t waitmask;
	struct sigaction sigint_action;
	struct sigaction sigpipe_action;

	list_head_init(&essids);
	init_spectrum();

	config_parse_file_and_cmdline(argc, argv);

	sigint_action.sa_handler = sigint_handler;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);
	sigaction(SIGTERM, &sigint_action, NULL);
	sigaction(SIGHUP, &sigint_action, NULL);

	sigpipe_action.sa_handler = sigpipe_handler;
	sigaction(SIGPIPE, &sigpipe_action, NULL);

	atexit(exit_handler);

	clock_gettime(CLOCK_MONOTONIC, &stats.stats_time);
	clock_gettime(CLOCK_MONOTONIC, &time_mono);
	clock_gettime(CLOCK_REALTIME, &time_real);

	conf.intf.channel_idx = -1;

	if (conf.mac_name_lookup)
		mac_name_file_read(conf.mac_name_file);

	if (conf.allow_control) {
		LOG_INF("Allowing control socket '%s'", conf.control_pipe);
		control_init_pipe();
	}

	if (conf.serveraddr[0] != '\0')
		conf.intf.sock = net_open_client_socket(conf.serveraddr, conf.port);
	else {
		ifctrl_init();
		ifctrl_iwget_interface_info(&conf.intf);

		/* if the interface is not already in monitor mode, try to set
		 * it to monitor or create an additional virtual monitor interface */
		if (conf.add_monitor || (!ifctrl_is_monitor(&conf.intf) &&
					 !ifctrl_iwset_monitor(conf.intf.ifname))) {
			char mon_ifname[IF_NAMESIZE];
			generate_mon_ifname(mon_ifname, IF_NAMESIZE);
			if (!ifctrl_iwadd_monitor(conf.intf.ifname, mon_ifname))
				err(1, "failed to add virtual monitor interface");

			LOG_INF("A virtual interface '%s' will be used "
				 "instead of '%s'.", mon_ifname, conf.intf.ifname);

			strncpy(conf.intf.ifname, mon_ifname, IF_NAMESIZE);
			conf.monitor_added = 1;
			/* Now we have a new monitor interface, proceed
			 * normally. The interface will be deleted at exit. */
		}

		printf("SURVEY\n");
		struct survey_info sinf[10];
		ifctrl_iwget_survey(conf.intf.ifname, sinf, 10);

		uwifi_init(&conf.intf);

		if (conf.recv_buffer_size)
			socket_set_receive_buffer(conf.intf.sock, conf.recv_buffer_size);
	}

	printf("Max PHY rate: %d Mbps\n", conf.intf.max_phy_rate/10);

	if (!conf.quiet && !conf.debug)
		init_display();

	if (conf.serveraddr[0] == '\0' && conf.port && conf.allow_client)
		net_init_server_socket(conf.port);

	/* Race-free signal handling:
	 *   1. block all handled signals while working (with workmask)
	 *   2. receive signals *only* while waiting in pselect() (with waitmask)
	 *   3. switch between these two masks atomically with pselect()
	 */
	if (sigemptyset(&workmask)                       == -1 ||
	    sigaddset(&workmask, SIGINT)                 == -1 ||
	    sigaddset(&workmask, SIGHUP)                 == -1 ||
	    sigaddset(&workmask, SIGTERM)                == -1 ||
	    sigprocmask(SIG_BLOCK, &workmask, &waitmask) == -1)
		err(1, "failed to block signals: %m");

	while (!conf.intf.channel_scan || conf.intf.channel_scan_rounds != 0)
	{
		receive_any(&waitmask);

		if (is_sigint_caught)
			exit(1);

		clock_gettime(CLOCK_MONOTONIC, &time_mono);
		clock_gettime(CLOCK_REALTIME, &time_real);
		uwifi_nodes_timeout(&conf.intf.wlan_nodes, conf.node_timeout,
				    &conf.intf.last_nodetimeout);

		if (conf.serveraddr[0] == '\0' /* server */ && !conf.paused) {
			int ret = uwifi_channel_auto_change(&conf.intf);
			if (ret == 1) {
				net_send_channel_config();
				update_spectrum_durations();
				if (!conf.quiet && !conf.debug)
					update_display(NULL);

				if (conf.intf.channel_idx == uwifi_channel_idx_from_freq(&conf.intf.channels, conf.intf.channel_set.freq)
				    && conf.intf.channel_scan_rounds > 0)
					--conf.intf.channel_scan_rounds;
			} else if (ret == -1) {
				LOG_ERR("Channel change failed. Disabling scan on '%s'",
					 conf.intf.ifname);
				conf.intf.channel_scan = false;
				update_display(NULL);
			}
		}
	}
	return 0;
}

void main_pause(int pause)
{
	conf.paused = pause;
	LOG_INF(conf.paused ? "- PAUSED -" : "- RESUME -");
}

void main_reset(void)
{
	if (!conf.quiet && !conf.debug)
		display_clear();
	LOG_INF("- RESET -");
	free_lists();
	memset(&hist, 0, sizeof(hist));
	memset(&stats, 0, sizeof(stats));
	memset(&spectrum, 0, sizeof(spectrum));
	init_spectrum();
	clock_gettime(CLOCK_MONOTONIC, &stats.stats_time);
	clock_gettime(CLOCK_REALTIME, &time_real);
}

void dumpfile_open(const char* name)
{
	if (DF != NULL) {
		fclose(DF);
		DF = NULL;
	}

	if (name == NULL || strlen(name) == 0) {
		LOG_INF("- Not writing outfile");
		conf.dumpfile[0] = '\0';
		return;
	}

	strncpy(conf.dumpfile, name, MAX_CONF_VALUE_STRLEN);
	conf.dumpfile[MAX_CONF_VALUE_STRLEN] = '\0';
	DF = fopen(conf.dumpfile, "w");
	if (DF == NULL)
		err(1, "Couldn't open dump file");

	fprintf(DF, "TIME, WLAN TYPE, MAC SRC, MAC DST, BSSID, PACKET TYPES, SIGNAL, ");
	fprintf(DF, "LENGTH, PHY RATE, FREQUENCY, TSF, ESSID, MODE, CHANNEL, ");
	fprintf(DF, "WEP, WPA1, RSN (WPA2), IP SRC, IP DST\n");

	LOG_INF("- Writing to outfile %s", conf.dumpfile);
}


#if 0
void print_rate_duration_table(void)
{
	int i;

	printf("LEN\t1M l\t1M s\t2M l\t2M s\t5.5M l\t5.5M s\t11M l\t11M s\t");
	printf("6M\t9\t12M\t18M\t24M\t36M\t48M\t54M\n");
	for (i=10; i<=2304; i+=10) {
		printf("%d:\t%d\t%d\t", i,
			ieee80211_frame_duration(PHY_FLAG_G, i, 10, 0, 0, IEEE80211_FTYPE_DATA, 0, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 10, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
		printf("%d\t%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 20, 0, 0, IEEE80211_FTYPE_DATA, 0, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 20, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
		printf("%d\t%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 55, 0, 0, IEEE80211_FTYPE_DATA, 0, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 55, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
		printf("%d\t%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 110, 0, 0, IEEE80211_FTYPE_DATA, 0, 0),
			ieee80211_frame_duration(PHY_FLAG_G, i, 110, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));

		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 60, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 90, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 120, 1, 0, IEEE80211_FTYPE_DATA, 0, 0)),
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 180, 1, 0, IEEE80211_FTYPE_DATA, 0, 0)),
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 240, 1, 0, IEEE80211_FTYPE_DATA, 0, 0)),
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 360, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
		printf("%d\t",
			ieee80211_frame_duration(PHY_FLAG_G, i, 480, 1, 0, IEEE80211_FTYPE_DATA, 0, 0)),
		printf("%d\n",
			ieee80211_frame_duration(PHY_FLAG_G, i, 540, 1, 0, IEEE80211_FTYPE_DATA, 0, 0));
	}
}
#endif
