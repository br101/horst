/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2014-2016 Bruno Randolf (br1@einfach.org)
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
#include <getopt.h>
#include <err.h>

#include "main.h"
#include "util.h"
#include "wlan_util.h"
#include "control.h"
#include "conf_options.h"

struct conf_option {
	int		option;
	const char*	name;
	int		value_required;
	const char*	default_value;
	bool		(*func)(const char* value);
};

static bool conf_quiet(__attribute__((unused)) const char* value) {
	conf.quiet = 1;
	return true;
}

#if DO_DEBUG
static bool conf_debug(__attribute__((unused)) const char* value) {
	conf.debug = 1;
	return true;
}
#endif

static bool conf_interface(const char* value) {
	strncpy(conf.ifname, value, IF_NAMESIZE);
	conf.ifname[IF_NAMESIZE] = '\0';
	return true;
}

static bool conf_add_monitor(const char* value) {
	if (value != NULL && strcmp(value, "0") == 0)
		conf.add_monitor = 0;
	else {
		conf.add_monitor = 1;
	}
	return true;
}

static bool conf_outfile(const char* value) {
	dumpfile_open(value);
	return true;
}

static bool conf_node_timeout(const char* value) {
	conf.node_timeout = atoi(value);
	return true;
}

static bool conf_receive_buffer(const char* value) {
	conf.recv_buffer_size = atoi(value);
	return true;
}

static bool conf_channel_set(const char* value) {
	bool ht40plus = false;
	enum chan_width width = CHAN_WIDTH_20_NOHT;

	char* pos = strchr(value, '+');
	if (pos != NULL) {
		width = CHAN_WIDTH_40;
		ht40plus = true;
		*pos = '\0';
	} else if ((pos = strchr(value, '-')) != NULL) {
		width = CHAN_WIDTH_40;
		ht40plus = false;
		*pos = '\0';
	}

	int n = atoi(value);
	if (conf.channel_initialized)
		channel_change(channel_find_index_from_chan(n), width, ht40plus);
	else {
		/* We have not yet initialized the channel module, channel will be
		* changed in channel_init(). */
		conf.channel_set_num = n;
		conf.channel_set_width = width;
		conf.channel_set_ht40plus = ht40plus;
	}
	return true;
}

static bool conf_channel_scan(const char* value) {
	if (value != NULL && strcmp(value, "0") == 0)
		conf.do_change_channel = 0;
	else {
		conf.do_change_channel = 1;
		conf.display_view = 's'; // show spectrum view
	}
	return true;
}

/*
 * This configuration option (channel_scan_rounds=X) defines the number of
 * rounds horst scans the channel spectrum when the automatic channel scanning
 * is enabled (channel_scan=1). A scan round is considered to be complete when
 * the current channel is changed back to the initial channel. When horst goes
 * out of scan rounds, it quits.
 */
static bool conf_channel_scan_rounds(const char* value) {
	conf.channel_scan_rounds = atoi(value);
	return true;
}

static bool conf_channel_dwell(const char* value) {
	conf.channel_time = atoi(value) * 1000;
	return true;
}

static bool conf_channel_upper(const char* value) {
	conf.channel_max = atoi(value);
	return true;
}

static bool conf_display_interval(const char* value) {
	conf.display_interval = atoi(value) * 1000;
	return true;
}

static bool conf_display_view(const char* value) {
	if (strcasecmp(value, "history") == 0 || strcasecmp(value, "hist") == 0)
		conf.display_view = 'h';
	else if (strcasecmp(value, "essid") == 0)
		conf.display_view = 'e';
	else if (strcasecmp(value, "statistics") == 0 || strcasecmp(value, "stats") == 0)
		conf.display_view = 'a';
	else if (strcasecmp(value, "spectrum") == 0 || strcasecmp(value, "spec") == 0)
		conf.display_view = 's';
	return true;
}

static bool conf_server(const char* value) {
	if (value != NULL && strcmp(value, "0") == 0)
		conf.allow_client = 0;
	else
		conf.allow_client = 1;
	return true;
}

