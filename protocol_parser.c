/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2014 Bruno Randolf (br1@einfach.org)
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
#define __FAVOR_BSD
#include <netinet/udp.h>

#include "prism_header.h"
#include "ieee80211.h"
#include "ieee80211_util.h"
#include "olsr_header.h"
#include "batman_header.h"
#include "batman_adv_header-14.h"
#include "protocol_parser.h"
#include "main.h"
#include "util.h"
#include "radiotap/radiotap.h"
#include "radiotap/radiotap_iter.h"

static int parse_prism_header(unsigned char** buf, int len, struct packet_info* p);
static int parse_radiotap_header(unsigned char** buf, int len, struct packet_info* p);
static int parse_80211_header(unsigned char** buf, int len, struct packet_info* p);
static int parse_llc(unsigned char** buf, int len, struct packet_info* p);
static int parse_ip_header(unsigned char** buf, int len, struct packet_info* p);
static int parse_udp_header(unsigned char** buf, int len, struct packet_info* p);
static int parse_olsr_packet(unsigned char** buf, int len, struct packet_info* p);
static int parse_batman_packet(unsigned char** buf, int len, struct packet_info* p);
static int parse_batman_adv_packet(unsigned char** buf, int len, struct packet_info* p);
static int parse_meshcruzer_packet(unsigned char** buf, int len, struct packet_info* p, int port);


/* return 1 if we parsed enough = min ieee header */
int
parse_packet(unsigned char* buf, int len, struct packet_info* p)
{
	if (conf.arphrd == ARPHRD_IEEE80211_PRISM) {
		len = parse_prism_header(&buf, len, p);
		if (len <= 0)
			return 0;
	}
	else if (conf.arphrd == ARPHRD_IEEE80211_RADIOTAP) {
		len = parse_radiotap_header(&buf, len, p);
		if (len == -1) /* Bad FCS, allow packet but stop parsing */
			return 1;
		else if (len <= 0)
			return 0;
	}

	if (conf.arphrd == ARPHRD_IEEE80211_PRISM ||
	    conf.arphrd == ARPHRD_IEEE80211_RADIOTAP) {
		DEBUG("before parse 80211 len: %d\n", len);
		len = parse_80211_header(&buf, len, p);
		if (len < 0) /* couldnt parse */
			return 0;
		else if (len == 0)
			return 1;
	}

	len = parse_llc(&buf, len, p);
	if (len <= 0)
		return 1;

	len = parse_ip_header(&buf, len, p);
	if (len <= 0)
		return 1;

	len = parse_udp_header(&buf, len, p);
	if (len <= 0)
		return 1;

	return 1;
}


static int
parse_prism_header(unsigned char** buf, int len, struct packet_info* p)
{
	wlan_ng_prism2_header* ph;

	DEBUG("PRISM2 HEADER\n");

	if (len > 0 && (size_t)len < sizeof(wlan_ng_prism2_header))
		return -1;

	ph = (wlan_ng_prism2_header*)*buf;

	/*
	 * different drivers report S/N and rssi values differently
	 * let's make sure here that SNR is always positive, so we
	 * don't have do handle special cases later
	*/
	if (((int)ph->noise.data) < 0) {
		/* new madwifi */
		p->phy_signal = ph->signal.data;
		p->phy_noise = ph->noise.data;
		p->phy_snr = ph->rssi.data;
	}
	else if (((int)ph->rssi.data) < 0) {
		/* broadcom hack */
		p->phy_signal = ph->rssi.data;
		p->phy_noise = -95;
		p->phy_snr = 95 + ph->rssi.data;
	}
	else {
		/* assume hostap */
		p->phy_signal = ph->signal.data;
		p->phy_noise = ph->noise.data;
		p->phy_snr = ph->signal.data - ph->noise.data; //XXX rssi?
	}

	p->phy_rate = ph->rate.data * 10;

	/* just in case...*/
	if (p->phy_snr > 99)
		p->phy_snr = 99;
	if (p->phy_rate == 0 || p->phy_rate > 1080) {
		/* assume min rate, guess mode from channel */
		DEBUG("*** fixing wrong rate\n");
		if (ph->channel.data > 14)
			p->phy_rate = 120; /* 6 * 2 */
		else
			p->phy_rate = 20; /* 1 * 2 */
	}

	p->phy_rate_idx = rate_to_index(p->phy_rate);

	/* guess phy mode */
	if (ph->channel.data > 14)
		p->phy_flags |= PHY_FLAG_A;
	else
		p->phy_flags |= PHY_FLAG_G;
	/* always assume shortpre */
	p->phy_flags |= PHY_FLAG_SHORTPRE;

	DEBUG("devname: %s\n", ph->devname);
	DEBUG("signal: %d -> %d\n", ph->signal.data, p->phy_signal);
	DEBUG("noise: %d -> %d\n", ph->noise.data, p->phy_noise);
	DEBUG("rate: %d\n", ph->rate.data);
	DEBUG("rssi: %d\n", ph->rssi.data);
	DEBUG("*** SNR %d\n", p->phy_snr);

	*buf = *buf + sizeof(wlan_ng_prism2_header);
	return len - sizeof(wlan_ng_prism2_header);
}


