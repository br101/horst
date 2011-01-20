/* copied from linux wireless-2.6/net/mac80211/util.c */

/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * utilities for mac80211
 */

#include <stddef.h>
#include <string.h>
#include <math.h>

#include "ieee80211.h"
#include "ieee80211_radiotap.h"
#include "ieee80211_util.h"
#include "main.h"
#include "util.h"


u8*
ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len)
{
	__le16 fc;

	if (len < 24)
		return NULL;

	fc = le16_to_cpu(hdr->frame_control);

	switch (fc & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_DATA:
		switch (fc & (IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS)) {
		case IEEE80211_FCTL_TODS:
			return hdr->addr1;
		case (IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS):
			return NULL;
		case IEEE80211_FCTL_FROMDS:
			return hdr->addr2;
		case 0:
			return hdr->addr3;
		}
		break;
	case IEEE80211_FTYPE_MGMT:
		return hdr->addr3;
	case IEEE80211_FTYPE_CTL:
		if ((fc & IEEE80211_FCTL_STYPE) == IEEE80211_STYPE_PSPOLL)
			return hdr->addr1;
		else
			return NULL;
	}

	return NULL;
}


int
ieee80211_get_hdrlen(u16 fc)
{
	int hdrlen = 24;

	switch (fc & IEEE80211_FCTL_FTYPE) {
	case IEEE80211_FTYPE_DATA:
		if ((fc & IEEE80211_FCTL_FROMDS) && (fc & IEEE80211_FCTL_TODS))
			hdrlen = 30; /* Addr4 */
		/*
		 * The QoS Control field is two bytes and its presence is
		 * indicated by the IEEE80211_STYPE_QOS_DATA bit. Add 2 to
		 * hdrlen if that bit is set.
		 * This works by masking out the bit and shifting it to
		 * bit position 1 so the result has the value 0 or 2.
		 */
		hdrlen += (fc & IEEE80211_STYPE_QOS_DATA) >> 6;
		break;
	case IEEE80211_FTYPE_CTL:
		/*
		 * ACK and CTS are 10 bytes, all others 16. To see how
		 * to get this condition consider
		 *   subtype mask:   0b0000000011110000 (0x00F0)
		 *   ACK subtype:    0b0000000011010000 (0x00D0)
		 *   CTS subtype:    0b0000000011000000 (0x00C0)
		 *   bits that matter:         ^^^      (0x00E0)
		 *   value of those: 0b0000000011000000 (0x00C0)
		 */
		if ((fc & 0xE0) == 0xC0)
			hdrlen = 10;
		else
			hdrlen = 16;
		break;
	}

	return hdrlen;
}

/* from mac80211/ieee80211_sta.c, modified */
void
ieee802_11_parse_elems(unsigned char *start, size_t len, struct packet_info *p)
{
	int left = len;
	unsigned char *pos = start;

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left)
			return;

		switch (id) {
		case WLAN_EID_SSID:
			memcpy(p->wlan_essid, pos, elen);
			break;
#if 0
		case WLAN_EID_SUPP_RATES:
			elems->supp_rates = pos;
			elems->supp_rates_len = elen;
			break;
		case WLAN_EID_FH_PARAMS:
			elems->fh_params = pos;
			elems->fh_params_len = elen;
			break;
#endif
		case WLAN_EID_DS_PARAMS:
			p->wlan_channel = *pos;
			break;
#if 0
		case WLAN_EID_CF_PARAMS:
			elems->cf_params = pos;
			elems->cf_params_len = elen;
			break;
		case WLAN_EID_TIM:
			elems->tim = pos;
			elems->tim_len = elen;
			break;
		case WLAN_EID_IBSS_PARAMS:
			elems->ibss_params = pos;
			elems->ibss_params_len = elen;
			break;
		case WLAN_EID_CHALLENGE:
			elems->challenge = pos;
			elems->challenge_len = elen;
			break;
		case WLAN_EID_WPA:
			if (elen >= 4 && pos[0] == 0x00 && pos[1] == 0x50 &&
			    pos[2] == 0xf2) {
				/* Microsoft OUI (00:50:F2) */
				if (pos[3] == 1) {
					/* OUI Type 1 - WPA IE */
					elems->wpa = pos;
					elems->wpa_len = elen;
				} else if (elen >= 5 && pos[3] == 2) {
					if (pos[4] == 0) {
						elems->wmm_info = pos;
						elems->wmm_info_len = elen;
					} else if (pos[4] == 1) {
						elems->wmm_param = pos;
						elems->wmm_param_len = elen;
					}
				}
			}
			break;
		case WLAN_EID_RSN:
			elems->rsn = pos;
			elems->rsn_len = elen;
			break;
		case WLAN_EID_ERP_INFO:
			elems->erp_info = pos;
			elems->erp_info_len = elen;
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			elems->ext_supp_rates = pos;
			elems->ext_supp_rates_len = elen;
			break;
#endif
		default:
			break;
		}

		left -= elen;
		pos += elen;
	}
}