static bool conf_client(const char* value) {
	strncpy(conf.serveraddr, value, MAX_CONF_VALUE_STRLEN);
	conf.serveraddr[MAX_CONF_VALUE_STRLEN] = '\0';
	return true;
}

static bool conf_port(const char* value) {
	conf.port = atoi(value);
	return true;
}

static bool conf_control_pipe(const char* value) {
	/*
	 * Here it's a bit difficult because -X is used for two purposes:
	 * 1) allow control pipe (-X with or without argument)
	 * 2) set the name of the control pipe (-X with argument) which can
	 *    also be used in conjuction with -x
	 * That's why we don't set a default value (as it would always allow control)
	 * and especially handle the default name here and in control_send_command()
	 */
	if (value != NULL)
		strncpy(conf.control_pipe, value, MAX_CONF_VALUE_STRLEN);
	else
		strncpy(conf.control_pipe, DEFAULT_CONTROL_PIPE, MAX_CONF_VALUE_STRLEN);
	conf.control_pipe[MAX_CONF_VALUE_STRLEN] = '\0';
	conf.allow_control = 1;
	return true;
}

static bool conf_filter_mac(const char* value) {
	static int n;
	if (n >= MAX_FILTERMAC) {
		printlog("Can only handle %d MAC filters", MAX_FILTERMAC);
		return false;
	}

	conf.do_macfilter = 1;
	convert_string_to_mac(value, conf.filtermac[n]);
	conf.filtermac_enabled[n] = 1;
	n++;
	return true;
}

static bool conf_filter_bssid(const char* value) {
	convert_string_to_mac(value, conf.filterbssid);
	return true;
}

static bool conf_filter_mode(const char* value) {
	if (conf.filter_mode == WLAN_MODE_ALL)
		conf.filter_mode = 0;
	if (strcmp(value, "ALL") == 0)
		conf.filter_mode = WLAN_MODE_ALL;
	else if (strcmp(value, "AP") == 0)
		conf.filter_mode |= WLAN_MODE_AP;
	else if (strcmp(value, "STA") == 0)
		conf.filter_mode |= WLAN_MODE_STA;
	else if (strcmp(value, "ADH") == 0 || strcmp(value, "IBSS") == 0)
		conf.filter_mode |= WLAN_MODE_IBSS;
	else if (strcmp(value, "PRB") == 0)
		conf.filter_mode |= WLAN_MODE_PROBE;
	else if (strcmp(value, "WDS") == 0)
		conf.filter_mode |= WLAN_MODE_4ADDR;
	else if (strcmp(value, "UNKNOWN") == 0)
		conf.filter_mode |= WLAN_MODE_UNKNOWN;
	return true;
}