static void
get_radiotap_info(struct ieee80211_radiotap_iterator *iter, struct packet_info* p)
{
	uint16_t x;
	char c;
	unsigned char known, flags, ht20, lgi;

	switch (iter->this_arg_index) {
	case IEEE80211_RADIOTAP_TSFT:
	case IEEE80211_RADIOTAP_FHSS:
	case IEEE80211_RADIOTAP_LOCK_QUALITY:
	case IEEE80211_RADIOTAP_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DBM_TX_POWER:
	case IEEE80211_RADIOTAP_TX_FLAGS:
	case IEEE80211_RADIOTAP_RX_FLAGS:
	case IEEE80211_RADIOTAP_RTS_RETRIES:
	case IEEE80211_RADIOTAP_DATA_RETRIES:
		break;
	case IEEE80211_RADIOTAP_FLAGS:
		/* short preamble */
		DEBUG("[flags %0x", *iter->this_arg);
		if (*iter->this_arg & IEEE80211_RADIOTAP_F_SHORTPRE) {
			p->phy_flags |= PHY_FLAG_SHORTPRE;
			DEBUG(" shortpre");
		}
		if (*iter->this_arg & IEEE80211_RADIOTAP_F_BADFCS) {
			p->phy_flags |= PHY_FLAG_BADFCS;
			p->pkt_types |= PKT_TYPE_BADFCS;
			DEBUG(" badfcs");
		}
		DEBUG("]");
		break;
	case IEEE80211_RADIOTAP_RATE:
		//TODO check!
		//printf("\trate: %lf\n", (double)*iter->this_arg/2);
		DEBUG("[rate %0x]", *iter->this_arg);
		p->phy_rate = (*iter->this_arg)*5; /* rate is in 500kbps */
		p->phy_rate_idx = rate_to_index(p->phy_rate);
		break;
#define IEEE80211_CHAN_A \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define IEEE80211_CHAN_B \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define IEEE80211_CHAN_G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)
	case IEEE80211_RADIOTAP_CHANNEL:
		/* channel & channel type */
		p->phy_freq = le16toh(*(uint16_t*)iter->this_arg);
		p->phy_chan =
			ieee80211_frequency_to_channel(p->phy_freq);
		DEBUG("[freq %d chan %d", p->phy_freq, p->phy_chan);
		iter->this_arg = iter->this_arg + 2;
		x = le16toh(*(uint16_t*)iter->this_arg);
		if (x & IEEE80211_CHAN_A) {
			p->phy_flags |= PHY_FLAG_A;
			DEBUG("A]");
		}
		else if (x & IEEE80211_CHAN_G) {
			p->phy_flags |= PHY_FLAG_G;
			DEBUG("G]");
		}
		else if (x & IEEE80211_CHAN_B) {
			p->phy_flags |= PHY_FLAG_B;
			DEBUG("B]");
		}
		break;
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
		c = *(char*)iter->this_arg;
		DEBUG("[sig %0d]", c);
		/* we get the signal per rx chain with newer drivers.
		 * save the highest value, but make sure we don't override
		 * with invalid values */
		if (c < 0 && (p->phy_signal == 0 || c > p->phy_signal))
			p->phy_signal = c;
		break;
	case IEEE80211_RADIOTAP_DBM_ANTNOISE:
		DEBUG("[noi %0x]", *(char*)iter->this_arg);
		p->phy_noise = *(char*)iter->this_arg;
		break;
	case IEEE80211_RADIOTAP_ANTENNA:
		DEBUG("[ant %0x]", *iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
		DEBUG("[snr %0x]", *iter->this_arg);
		p->phy_snr = *iter->this_arg;
		break;
	case IEEE80211_RADIOTAP_DB_ANTNOISE:
		//printf("\tantnoise: %02d\n", *iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_MCS:
		/* Ref http://www.radiotap.org/defined-fields/MCS */
		known = *iter->this_arg++;
		flags = *iter->this_arg++;
		DEBUG("[MCS known %0x flags %0x index %0x]", known, flags, *iter->this_arg);
		if (known & IEEE80211_RADIOTAP_MCS_HAVE_BW)
			ht20 = (flags & IEEE80211_RADIOTAP_MCS_BW_MASK) == IEEE80211_RADIOTAP_MCS_BW_20;
		else
			ht20 = 1; /* assume HT20 if not present */

		if (known & IEEE80211_RADIOTAP_MCS_HAVE_GI)
			lgi = !(flags & IEEE80211_RADIOTAP_MCS_SGI);
		else
			lgi = 1; /* assume long GI if not present */

		DEBUG(" %s %s", ht20 ? "HT20" : "HT40", lgi ? "LGI" : "SGI");

		p->phy_rate_idx = 12 + *iter->this_arg;
		p->phy_rate_flags = flags;
		p->phy_rate = mcs_index_to_rate(*iter->this_arg, ht20, lgi);

		DEBUG(" RATE %d ", p->phy_rate);
		break;
	default:
		printf("\tBOGUS DATA\n");
		break;
	}
}

static int
parse_radiotap_header(unsigned char** buf, int len, struct packet_info* p)
{
	struct ieee80211_radiotap_header* rh;
	struct ieee80211_radiotap_iterator iter;
	int err, rt_len;

	rh = (struct ieee80211_radiotap_header*)*buf;
	rt_len = le16toh(rh->it_len);

	err = ieee80211_radiotap_iterator_init(&iter, rh, rt_len, NULL);
	if (err) {
		DEBUG("malformed radiotap header (init returns %d)\n", err);
		return -2;
	}

	while (!(err = ieee80211_radiotap_iterator_next(&iter))) {
		if (iter.is_radiotap_ns) {
			get_radiotap_info(&iter, p);
		}
	}

	DEBUG("\n");
	DEBUG("SIG %d NOI %d SNR %d\n", p->phy_signal, p->phy_noise, p->phy_snr);

	if (p->phy_signal > 0 || p->phy_signal < -95)
		p->phy_signal = 0;

	/* no SNR from radiotap, try to calculate, normal case nowadays */
	if (p->phy_snr == 0 && p->phy_signal < 0) {
		if (p->phy_noise < 0) {
			p->phy_snr = p->phy_signal - p->phy_noise;
		} else {
			/* HACK: here we just assume noise to be -95dBm */
			p->phy_snr = p->phy_signal + 95;
		}
	}

	/* sanitize */
	if (p->phy_snr > 99)
		p->phy_snr = 99;
	if (p->phy_rate == 0 || p->phy_rate > 6000) {
		/* assume min rate for mode */
		DEBUG("*** fixing wrong rate\n");
		if (p->phy_flags & PHY_FLAG_A)
			p->phy_rate = 120; /* 6 * 2 */
		else if (p->phy_flags & PHY_FLAG_B)
			p->phy_rate = 20; /* 1 * 2 */
		else if (p->phy_flags & PHY_FLAG_G)
			p->phy_rate = 120; /* 6 * 2 */
		else
			p->phy_rate = 20;
	}

	DEBUG("\nrate: %.2f\n", (float)p->phy_rate/10);
	DEBUG("rate_idx: %d\n", p->phy_rate_idx);
	DEBUG("signal: %d\n", p->phy_signal);
	DEBUG("noise: %d\n", p->phy_noise);
	DEBUG("snr: %d\n", p->phy_snr);

	if (p->phy_flags & PHY_FLAG_BADFCS) {
		/* we can't trust frames with a bad FCS - stop parsing */
		DEBUG("=== bad FCS, stop ===\n");
		return -1;
	} else {
		*buf = *buf + rt_len;
		return len - rt_len;
	}
}


static int
parse_80211_header(unsigned char** buf, int len, struct packet_info* p)
{
	struct ieee80211_hdr* wh;
	struct ieee80211_mgmt* whm;
	int hdrlen;
	u8* ra = NULL;
	u8* ta = NULL;
	u8* bssid = NULL;
	u16 fc, cap_i;

	if (len < 2) /* not even enough space for fc */
		return -1;

	wh = (struct ieee80211_hdr*)*buf;
	fc = le16toh(wh->frame_control);
	hdrlen = ieee80211_get_hdrlen(fc);

	DEBUG("len %d hdrlen %d\n", len, hdrlen);

	if (len < hdrlen)
		return -1;

	p->wlan_len = len;
	p->wlan_type = (fc & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE));

	DEBUG("wlan_type %x - type %x - stype %x\n", fc, fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE );

	DEBUG("%s\n", get_packet_type_name(fc));

	bssid = ieee80211_get_bssid(wh, len);

	switch (p->wlan_type & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_DATA:
		p->pkt_types |= PKT_TYPE_DATA;
		switch (p->wlan_type & IEEE80211_FCTL_STYPE) {
		case IEEE80211_STYPE_NULLFUNC:
			p->pkt_types |= PKT_TYPE_NULL;
			break;
		case IEEE80211_STYPE_QOS_DATA:
			/* TODO: ouch, should properly define a qos header */
			p->pkt_types |= PKT_TYPE_QDATA;
			p->wlan_qos_class = wh->addr4[0] & 0x7;
			DEBUG("***QDATA %x\n", p->wlan_qos_class);
			break;
		}
		p->wlan_nav = le16toh(wh->duration_id);
		DEBUG("DATA NAV %d\n", p->wlan_nav);
		p->wlan_seqno = le16toh(wh->seq_ctrl);
		DEBUG("DATA SEQ %d\n", p->wlan_seqno);

		DEBUG("A1 %s\n", ether_sprintf(wh->addr1));
		DEBUG("A2 %s\n", ether_sprintf(wh->addr2));
		DEBUG("A3 %s\n", ether_sprintf(wh->addr3));
		DEBUG("A4 %s\n", ether_sprintf(wh->addr4));
		DEBUG("ToDS %d FromDS %d\n", (fc & IEEE80211_FCTL_FROMDS) != 0, (fc & IEEE80211_FCTL_TODS) != 0);

		ra = wh->addr1;
		ta = wh->addr2;
		//sa = ieee80211_get_SA(wh);
		//da = ieee80211_get_DA(wh);

		/* AP, STA or IBSS */
		if ((fc & IEEE80211_FCTL_FROMDS) == 0 &&
		    (fc & IEEE80211_FCTL_TODS) == 0)
			p->wlan_mode = WLAN_MODE_IBSS;
		else if ((fc & IEEE80211_FCTL_FROMDS) &&
			 (fc & IEEE80211_FCTL_TODS))
			p->wlan_mode = WLAN_MODE_4ADDR;
		else if (fc & IEEE80211_FCTL_FROMDS)
			p->wlan_mode = WLAN_MODE_AP;
		else if (fc & IEEE80211_FCTL_TODS)
			p->wlan_mode = WLAN_MODE_STA;

		/* WEP */
		if (fc & IEEE80211_FCTL_PROTECTED)
			p->wlan_wep = 1;

		if (fc & IEEE80211_FCTL_RETRY)
			p->wlan_retry = 1;

		break;

	case IEEE80211_FTYPE_CTL:
		p->pkt_types |= PKT_TYPE_CTRL;
		switch (p->wlan_type & IEEE80211_FCTL_STYPE) {
		case IEEE80211_STYPE_RTS:
			p->pkt_types |= PKT_TYPE_RTS;
			p->wlan_nav = le16toh(wh->duration_id);
			DEBUG("RTS NAV %d\n", p->wlan_nav);
			ra = wh->addr1;
			ta = wh->addr2;
			break;

		case IEEE80211_STYPE_CTS:
			p->pkt_types |= PKT_TYPE_CTS;
			p->wlan_nav = le16toh(wh->duration_id);
			DEBUG("CTS NAV %d\n", p->wlan_nav);
			ra = wh->addr1;
			break;

		case IEEE80211_STYPE_ACK:
			p->pkt_types |= PKT_TYPE_ACK;
			p->wlan_nav = le16toh(wh->duration_id);
			DEBUG("ACK NAV %d\n", p->wlan_nav);
			ra = wh->addr1;
			break;

		case IEEE80211_STYPE_PSPOLL:
			ra = wh->addr1;
			bssid = wh->addr1;
			ta = wh->addr2;
			break;

		case IEEE80211_STYPE_CFEND:
		case IEEE80211_STYPE_CFENDACK:
			ra = wh->addr1;
			ta = wh->addr2;
			bssid = wh->addr2;
			break;

		case IEEE80211_STYPE_BACK_REQ:
		case IEEE80211_STYPE_BACK:
			p->pkt_types |= PKT_TYPE_ACK;
			p->wlan_nav = le16toh(wh->duration_id);
			ra = wh->addr1;
			ta = wh->addr2;
		}
		break;

	case IEEE80211_FTYPE_MGMT:
		p->pkt_types |= PKT_TYPE_MGMT;
		whm = (struct ieee80211_mgmt*)*buf;
		ta = whm->sa;
		ra = whm->da;
		bssid = whm->bssid;
		p->wlan_seqno = le16toh(wh->seq_ctrl);
		DEBUG("MGMT SEQ %d\n", p->wlan_seqno);

		if (fc & IEEE80211_FCTL_RETRY)
			p->wlan_retry = 1;

		switch (p->wlan_type & IEEE80211_FCTL_STYPE) {
		case IEEE80211_STYPE_BEACON:
			p->pkt_types |= PKT_TYPE_BEACON;
			p->wlan_tsf = le64toh(whm->u.beacon.timestamp);
			p->wlan_bintval = le16toh(whm->u.beacon.beacon_int);
			ieee802_11_parse_elems(whm->u.beacon.variable,
				len - sizeof(struct ieee80211_mgmt) - 4 /* FCS */, p);
			DEBUG("ESSID %s \n", p->wlan_essid );
			DEBUG("CHAN %d \n", p->wlan_channel );
			cap_i = le16toh(whm->u.beacon.capab_info);
			if (cap_i & WLAN_CAPABILITY_IBSS)
				p->wlan_mode = WLAN_MODE_IBSS;
			else if (cap_i & WLAN_CAPABILITY_ESS)
				p->wlan_mode = WLAN_MODE_AP;
			if (cap_i & WLAN_CAPABILITY_PRIVACY)
				p->wlan_wep = 1;
			break;

		case IEEE80211_STYPE_PROBE_RESP:
			p->pkt_types |= PKT_TYPE_PROBE;
			p->wlan_tsf = le64toh(whm->u.beacon.timestamp);
			ieee802_11_parse_elems(whm->u.beacon.variable,
				len - sizeof(struct ieee80211_mgmt) - 4 /* FCS */, p);
			DEBUG("ESSID %s \n", p->wlan_essid );
			DEBUG("CHAN %d \n", p->wlan_channel );
			cap_i = le16toh(whm->u.beacon.capab_info);
			if (cap_i & WLAN_CAPABILITY_IBSS)
				p->wlan_mode = WLAN_MODE_IBSS;
			else if (cap_i & WLAN_CAPABILITY_ESS)
				p->wlan_mode = WLAN_MODE_AP;
			if (cap_i & WLAN_CAPABILITY_PRIVACY)
				p->wlan_wep = 1;
			break;

		case IEEE80211_STYPE_PROBE_REQ:
			p->pkt_types |= PKT_TYPE_PROBE;
			ieee802_11_parse_elems(whm->u.probe_req.variable,
				len - 24 - 4 /* FCS */,
				p);
			p->wlan_mode = WLAN_MODE_PROBE;
			break;

		case IEEE80211_STYPE_ASSOC_REQ:
		case IEEE80211_STYPE_ASSOC_RESP:
		case IEEE80211_STYPE_REASSOC_REQ:
		case IEEE80211_STYPE_REASSOC_RESP:
		case IEEE80211_STYPE_DISASSOC:
			p->pkt_types |= PKT_TYPE_ASSOC;
			break;

		case IEEE80211_STYPE_AUTH:
			if (fc & IEEE80211_FCTL_PROTECTED)
				p->wlan_wep = 1;
				/* no break */
		case IEEE80211_STYPE_DEAUTH:
			p->pkt_types |= PKT_TYPE_AUTH;
			break;

		case IEEE80211_STYPE_ACTION:
			break;
		}
		break;
	}

	if (ta != NULL) {
		memcpy(p->wlan_src, ta, MAC_LEN);
		DEBUG("TA    %s\n", ether_sprintf(ta));
	}
	if (ra != NULL) {
		memcpy(p->wlan_dst, ra, MAC_LEN);
		DEBUG("RA    %s\n", ether_sprintf(ra));
	}
	if (bssid != NULL) {
		memcpy(p->wlan_bssid, bssid, MAC_LEN);
		DEBUG("BSSID %s\n", ether_sprintf(bssid));
	}

	/* only data frames contain more info, otherwise stop parsing */
	if (IEEE80211_IS_DATA(p->wlan_type) && p->wlan_wep != 1) {
		*buf = *buf + hdrlen;
		return len - hdrlen;
	}
	return 0;
}