/* from mac80211/ieee80211_i.c, slightly modified */

/**
 * ieee80211_is_erp_rate - Check if a rate is an ERP rate
 * @phymode: The PHY-mode for this rate (MODE_IEEE80211...)
 * @rate: Transmission rate to check, in 100 kbps
 *
 * Check if a given rate is an Extended Rate PHY (ERP) rate.
 */
static inline int
ieee80211_is_erp_rate(int phymode, int rate)
{
	if (phymode & PHY_FLAG_G) {
		if (rate != 10 && rate != 20 &&
		    rate != 55 && rate != 110) {
			DEBUG("erp\n");
			return 1;
		}
	}
	DEBUG("no erp\n");
	return 0;
}

int
get_cw_time(int cw_min, int cw_max, int retries, int slottime)
{
	int cw = pow(2, (cw_min + retries)) - 1;
	cw_max = pow(2, cw_max) - 1;

	if(cw >  cw_max)
		cw = cw_max;

	DEBUG("CW min %d max %d ret %d = %d\n", cw_min, cw_max, retries, cw);
	return (cw * slottime) / 2;
}

const unsigned char ieee802_1d_to_ac[8] = { 0, 1, 1, 0, 2, 2, 3, 3 };
				     /* BE	BK	VI	VO */
const unsigned char ac_to_aifs[4] = {	3,	7,	2,	2};
const unsigned char ac_to_cwmin[4] = {	4,	4,	3,	2};
const unsigned int ac_to_cwmax[4] = {	10,	10,	4,	3};