static bool conf_filter_pkt(const char* value) {
	int t, i;

	if (conf.filter_pkt == PKT_TYPE_ALL) {
		conf.filter_pkt = 0;
		conf.filter_badfcs = 0;
		conf.filter_stype[WLAN_FRAME_TYPE_MGMT] = 0;
		conf.filter_stype[WLAN_FRAME_TYPE_CTRL] = 0;
		conf.filter_stype[WLAN_FRAME_TYPE_DATA] = 0;
	}
	if (strcmp(value, "ALL") == 0) {
		conf.filter_pkt = PKT_TYPE_ALL;
		conf.filter_badfcs = 1;
		conf.filter_stype[WLAN_FRAME_TYPE_MGMT] = 0xffff;
		conf.filter_stype[WLAN_FRAME_TYPE_CTRL] = 0xffff;
		conf.filter_stype[WLAN_FRAME_TYPE_DATA] = 0xffff;
	}
	else if (strcmp(value, "BADFCS") == 0)
		conf.filter_badfcs = 1;
	else if (strcmp(value, "CTRL") == 0 || strcmp(value, "CONTROL") == 0)
		conf.filter_stype[WLAN_FRAME_TYPE_CTRL] = 0xffff;
	else if (strcmp(value, "MGMT") == 0 || strcmp(value, "MANAGEMENT") == 0)
		conf.filter_stype[WLAN_FRAME_TYPE_MGMT] = 0xffff;
	else if (strcmp(value, "DATA") == 0)
		conf.filter_stype[WLAN_FRAME_TYPE_DATA] = 0xffff;
	else if (strcmp(value, "ARP") == 0)
		conf.filter_pkt |= PKT_TYPE_ARP;
	else if (strcmp(value, "IP") == 0)
		conf.filter_pkt |= PKT_TYPE_IP;
	else if (strcmp(value, "ICMP") == 0)
		conf.filter_pkt |= PKT_TYPE_ICMP;
	else if (strcmp(value, "UDP") == 0)
		conf.filter_pkt |= PKT_TYPE_UDP;
	else if (strcmp(value, "TCP") == 0)
		conf.filter_pkt |= PKT_TYPE_TCP;
	else if (strcmp(value, "OLSR") == 0)
		conf.filter_pkt |= PKT_TYPE_OLSR;
	else if (strcmp(value, "BATMAN") == 0)
		conf.filter_pkt |= PKT_TYPE_BATMAN;
	else if (strcmp(value, "MESHZ") == 0)
		conf.filter_pkt |= PKT_TYPE_MESHZ;

	for (t = 0; t < WLAN_NUM_TYPES; t++) {
		for (i = 0; i < WLAN_NUM_STYPES; i++) {
			if (strcasecmp(stype_names[t][i].name, value) == 0) {
				conf.filter_stype[t] |= BIT(i);
				return true;
			}
		}
	}
	return true;
}

static bool conf_mac_names(const char* value) {
	if (value != NULL)
		strncpy(conf.mac_name_file, value, MAX_CONF_VALUE_STRLEN);
	else
		strncpy(conf.mac_name_file, DEFAULT_MAC_NAME_FILE, MAX_CONF_VALUE_STRLEN);
	conf.mac_name_file[MAX_CONF_VALUE_STRLEN] = '\0';
	conf.mac_name_lookup = 1;
	return true;
}

static struct conf_option conf_options[] = {
	/* C , NAME        VALUE REQUIRED, DEFAULT	CALLBACK */
	{ 'q', "quiet",			0, NULL,	conf_quiet },		// NOT dynamic
#if DO_DEBUG
	{ 'D', "debug", 		0, NULL,	conf_debug },		// NOT dynamic
#endif
	{ 'i', "interface", 		1, "wlan0",	conf_interface },	// NOT dynamic
	{ 'a', "add_monitor",		0, NULL,	conf_add_monitor },
	{ 'd', "display_interval",	1, "100", 	conf_display_interval },
	{ 'V', "display_view",		1, NULL, 	conf_display_view },
	{ 'o', "outfile", 		1, NULL,	conf_outfile },
	{ 't', "node_timeout", 		1, "60",	conf_node_timeout },
	{ 'b', "receive_buffer",	1, NULL,	conf_receive_buffer },	// NOT dynamic
	{ 'C', "channel",		1, NULL, 	conf_channel_set },
	{ 's', "channel_scan",		0, NULL,	conf_channel_scan },
	{  0 , "channel_scan_rounds",	1, "-1",	conf_channel_scan_rounds },
	{  0 , "channel_dwell",		1, "250", 	conf_channel_dwell },
	{ 'u', "channel_upper",		1, NULL, 	conf_channel_upper },
	{ 'N', "server",		0, NULL,	conf_server },		// NOT dynamic
	{ 'n', "client",		1, NULL,	conf_client },		// NOT dynamic
	{ 'p', "port",			1, "4444",	conf_port },		// NOT dynamic
	{ 'X', "control_pipe",		2, NULL,	conf_control_pipe },	// NOT dynamic
	{ 'e', "filter_mac", 		1, NULL,	conf_filter_mac },
	{ 'B', "filter_bssid", 		1, NULL,	conf_filter_bssid },
	{ 'm', "filter_mode",		1, "ALL",	conf_filter_mode },
	{ 'f', "filter_packet",		1, "ALL",	conf_filter_pkt },
	{ 'M', "mac_names",		2, NULL,	conf_mac_names },
};