static int
parse_llc(unsigned char ** buf, int len, struct packet_info* p)
{
	DEBUG("* parse LLC\n");

	if (len < 6)
		return -1;

	/* check type in LLC header */
	*buf = *buf + 6;

	if (ntohs(*((uint16_t*)*buf)) == 0x4305) {
		DEBUG("BATMAN-ADV\n");
		(*buf)++; (*buf)++;
		return parse_batman_adv_packet(buf, len - 8, p);
	}
	else {
		if (**buf != 0x08)
			return -1;
		(*buf)++;
		if (**buf == 0x06) { /* ARP */
			p->pkt_types |= PKT_TYPE_ARP;
			return 0;
		}
		if (**buf != 0x00)  /* not IP */
			return -1;
		(*buf)++;

		DEBUG("* parse LLC left %d\n", len - 8);

		return len - 8;
	}
}


static int
parse_batman_adv_packet(unsigned char** buf, int len, struct packet_info* p) {
	struct batman_ogm_packet *bp;
	//batadv_ogm_packet
	bp = (struct batman_ogm_packet*)*buf;

	p->pkt_types |= PKT_TYPE_BATADV;
	p->bat_version = bp->version;
	p->bat_packet_type = bp->packet_type;

	DEBUG("parse bat len %d type %d vers %d\n", len, bp->packet_type, bp->version);

	/* version 14 */
	if (bp->version == 14) {
		switch (bp->packet_type) {
		case BAT_OGM:
			DEBUG("OGM\n");
			return 0;
		case BAT_ICMP:
			DEBUG("ICMP\n");
			break;
		case BAT_UNICAST:
			DEBUG("UNI %lu\n", sizeof(struct unicast_packet));
			*buf = *buf + sizeof(struct unicast_packet) + 14;
			return len - sizeof(struct unicast_packet) - 14;
		case BAT_BCAST:
			DEBUG("BCAST\n");
			break;
		case BAT_VIS:
		case BAT_UNICAST_FRAG:
		case BAT_TT_QUERY:
		case BAT_ROAM_ADV:
			break;
		}
	}

	return 0;
}


