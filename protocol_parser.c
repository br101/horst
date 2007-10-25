/* olsr scanning tool
 *
 * Copyright (C) 2005  Bruno Randolf
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
#include <sys/socket.h>
#include <net/if_arp.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "prism_header.h"
#include "ieee80211_radiotap.h"
#include "ieee80211_header.h"
#include "olsr_header.h"

#include "protocol_parser.h"
#include "main.h"
#include "display.h"

#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803    /* IEEE 802.11 + radiotap header */
#endif

#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802      /* IEEE 802.11 + Prism2 header  */
#endif

static int parse_prism_header(unsigned char** buf, int len);
static int parse_radiotap_header(unsigned char** buf, int len);
static int parse_80211_header(unsigned char** buf, int len);
static int parse_ip_header(unsigned char** buf, int len);
static int parse_udp_header(unsigned char** buf, int len);
static int parse_olsr_packet(unsigned char** buf, int len);

/* return 1 if we parsed enough = min ieee header */
int
parse_packet(unsigned char* buf, int len)
{
	if (arphrd == ARPHRD_IEEE80211_PRISM) {
		len = parse_prism_header(&buf, len);
		if (len <= 0)
			return 0;
	}
	else if (arphrd == ARPHRD_IEEE80211_RADIOTAP) {
		len = parse_radiotap_header(&buf, len);
		if (len <= 0)
			return 0;
	}

	if (arphrd == ARPHRD_IEEE80211 ||
	    arphrd == ARPHRD_IEEE80211_PRISM ||
	    arphrd == ARPHRD_IEEE80211_RADIOTAP) {
		DEBUG("before parse 80211 len: %d\n", len);
		len = parse_80211_header(&buf, len);
		if (len < 0) /* couldnt parse */
			return 0;
		else if (len == 0)
			return 1;
	}
	
	len = parse_ip_header(&buf, len);
	if (len <= 0)
		return 1;

	len = parse_udp_header(&buf, len);
	if (len <= 0)
		return 1;

	len = parse_olsr_packet(&buf, len);
	return 1;
}


static int
parse_prism_header(unsigned char** buf, int len)
{
	wlan_ng_prism2_header* ph;

	DEBUG("PRISM2 HEADER\n");

	if (len < sizeof(wlan_ng_prism2_header))
		return -1;

	ph = (wlan_ng_prism2_header*)*buf;

	/*
	 * different drivers report S/N and rssi values differently
	 * let's make sure here that SNR is always positive, so we
	 * don't have do handle special cases later
	 * let signal and noise be driver specific for now
	*/
	if (((int)ph->noise.data)<0) {
		/* new madwifi */
		current_packet.signal = ph->signal.data;
		current_packet.noise = ph->noise.data;
		current_packet.snr = ph->rssi.data;
		/* old madwifi:
		current_packet.noise = 95; // noise is constantly -95
		// signal is rssi (received signal strength) relative to -95dB noise
		current_packet.signal = 95 - ph->signal.data;
		current_packet.snr = ph->signal.data;
		*/
	}
	else if (((int)ph->rssi.data)<0) {
		/* broadcom hack */
		current_packet.signal = -95 - ph->rssi.data;
		current_packet.noise = -95;
		current_packet.snr = -ph->rssi.data;
	}
	else {
		/* assume hostap */
		current_packet.signal = ph->signal.data;
		current_packet.noise = ph->noise.data;
		current_packet.snr = ph->signal.data - ph->noise.data; //XXX rssi?
	}

	/* just in case...*/
	if (current_packet.snr<0)
		current_packet.snr = -current_packet.snr;
	if (current_packet.snr>99)
		current_packet.snr = 99;

	current_packet.rate = ph->rate.data/2;

	DEBUG("devname: %s\n", ph->devname);
	DEBUG("signal: %d -> %d\n", ph->signal.data, current_packet.signal);
	DEBUG("noise: %d -> %d\n", ph->noise.data, current_packet.noise);
	DEBUG("rate: %d\n", ph->rate.data);
	DEBUG("rssi: %d\n", ph->rssi.data);
	DEBUG("*** SNR %d\n", current_packet.snr);

	*buf = *buf + sizeof(wlan_ng_prism2_header);
	return len - sizeof(wlan_ng_prism2_header);
}


