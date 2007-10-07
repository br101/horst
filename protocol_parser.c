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
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <sys/socket.h>
#include <linux/if_arp.h>

#include "ieee80211_radiotap.h"

#include "protocol_parser.h"
#include "main.h"
#include "display.h"

/* compatibility with old kernel includes */
#ifndef bswap_16
#define bswap_16(x) (((x) & 0x00ff) << 8 | ((x) & 0xff00) >> 8)
#endif

#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803    /* IEEE 802.11 + radiotap header */
#endif

#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802      /* IEEE 802.11 + Prism2 header  */
#endif

static unsigned char* parse_prism_header(unsigned char* buf, int len);
static unsigned char* parse_radiotap_header(unsigned char* buf, int len);
static unsigned char* parse_80211_header(unsigned char* buf, int len);
static unsigned char* parse_ip_header(unsigned char* buf, int len);
static unsigned char* parse_udp_header(unsigned char* buf, int len);
static unsigned char* parse_olsr_packet(unsigned char* buf, int len);

int
parse_packet(unsigned char* buf, int len)
{
	if (arphrd == ARPHRD_IEEE80211_PRISM)
		buf = parse_prism_header(buf, len);
	else if (arphrd == ARPHRD_IEEE80211_RADIOTAP)
		buf = parse_radiotap_header(buf, len);

	if (buf != NULL &&
	    (arphrd == ARPHRD_IEEE80211 ||
	    arphrd == ARPHRD_IEEE80211_PRISM ||
	    arphrd == ARPHRD_IEEE80211_RADIOTAP))
		buf = parse_80211_header(buf, len);
	if (buf != NULL)
		buf = parse_ip_header(buf, len);
	if (buf != NULL)
		buf = parse_udp_header(buf, len);
	if (buf != NULL)
		parse_olsr_packet(buf, len);
	
	if (buf != NULL) /* parsing successful */
		return 1;
	return 0;
}

static unsigned char*
parse_prism_header(unsigned char* buf, int len)
{
	wlan_ng_prism2_header* ph;
	ph = (wlan_ng_prism2_header*)buf;

	DEBUG("PRISM2 HEADER\n");

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

	DEBUG("devname: %s\n", ph->devname);
	DEBUG("signal: %d -> %d\n", ph->signal.data, current_packet.signal);
	DEBUG("noise: %d -> %d\n", ph->noise.data, current_packet.noise);
	DEBUG("rate: %d\n", ph->rate.data);
	DEBUG("rssi: %d\n", ph->rssi.data);
	DEBUG("*** SNR %d\n", current_packet.snr);

	return buf + sizeof(wlan_ng_prism2_header);
}


static unsigned char*
parse_radiotap_header(unsigned char* buf, int len)
{
	struct ath_rx_radiotap_header* rh;
	rh = (struct ath_rx_radiotap_header*)buf;

	DEBUG("RADIOTAP HEADER\n");

	current_packet.signal = rh->wr_dbm_antsignal;
	current_packet.noise = rh->wr_dbm_antnoise;
	current_packet.snr = rh->wr_antsignal;

	DEBUG("signal: %d -> %d\n", rh->wr_dbm_antsignal, current_packet.signal);
	DEBUG("noise: %d -> %d\n", rh->wr_dbm_antnoise, current_packet.noise);
	DEBUG("rssi: %d\n", rh->wr_antsignal);
	DEBUG("*** SNR %d\n", current_packet.snr);

	return buf + rh->wr_ihdr.it_len;
}


static unsigned char*
parse_80211_header(unsigned char* buf, int len)
{
	struct ieee80211_hdr* wh;
	wh = (struct ieee80211_hdr*)buf;

	DEBUG("STYPE %x\n", WLAN_FC_GET_STYPE(wh->frame_control));

	//TODO: addresses are not always like this (WDS)
	memcpy(current_packet.wlan_dst, wh->addr1, 6);
	memcpy(current_packet.wlan_src, wh->addr2, 6);
	memcpy(current_packet.wlan_bssid, wh->addr3, 6);
	current_packet.wlan_type = WLAN_FC_GET_TYPE(ntohs(bswap_16(wh->frame_control)));
	current_packet.wlan_stype = WLAN_FC_GET_STYPE(ntohs(bswap_16(wh->frame_control)));

	//TODO: other subtypes
	if (current_packet.wlan_type == WLAN_FC_TYPE_MGMT && current_packet.wlan_stype == WLAN_FC_STYPE_BEACON) {
		struct ieee80211_mgmt* whm;
		whm = (struct ieee80211_mgmt*)buf;
		memcpy(current_packet.wlan_tsf, whm->u.beacon.timestamp,8);
		if (whm->u.beacon.variable[0] == 0) { /* ESSID */
			memcpy(current_packet.wlan_essid, &whm->u.beacon.variable[2], whm->u.beacon.variable[1]);
			current_packet.wlan_essid[whm->u.beacon.variable[1]]='\0';
		}
		current_packet.pkt_types = PKT_TYPE_BEACON;
		return NULL;
	}
	else if (current_packet.wlan_type == WLAN_FC_TYPE_MGMT && current_packet.wlan_stype == WLAN_FC_STYPE_PROBE_REQ) {
		current_packet.pkt_types = PKT_TYPE_PROBE_REQ;
		return NULL;
	}
	else if (current_packet.wlan_type == WLAN_FC_TYPE_DATA && current_packet.wlan_stype == WLAN_FC_STYPE_DATA) {
		current_packet.pkt_types = PKT_TYPE_DATA;
	}
	else {
		return NULL;
	}

	//TODO: 802.11 headers with 4 addresses (WDS) are longer
	return buf + IEEE80211_HEADER_LEN;
}

static unsigned char*
parse_ip_header(unsigned char* buf, int len)
{
	/* check type in LLC header */	
	buf = buf + 6;

	if (*buf != 0x08) /* not IP */
		return NULL;
	buf++;
	
	if (*buf != 0x00)
		return NULL;
	buf++;

	struct iphdr* ih;
	ih = (struct iphdr*)buf;

	DEBUG("*** IP SRC: %s\n", ip_sprintf(ih->saddr));
	DEBUG("*** IP DST: %s\n", ip_sprintf(ih->daddr));
	current_packet.ip_src = ih->saddr;
	current_packet.ip_dst = ih->daddr;
	current_packet.pkt_types |= PKT_TYPE_IP;

	DEBUG("IP proto: %d\n", ih->protocol);
	if (ih->protocol != 17) /* UDP */
		return NULL;
	return buf + ih->ihl*4;
}

static unsigned char*
parse_udp_header(unsigned char* buf, int len)
{
	struct udphdr* uh;
	uh = (struct udphdr*)buf;

	DEBUG("UPD dest port: %d\n", ntohs(uh->dest));
	if (ntohs(uh->dest) != 698) /* OLSR */
		return NULL;
	return buf + 8;
}

static unsigned char*
parse_olsr_packet(unsigned char* buf, int len)
{
	struct olsr* oh;
	oh = (struct olsr*)buf;
	int number;
	int i;

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

	return NULL;
}
