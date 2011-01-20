/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2011 Bruno Randolf (br1@einfach.org)
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

#include "util.h"
#include "ieee80211.h"


struct pkt_names {
	char c;
	const char* name;
};

/* a list of packet type names for easier indexing with padding */
static struct pkt_names mgmt_names[] = {
	{ 'a', "ASOCRQ" },		/* IEEE80211_STYPE_ASSOC_REQ	0x0000 */
	{ 'A', "ASOCRP" },		/* IEEE80211_STYPE_ASSOC_RESP	0x0010 */
	{ 'a', "REASRQ" },		/* IEEE80211_STYPE_REASSOC_REQ	0x0020 */
	{ 'A', "REASRP" },		/* IEEE80211_STYPE_REASSOC_RESP	0x0030 */
	{ 'p', "PROBRQ" },		/* IEEE80211_STYPE_PROBE_REQ	0x0040 */
	{ 'P', "PROBRP" },		/* IEEE80211_STYPE_PROBE_RESP	0x0050 */
	{}, {}, 			/* unused */
	{ 'B', "BEACON" },		/* IEEE80211_STYPE_BEACON	0x0080 */
	{ 't', "ATIM" },		/* IEEE80211_STYPE_ATIM		0x0090 */
	{ 'D', "DISASC" },		/* IEEE80211_STYPE_DISASSOC	0x00A0 */
	{ 'u', "AUTH" },		/* IEEE80211_STYPE_AUTH		0x00B0 */
	{ 'U', "DEAUTH" },		/* IEEE80211_STYPE_DEAUTH	0x00C0 */
	{ 'T', "ACTION" },		/* IEEE80211_STYPE_ACTION	0x00D0 */
};

static struct pkt_names ctrl_names[] = {
	{ 's', "PSPOLL" },		/* IEEE80211_STYPE_PSPOLL	0x00A0 */
	{ 'R', "RTS" },			/* IEEE80211_STYPE_RTS		0x00B0 */
	{ 'C', "CTS" },			/* IEEE80211_STYPE_CTS		0x00C0 */
	{ 'K', "ACK" },			/* IEEE80211_STYPE_ACK		0x00D0 */
	{ 'f', "CFEND" },		/* IEEE80211_STYPE_CFEND	0x00E0 */
	{ 'f', "CFENDK" },		/* IEEE80211_STYPE_CFENDACK	0x00F0 */
};

static struct pkt_names data_names[] = {
	{ 'D', "DATA" },		/* IEEE80211_STYPE_DATA			0x0000 */
	{ 'F', "DCFACK" },		/* IEEE80211_STYPE_DATA_CFACK		0x0010 */
	{ 'F', "DCFPLL" },		/* IEEE80211_STYPE_DATA_CFPOLL		0x0020 */
	{ 'F', "DCFKPL" },		/* IEEE80211_STYPE_DATA_CFACKPOLL	0x0030 */
	{ 'n', "NULL" },		/* IEEE80211_STYPE_NULLFUNC		0x0040 */
	{ 'f', "CFACK" },		/* IEEE80211_STYPE_CFACK		0x0050 */
	{ 'f', "CFPOLL" },		/* IEEE80211_STYPE_CFPOLL		0x0060 */
	{ 'f', "CFCKPL" },		/* IEEE80211_STYPE_CFACKPOLL		0x0070 */
	{ 'Q', "QDATA" },		/* IEEE80211_STYPE_QOS_DATA		0x0080 */
	{ 'F', "QDCFCK" },		/* IEEE80211_STYPE_QOS_DATA_CFACK	0x0090 */
	{ 'F', "QDCFPL" },		/* IEEE80211_STYPE_QOS_DATA_CFPOLL	0x00A0 */
	{ 'F', "QDCFKP" },		/* IEEE80211_STYPE_QOS_DATA_CFACKPOLL	0x00B0 */
	{ 'N', "QDNULL" },		/* IEEE80211_STYPE_QOS_NULLFUNC		0x00C0 */
	{ 'f', "QCFACK" },		/* IEEE80211_STYPE_QOS_CFACK		0x00D0 */
	{ 'f', "QCFPLL" },		/* IEEE80211_STYPE_QOS_CFPOLL		0x00E0 */
	{ 'f', "QCFKPL" },		/* IEEE80211_STYPE_QOS_CFACKPOLL	0x00F0 */
};

#define DATA_NAME_INDEX(_i) (((_i) & IEEE80211_FCTL_STYPE)>>4)
#define MGMT_NAME_INDEX(_i) (((_i) & IEEE80211_FCTL_STYPE)>>4)
#define CTRL_NAME_INDEX(_i) ((((_i) & IEEE80211_FCTL_STYPE)>>4)-10)