static int
parse_radiotap_header(unsigned char** buf, int len)
{
	struct ieee80211_radiotap_header* rh;
	__le32 present; /* the present bitmap */
	unsigned char* b; /* current byte */
	int i;

	DEBUG("RADIOTAP HEADER\n");

	if (len < sizeof(struct ieee80211_radiotap_header))
		return -1;

	rh = (struct ieee80211_radiotap_header*)*buf;
	b = *buf + sizeof(struct ieee80211_radiotap_header);
	present = rh->it_present;

	DEBUG("%08x\n", present);

	/* check for header extension - ignore for now, just advance current position */
	while (present & 0x80000000  && b-*buf < rh->it_len) {
		DEBUG("extension\n");
		b = b + 4;
		present = *(__le32*)b;
	}
	present = rh->it_present; // in case it move
	/* radiotap bitmap has 32 bit, but we are only interrested until
	 * bit 12 (IEEE80211_RADIOTAP_DB_ANTSIGNAL) => i<13 */
	for (i=0; i<13 && b-*buf < rh->it_len; i++) {
		if ((present >> i) & 1) {
			DEBUG("1");
			switch (i) {
				/* just ignore the following (advance position only): */
				case IEEE80211_RADIOTAP_TSFT:
					DEBUG("[+8]");
					b = b + 8;
					break;
				case IEEE80211_RADIOTAP_DBM_TX_POWER:
				case IEEE80211_RADIOTAP_ANTENNA:
				case IEEE80211_RADIOTAP_RTS_RETRIES:
				case IEEE80211_RADIOTAP_DATA_RETRIES:
				case IEEE80211_RADIOTAP_FLAGS:
					DEBUG("[+1]");
					b++;
					break;
				case IEEE80211_RADIOTAP_CHANNEL:
				case IEEE80211_RADIOTAP_EXT:
					DEBUG("[+4]");
					b = b + 4;
					break;
				case IEEE80211_RADIOTAP_FHSS:
				case IEEE80211_RADIOTAP_LOCK_QUALITY:
				case IEEE80211_RADIOTAP_TX_ATTENUATION:
				case IEEE80211_RADIOTAP_RX_FLAGS:
				case IEEE80211_RADIOTAP_TX_FLAGS:
				case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
					DEBUG("[+2]");
					b = b + 2;
					break;
				/* we are only interrested in these: */
				case IEEE80211_RADIOTAP_RATE:
					DEBUG("[rate %0x]", *b);
					current_packet.rate = (*b)/2;
					b++;
					break;
				case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
					DEBUG("[sig %0x]", *b);
					current_packet.signal = *(char*)b;
					b++;
					break;
				case IEEE80211_RADIOTAP_DBM_ANTNOISE:
					DEBUG("[noi %0x]", *b);
					current_packet.noise = *(char*)b;
					b++;
					break;
				case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
					DEBUG("[snr %0x]", *b);
					current_packet.snr = *b;
					b++;
					break;
			}
		}
		else {
			DEBUG("0");
		}
	}

	if (current_packet.snr>99)
		current_packet.snr = 99;

	DEBUG("\nrate: %d\n", current_packet.rate);
	DEBUG("signal: %d\n", current_packet.signal);
	DEBUG("noise: %d\n", current_packet.noise);
	DEBUG("snr: %d\n", current_packet.snr);

	*buf = *buf + rh->it_len;
	return len - rh->it_len;
}