static int
parse_ip_header(unsigned char** buf, int len, struct packet_info* p)
{
	struct ip* ih;

	DEBUG("* parse IP\n");

	if (len > 0 && (size_t)len < sizeof(struct ip))
		return -1;

	ih = (struct ip*)*buf;

	DEBUG("*** IP SRC: %s\n", ip_sprintf(ih->ip_src.s_addr));
	DEBUG("*** IP DST: %s\n", ip_sprintf(ih->ip_dst.s_addr));
	p->ip_src = ih->ip_src.s_addr;
	p->ip_dst = ih->ip_dst.s_addr;
	p->pkt_types |= PKT_TYPE_IP;

	DEBUG("IP proto: %d\n", ih->ip_p);
	switch (ih->ip_p) {
	case IPPROTO_UDP: p->pkt_types |= PKT_TYPE_UDP; break;
	/* all others set the type and return. no more parsing */
	case IPPROTO_ICMP: p->pkt_types |= PKT_TYPE_ICMP; return 0;
	case IPPROTO_TCP: p->pkt_types |= PKT_TYPE_TCP; return 0;
	}


	*buf = *buf + ih->ip_hl * 4;
	return len - ih->ip_hl * 4;
}


static int
parse_udp_header(unsigned char** buf, int len, struct packet_info* p)
{
	struct udphdr* uh;

	if (len > 0 && (size_t)len < sizeof(struct udphdr))
		return -1;

	uh = (struct udphdr*)*buf;

	DEBUG("UPD dest port: %d\n", ntohs(uh->uh_dport));

	p->tcpudp_port = ntohs(uh->uh_dport);

	*buf = *buf + 8;
	len = len - 8;

	if (p->tcpudp_port == 698) /* OLSR */
		return parse_olsr_packet(buf, len, p);

	if (p->tcpudp_port == BAT_PORT) /* batman */
		return parse_batman_packet(buf, len, p);

	if (p->tcpudp_port == 9256 || p->tcpudp_port == 9257 ) /* MeshCruzer */
		return parse_meshcruzer_packet(buf, len, p, p->tcpudp_port);

	return 0;
}