/* from mac80211/util.c, modified */
int
ieee80211_frame_duration(int phymode, size_t len, int rate, int short_preamble,
			 int shortslot, int type, char qos_class, int retries)
{
	int dur;
	int erp;
	int sifs, slottime;
	static int last_was_cts;

	erp = ieee80211_is_erp_rate(phymode, rate);

	/* calculate duration (in microseconds, rounded up to next higher
	 * integer if it includes a fractional microsecond) to send frame of
	 * len bytes (does not include FCS) at the given rate. Duration will
	 * also include SIFS.
	 *
	 * rate is in 100 kbps, so divident is multiplied by 10 in the
	 * DIV_ROUND_UP() operations.
	 */

	DEBUG("DUR mode %d, len %d, rate %d, shortpre %d shortslot %d type %x UP %d\n", phymode, (int)len, rate, short_preamble, shortslot, type, qos_class);

	if (phymode == PHY_FLAG_A || erp) {
		DEBUG("OFDM\n");
		/*
		 * OFDM:
		 *
		 * N_DBPS = DATARATE x 4
		 * N_SYM = Ceiling((16+8xLENGTH+6) / N_DBPS)
		 *	(16 = SIGNAL time, 6 = tail bits)
		 * TXTIME = T_PREAMBLE + T_SIGNAL + T_SYM x N_SYM + Signal Ext
		 *
		 * T_SYM = 4 usec
		 * 802.11a - 17.5.2: aSIFSTime = 16 usec
		 * 802.11g - 19.8.4: aSIFSTime = 10 usec +
		 *	signal ext = 6 usec
		 */
		sifs = 16;  /* SIFS + signal ext */
		slottime = 9;
		dur = 16; /* 17.3.2.3: T_PREAMBLE = 16 usec */
		dur += 4; /* 17.3.2.3: T_SIGNAL = 4 usec */
		dur += 4 * DIV_ROUND_UP((16 + 8 * (len + 4) + 6) * 10,
					4 * rate); /* T_SYM x N_SYM */
	} else {
		DEBUG("CCK\n");
		/*
		 * 802.11b or 802.11g with 802.11b compatibility:
		 * 18.3.4: TXTIME = PreambleLength + PLCPHeaderTime +
		 * Ceiling(((LENGTH+PBCC)x8)/DATARATE). PBCC=0.
		 *
		 * 802.11 (DS): 15.3.3, 802.11b: 18.3.4
		 * aSIFSTime = 10 usec
		 * aPreambleLength = 144 usec or 72 usec with short preamble
		 * aPLCPHeaderLength = 48 usec or 24 usec with short preamble
		 */
		sifs = 10; /* aSIFSTime = 10 usec */
		slottime = shortslot ? 9 : 20;
		dur = short_preamble ? (72 + 24) : (144 + 48);
		dur += DIV_ROUND_UP(8 * (len + 4) * 10, rate);
	}

	if (IEEE80211_IS_CTRL_STYPE(type, IEEE80211_STYPE_CTS) ||
	    IEEE80211_IS_CTRL_STYPE(type, IEEE80211_STYPE_ACK)) {
		//TODO: also fragments
		DEBUG("DUR SIFS\n");
		dur += sifs;
	}
	else if (IEEE80211_IS_MGMT_STYPE(type, IEEE80211_STYPE_BEACON)) {
		/* TODO: which AIFS and CW should be used for beacons? */
		dur += sifs + (2 * slottime); /* AIFS */
		dur += (slottime * 1) / 2; /* contention */
	}
	else if (IEEE80211_IS_DATA(type) && last_was_cts) {
		DEBUG("DUR LAST CTS\n");
		dur += sifs;
	}
	else if (IEEE80211_IS_DATA_STYPE(type, IEEE80211_STYPE_QOS_DATA)) {
		unsigned char ac = ieee802_1d_to_ac[(unsigned char)qos_class];
		dur += sifs + (ac_to_aifs[ac] * slottime); /* AIFS */
		dur += get_cw_time(ac_to_cwmin[ac], ac_to_cwmax[ac], retries, slottime);
		DEBUG("DUR AIFS %d CWMIN %d AC %d, UP %d\n", ac_to_aifs[ac], ac_to_cwmin[ac], ac, qos_class);
	}
	else {
		DEBUG("DUR DIFS\n");
		dur += sifs + (2 * slottime); /* DIFS */
		dur += get_cw_time(4, 10, retries, slottime);
	}

	if (IEEE80211_IS_CTRL_STYPE(type, IEEE80211_STYPE_CTS)) {
		DEBUG("SET CTS\n");
		last_was_cts = 1;
	}
	else
		last_was_cts = 0;

	/* TODO: Add EIFS (SIFS + ACKTXTIME) to frames with CRC errors, if we can get them */

	DEBUG("DUR %d\n", dur);
	return dur;
}

int
ieee80211_frequency_to_channel(int freq)
{
	int base;

	if (freq == 2484)
		return 14;
	if (freq < 2484)
		base = 2407;
	else if (freq >= 4910 && freq <= 4980)
		base = 4000;
	else
		base = 5000;
	return (freq - base) / 5;
}