static int
parse_80211_header(unsigned char** buf, int len)
{
	struct ieee80211_hdr* wh;

	if (len < sizeof(struct ieee80211_hdr))
		return -1;

	wh = (struct ieee80211_hdr*)*buf;
	DEBUG("STYPE %x\n", WLAN_FC_GET_STYPE(wh->frame_control));

	current_packet.wlan_type = WLAN_FC_GET_TYPE(wh->frame_control);
	current_packet.wlan_stype = WLAN_FC_GET_STYPE(wh->frame_control);

	if (WLAN_FC_GET_TYPE(wh->frame_control) == WLAN_FC_TYPE_DATA)
	{
		if (wh->frame_control & WLAN_FC_TODS) {
			/* to AP */
			memcpy(current_packet.wlan_src, wh->addr2, 6);
			memcpy(current_packet.wlan_dst, wh->addr3, 6);
			memcpy(current_packet.wlan_bssid, wh->addr1, 6);
			current_packet.wlan_mode = WLAN_MODE_STA;
		}
		else if (wh->frame_control & WLAN_FC_FROMDS) {
			/* from AP */
			memcpy(current_packet.wlan_src, wh->addr3, 6);
			memcpy(current_packet.wlan_dst, wh->addr1, 6);
			memcpy(current_packet.wlan_bssid, wh->addr2, 6);
			current_packet.wlan_mode = WLAN_MODE_AP;
		}
		else if (wh->frame_control & WLAN_FC_FROMDS && wh->frame_control & WLAN_FC_TODS) {
			/* WDS - ignore */
			return 0;
		}
		else {
			/* IBSS */
			memcpy(current_packet.wlan_src, wh->addr2, 6);
			memcpy(current_packet.wlan_dst, wh->addr1, 6);
			memcpy(current_packet.wlan_bssid, wh->addr3, 6);
			current_packet.wlan_mode = WLAN_MODE_IBSS;
		}
		current_packet.pkt_types = PKT_TYPE_DATA;
	}
	else if (WLAN_FC_GET_TYPE(wh->frame_control) == WLAN_FC_TYPE_MGMT) {
		memcpy(current_packet.wlan_dst, wh->addr1, 6);
		memcpy(current_packet.wlan_src, wh->addr2, 6);
		memcpy(current_packet.wlan_bssid, wh->addr3, 6);
		//TODO: other subtypes
		if (WLAN_FC_GET_STYPE(wh->frame_control) & WLAN_FC_STYPE_BEACON) {
			struct ieee80211_mgmt* whm;
			if (len < sizeof(struct ieee80211_mgmt))
				return -1;
			whm = (struct ieee80211_mgmt*)*buf;
			memcpy(current_packet.wlan_tsf, whm->u.beacon.timestamp,8);
			if (whm->u.beacon.variable[0] == 0) { /* ESSID */
				memcpy(current_packet.wlan_essid, &whm->u.beacon.variable[2], whm->u.beacon.variable[1]);
				current_packet.wlan_essid[whm->u.beacon.variable[1]]='\0';
			}
			current_packet.pkt_types = PKT_TYPE_BEACON;
			if (whm->u.beacon.capab_info & WLAN_CAPABILITY_IBSS)
				current_packet.wlan_mode = WLAN_MODE_IBSS;
			else if (whm->u.beacon.capab_info & WLAN_CAPABILITY_ESS)
				current_packet.wlan_mode = WLAN_MODE_AP;
			return 0;
		}
		else if (WLAN_FC_GET_STYPE(wh->frame_control) & WLAN_FC_STYPE_PROBE_REQ) {
			current_packet.pkt_types = PKT_TYPE_PROBE_REQ;
			return 0;
		}
	}
	else if (WLAN_FC_GET_TYPE(wh->frame_control) == WLAN_FC_TYPE_CTRL) {
		; /* ignore */
	}
	else { /* shouldnt happen (tm) */
		return 0;
	}

	//TODO: 802.11 headers with 4 addresses (WDS) are longer

	*buf = *buf + IEEE80211_HEADER_LEN;
	return len - IEEE80211_HEADER_LEN;
}