/*
 * More possible config options:
 *
 * main view:
 *	sort nodes by: signal, time, bssid, channel
 * spec view:
 *	show nodes or bars
 */


/*
 * This handles command line options from getopt as well as options from the config file
 * In the first case 'c' is non-zero and name is NULL
 * In the second case 'c' is 0 and name is set
 * Value may be null in all cases
 */
bool config_handle_option(int c, const char* name, const char* value)
{
	unsigned int i;
	char* end;

	for (i=0; i < sizeof(conf_options)/sizeof(struct conf_option); i++) {
		if (((c != 0 && conf_options[i].option == c) ||
		    (name != NULL && strcmp(conf_options[i].name, name) == 0)) &&
		     conf_options[i].func != NULL) {
			if (!conf.quiet) {
				if (value != NULL)
					printlog("Set '%s' = '%s'", conf_options[i].name, value);
				else
					printlog("Set '%s'", conf_options[i].name);
			}
			if (value != NULL) {
				/* split list values and call function multiple times */
				while ((end = strchr(value, ',')) != NULL) {
					*end = '\0';
					conf_options[i].func(value);
					value = end + 1;
				}
			}
			/* call function */
			return conf_options[i].func(value);
		}
	}
	if (name != NULL)
		printlog("Ignoring unknown config option '%s' = '%s'", name, value);
	return false;
}

static void config_read_file(const char* filename)
{
	FILE* fp ;
	char line[255];
	char name[MAX_CONF_NAME_STRLEN + 1];
	char value[MAX_CONF_VALUE_STRLEN + 1];
	int n;
	int linenum = 0;

	if ((fp = fopen(filename, "r")) == NULL) {
		printlog("Could not open config file '%s'", filename);
		return;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		++linenum;
		if (line[0] == '#' ) // comment
			continue;

		// Note: 200 below has to match MAX_CONF_VALUE_STRLEN
		// Note: 32 below has to match MAX_CONF_NAME_STRLEN
		n = sscanf(line, " %32[^= \n] = %200[^ \n]", name, value);
		if (n < 0) { // empty line
			continue;
		} else if (n == 0) {
			printlog("Config file has garbage on line %d, "
				 "ignoring the line.", linenum);
			continue;
		} else if (n == 1) { // no value
			config_handle_option(0, name, NULL);
		} else {
			config_handle_option(0, name, value);
		}
	}

	fclose(fp);
}

static void config_apply_defaults(void)
{
	unsigned int i;
	for (i=0; i < sizeof(conf_options)/sizeof(struct conf_option); i++) {
		if (conf_options[i].default_value != NULL) {
			conf_options[i].func(conf_options[i].default_value);
		}
	}
}

static char* config_get_getopt_string(char* buf, size_t maxlen, const char* add)
{
	unsigned int pos = 0;
	unsigned int i;
	maxlen = maxlen - 1; // we use it as string index

	for (i=0; i < sizeof(conf_options)/sizeof(struct conf_option) && pos < maxlen; i++) {
		if (conf_options[i].option != 0 && pos < maxlen) {
			buf[pos++] = conf_options[i].option;
			if (conf_options[i].value_required && pos < maxlen) {
				buf[pos++] = ':';
			}
			if (conf_options[i].value_required == 2 && pos < maxlen) {
				buf[pos++] = ':';
			}
		}
	}
	buf[pos] = '\0';

	if (add != NULL) {
		if (pos < maxlen && (maxlen - pos) >= strlen(add))
			strncat(buf, add, (maxlen - pos));
		else {
			printlog("Not enough space for getopt string!");
			exit(1);
		}
	}

	return buf;
}