static int
parse_olsr_packet(unsigned char** buf, int len, struct packet_info* p)
{
	struct olsr* oh;
	int number, i, msgtype;

	if (len > 0 && (size_t)len < sizeof(struct olsr))
		return -1;

	oh = (struct olsr*)*buf;

	// TODO: more than one olsr messages can be in one packet
	msgtype = oh->olsr_msg[0].olsr_msgtype;

	DEBUG("OLSR msgtype: %d\n*** ", msgtype);

	p->pkt_types |= PKT_TYPE_OLSR;
	p->olsr_type = msgtype;

	if (msgtype == LQ_HELLO_MESSAGE || msgtype == LQ_TC_MESSAGE )
		p->pkt_types |= PKT_TYPE_OLSR_LQ;

	if (msgtype == HELLO_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize) - 12) / sizeof(struct hellomsg);
		DEBUG("HELLO %d\n", number);
		p->olsr_neigh = number;
	}

	if (msgtype == LQ_HELLO_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize) - 16) / 12;
		DEBUG("LQ_HELLO %d (%d)\n", number, (ntohs(oh->olsr_msg[0].olsr_msgsize) - 16));
		p->olsr_neigh = number;
	}
#if 0
/*	XXX: tc messages are relayed. so we would have to find the originating node (IP)
	and store the information there. skip for now */

	if (msgtype == TC_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-12) / sizeof(struct tcmsg);
		DEBUG("TC %d\n", number);
		p->olsr_tc = number;
	}

	if (msgtype == LQ_TC_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-16) / 8;
		DEBUG("LQ_TC %d (%d)\n", number, (ntohs(oh->olsr_msg[0].olsr_msgsize)-16));
		p->olsr_tc = number;
	}