static int
parse_ip_header(unsigned char** buf, int len)
{
	struct iphdr* ih;

	if (len < 6)
		return -1;

	/* check type in LLC header */	
	*buf = *buf + 6;

	if (**buf != 0x08) /* not IP */
		return -1;
	(*buf)++;
	
	if (**buf != 0x00)
		return -1;
	(*buf)++;

	if (len < sizeof(struct iphdr))
		return -1;

	ih = (struct iphdr*)*buf;

	DEBUG("*** IP SRC: %s\n", ip_sprintf(ih->saddr));
	DEBUG("*** IP DST: %s\n", ip_sprintf(ih->daddr));
	current_packet.ip_src = ih->saddr;
	current_packet.ip_dst = ih->daddr;
	current_packet.pkt_types |= PKT_TYPE_IP;

	DEBUG("IP proto: %d\n", ih->protocol);
	if (ih->protocol != 17) /* UDP */
		return 0;

	*buf = *buf + ih->ihl*4;
	return len - ih->ihl*4;
}


static int
parse_udp_header(unsigned char** buf, int len)
{
	struct udphdr* uh;

	if (len < sizeof(struct udphdr))
		return -1;

	uh = (struct udphdr*)*buf;

	DEBUG("UPD dest port: %d\n", ntohs(uh->dest));
	if (ntohs(uh->dest) != 698) /* OLSR */
		return 0;
	*buf = *buf + 8;
	return len - 8;
}


static int
parse_olsr_packet(unsigned char** buf, int len)
{
	struct olsr* oh;
	int number;
	int i;

	if (len < sizeof(struct olsr))
		return -1;

	oh = (struct olsr*)*buf;

	// TODO: more than one olsr messages can be in one packet
	int msgtype = oh->olsr_msg[0].olsr_msgtype;
	
	DEBUG("OLSR msgtype: %d\n*** ", msgtype);

	current_packet.pkt_types |= PKT_TYPE_OLSR;
	current_packet.olsr_type = msgtype;
	
	if (msgtype == LQ_HELLO_MESSAGE || msgtype == LQ_TC_MESSAGE )
		current_packet.pkt_types |= PKT_TYPE_OLSR_LQ;

	if (msgtype == HELLO_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-12) / sizeof(struct hellomsg);
		DEBUG("HELLO %d\n", number);
		current_packet.olsr_neigh = number;
	}

	if (msgtype == LQ_HELLO_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-16) / 12;
		DEBUG("LQ_HELLO %d (%d)\n", number, (ntohs(oh->olsr_msg[0].olsr_msgsize)-16));
		current_packet.olsr_neigh = number;
	}

/*	XXX: tc messages are relayed. so we would have to find the originating node (IP)
	and store the information there. skip for now */
/*
	if (msgtype == TC_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-12) / sizeof(struct tcmsg);
		DEBUG("TC %d\n", number);
		current_packet.olsr_tc = number;
	}

	if (msgtype == LQ_TC_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-16) / 8;
		DEBUG("LQ_TC %d (%d)\n", number, (ntohs(oh->olsr_msg[0].olsr_msgsize)-16));
		current_packet.olsr_tc = number;
	}
*/	
	if (msgtype == HNA_MESSAGE) {
		/* same here, but we assume that nodes which relay a HNA with a default gateway
		know how to contact the gw, so have a indirect connection to a GW themselves */
		struct hnapair* hna;
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-12) / sizeof(struct hnapair);
		DEBUG("HNA NUM: %d (%d) [%d]\n", number, ntohs(oh->olsr_msg[0].olsr_msgsize),sizeof(struct hnapair) );
		for (i=0; i<number; i++) {
			hna = &(oh->olsr_msg[0].message.hna.hna_net[i]);
			DEBUG("HNA %s", ip_sprintf(hna->addr));
			DEBUG("/%s\n", ip_sprintf(hna->netmask));
			if (hna->addr==0 && hna->netmask==0)
				current_packet.pkt_types |= PKT_TYPE_OLSR_GW;
		}
	}

	return 0;
}