static void print_usage(const char* name)
{
	printf("\nUsage: %s [-v] [-h] [-q] [-D] [-a] [-c file] [-i interface] [-t sec] [-d ms] [-V view] [-b bytes]\n"
		"\t\t[-s] [-u] [-N] [-n IP] [-p port] [-o file] [-X[name]] [-x command]\n"
		"\t\t[][-e MAC] [-f PKT_NAME] [-m MODE] [-B BSSID]\n\n"

		"General Options: Description (default value)\n"
		"  -v\t\tshow version\n"
		"  -h\t\tHelp\n"
		"  -q\t\tQuiet, no output\n"
#if DO_DEBUG
		"  -D\t\tShow lots of debug output, no UI\n"
#endif
		"  -a\t\tAlways add virtual monitor interface\n"
		"  -c <file>\tConfig file (" CONFIG_FILE ")\n"
		"  -C <chan>\tSet initial channel\n"
		"  -i <intf>\tInterface name (wlan0)\n"
		"  -t <sec>\tNode timeout in seconds (60)\n"
		"  -d <ms>\tDisplay update interval in ms (100)\n"
		"  -V view\tDisplay view: history|essid|statistics|spectrum\n"
		"  -b <bytes>\tReceive buffer size in bytes (not set)\n"
		"  -M[filename]\tMAC address to host name mapping (/tmp/dhcp.leases)\n"

		"\nFeature Options:\n"
		"  -s\t\t(Poor mans) Spectrum analyzer mode\n"
		"  -u\t\tUpper channel limit\n\n"

		"  -N\t\tAllow network connection, server mode (off)\n"
		"  -n <IP>\tConnect to server with <IP>, client mode (off)\n"
		"  -p <port>\tPort number of server (4444)\n\n"

		"  -o <filename>\tWrite packet info into 'filename'\n\n"

		"  -X[filename]\tAllow control socket on 'filename' (/tmp/horst)\n"
		"  -x <command>\tSend control command\n"

		"\nFilter Options:\n"
		" Filters are generally 'positive' or 'inclusive' which means you define\n"
		" what you want to see, and everything else is getting filtered out.\n"
		" If a filter is not set it is inactive and nothing is filtered.\n"
		" Most filter options can be specified multiple times and will be combined\n"
		"  -e <MAC>\tSource MAC addresses (xx:xx:xx:xx:xx:xx), up to 9 times\n"
		"  -f <PKT_NAME>\tFilter packet types, multiple\n"
		"  -m <MODE>\tOperating mode: AP|STA|ADH|PRB|WDS|UNKNOWN, multiple\n"
		"  -B <MAC>\tBSSID (xx:xx:xx:xx:xx:xx), only one\n"
		"\n",
		name);
}

void config_parse_file_and_cmdline(int argc, char** argv)
{
	char getopt_str[(sizeof(conf_options)/sizeof(struct conf_option))*2 + 10];
	char* conf_filename = CONFIG_FILE;
	int c;

	config_get_getopt_string(getopt_str, sizeof(getopt_str), "hvc:x:");

	/* first: apply default values */
	config_apply_defaults();

	/*
	 * then: handle command line options which are not
	 * configuration options ("hc:")
	 */
	while ((c = getopt(argc, argv, getopt_str)) > 0) {
		switch (c) {
		case 'c':
			printlog("Using config file '%s'", optarg);
			conf_filename = optarg;
			break;
		case 'v':
			printf("Version %s (build date: %s %s)\n", VERSION, __DATE__, __TIME__);
			exit(0);
		case 'h':
		case '?':
			print_usage(argv[0]);
			exit(0);
		}
	}

	/* read config file */
	config_read_file(conf_filename);

	/*
	 * get command line options which are configuration, to let them
	 * override or add to the config file options
	 */
	optind = 1;
	while ((c = getopt(argc, argv, getopt_str)) > 0) {
		config_handle_option(c, NULL, optarg);
	}

	/*
	 * and finally get command line options ("commands") which depend
	 * on config options ("x:")
	 */
	optind = 1;
	while ((c = getopt(argc, argv, getopt_str)) > 0) {
		switch (c) {
		case 'x':
			control_send_command(optarg);
			exit(0);
		}
	}
}