int
normalize(float oval, int max_val, int max) {
	int val;
	val= (oval / max_val) * max;
	if (val > max) /* cap if still bigger */
		val = max;
	if (val == 0 && oval > 0)
		val = 1;
	if (val < 0)
		val = 0;
	return val;
}


void
dump_packet(const unsigned char* buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if ((i % 2) == 0)
			DEBUG(" ");
		if ((i % 16) == 0)
			DEBUG("\n");
		DEBUG("%02x", buf[i]);
	}
	DEBUG("\n");
}


const char*
ether_sprintf(const unsigned char *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

const char*
ether_sprintf_short(const unsigned char *mac)
{
	static char etherbuf[5];
	snprintf(etherbuf, sizeof(etherbuf), "%02x%02x",
		mac[4], mac[5]);
	return etherbuf;
}


const char*
ip_sprintf(const unsigned int ip)
{
	static char ipbuf[18];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d",
		cip[0], cip[1], cip[2], cip[3]);
	return ipbuf;
}


const char*
ip_sprintf_short(const unsigned int ip)
{
	static char ipbuf[5];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), ".%d", cip[3]);
	return ipbuf;
}


void
convert_string_to_mac(const char* string, unsigned char* mac)
{
	int c;
	for(c = 0; c < 6 && string; c++) {
		int x = 0;
		if (string)
			sscanf(string, "%x", &x);
		mac[c] = x;
		string = strchr(string, ':');
		if (string)
			string++;
	}
}


char
get_packet_type_char(int type)
{
	switch (type & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_MGMT:
		if (MGMT_NAME_INDEX(type) < sizeof(mgmt_names)/sizeof(struct pkt_names)) {
			if (mgmt_names[MGMT_NAME_INDEX(type)].c)
				return mgmt_names[MGMT_NAME_INDEX(type)].c;
		}
		break;
	case IEEE80211_FTYPE_CTL:
		if (CTRL_NAME_INDEX(type) < sizeof(ctrl_names)/sizeof(struct pkt_names)) {
			if (ctrl_names[CTRL_NAME_INDEX(type)].c)
				return ctrl_names[CTRL_NAME_INDEX(type)].c;
		}
		break;
	case IEEE80211_FTYPE_DATA:
		if (DATA_NAME_INDEX(type) < sizeof(data_names)/sizeof(struct pkt_names)) {
			if (data_names[DATA_NAME_INDEX(type)].c)
				return data_names[DATA_NAME_INDEX(type)].c;
		}
		break;
	}
	return '?';
}


const char*
get_packet_type_name(int type)
{
	switch (type & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_MGMT:
		if (MGMT_NAME_INDEX(type) < sizeof(mgmt_names)/sizeof(struct pkt_names)) {
			if (mgmt_names[MGMT_NAME_INDEX(type)].c)
				return mgmt_names[MGMT_NAME_INDEX(type)].name;
		}
		break;
	case IEEE80211_FTYPE_CTL:
		if (CTRL_NAME_INDEX(type) < sizeof(ctrl_names)/sizeof(struct pkt_names)) {
			if (ctrl_names[CTRL_NAME_INDEX(type)].c)
				return ctrl_names[CTRL_NAME_INDEX(type)].name;
		}
		break;
	case IEEE80211_FTYPE_DATA:
		if (DATA_NAME_INDEX(type) < sizeof(data_names)/sizeof(struct pkt_names)) {
			if (data_names[DATA_NAME_INDEX(type)].c)
				return data_names[DATA_NAME_INDEX(type)].name;
		}
		break;
	}
	return "UNKNOW";
}


const char*
kilo_mega_ize(unsigned int val) {
	static char buf[20];
	char c = 0;
	int rest;
	if (val >= 1024) { /* kilo */
		rest = (val & 1023) / 102.4; /* only one digit */
		val = val >> 10;
		c = 'k';
	}
	if (val >= 1024) { /* mega */
		rest = (val & 1023) / 102.4; /* only one digit */
		val = val >> 10;
		c = 'M';
	}
	if (c)
		snprintf(buf, sizeof(buf), "%d.%d%c", val, rest, c);
	else
		snprintf(buf, sizeof(buf), "%d", val);
	return buf;
}


/* simple ilog2 implementation */
int
ilog2(int x) {
	int n;
	for (n = 0; !(x & 1); n++)
		x = x >> 1;
	return n;
}