#endif
	if (msgtype == HNA_MESSAGE) {
		/* same here, but we assume that nodes which relay a HNA with a default gateway
		know how to contact the gw, so have a indirect connection to a GW themselves */
		struct hnapair* hna;
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize) - 12) / sizeof(struct hnapair);
		DEBUG("HNA NUM: %d (%d) [%d]\n", number, ntohs(oh->olsr_msg[0].olsr_msgsize),
			(int)sizeof(struct hnapair) );
		for (i = 0; i < number; i++) {
			hna = &(oh->olsr_msg[0].message.hna.hna_net[i]);
			DEBUG("HNA %s", ip_sprintf(hna->addr));
			DEBUG("/%s\n", ip_sprintf(hna->netmask));
			if (hna->addr == 0 && hna->netmask == 0)
				p->pkt_types |= PKT_TYPE_OLSR_GW;
		}
	}
	/* done for good */
	return 0;
}


static int
parse_batman_packet(__attribute__((unused)) unsigned char** buf,
		    __attribute__((unused)) int len,
		    __attribute__((unused)) struct packet_info* p)
{
	p->pkt_types |= PKT_TYPE_BATMAN;

	return 0;
}


static int
parse_meshcruzer_packet(__attribute__((unused)) unsigned char** buf,
			__attribute__((unused)) int len,
			__attribute__((unused)) struct packet_info* p,
			__attribute__((unused)) int port)
{
	p->pkt_types |= PKT_TYPE_MESHZ;

	return 0;
}
