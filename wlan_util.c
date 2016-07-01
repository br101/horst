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

#include "main.h"
#include "util.h"
#include "wlan80211.h"
#include "wlan_util.h"

/* lists of packet names */

struct pkt_name stype_names[WLAN_NUM_TYPES][WLAN_NUM_STYPES] = {
{
	{ 'a', "ASOCRQ", WLAN_FRAME_ASSOC_REQ, "Association request" },
	{ 'A', "ASOCRP", WLAN_FRAME_ASSOC_RESP, "Association response" },
	{ 'o', "REASRQ", WLAN_FRAME_REASSOC_REQ, "Reassociation request" },
	{ 'O', "REASRP", WLAN_FRAME_REASSOC_RESP, "Reassociation response" },
	{ 'p', "PROBRQ", WLAN_FRAME_PROBE_REQ, "Probe request" },
	{ 'P', "PROBRP", WLAN_FRAME_PROBE_RESP, "Probe response" },
	{ 'T', "TIMING", WLAN_FRAME_TIMING, "Timing Advertisement" },
	{ '-', "-RESV-", 0x0070, "RESERVED" },
	{ 'b', "BEACON", WLAN_FRAME_BEACON, "Beacon" },
	{ 't', "ATIM",   WLAN_FRAME_ATIM, "ATIM" },
	{ 'S', "DISASC", WLAN_FRAME_DISASSOC, "Disassociation" },
	{ 'u', "AUTH",   WLAN_FRAME_AUTH, "Authentication" },
	{ 'U', "DEAUTH", WLAN_FRAME_DEAUTH, "Deauthentication" },
	{ 'X', "ACTION", WLAN_FRAME_ACTION, "Action" },
	{ 'x', "ACTNOA", WLAN_FRAME_ACTION_NOACK, "Action No Ack" },
	{ '-', "-RESV-", 0x0070, "RESERVED" },
}, {
	{ '-', "-RESV-", 0x0004, "RESERVED" },
	{ '-', "-RESV-", 0x0014, "RESERVED" },
	{ '-', "-RESV-", 0x0024, "RESERVED" },
	{ '-', "-RESV-", 0x0034, "RESERVED" },
	{ 'B', "BEAMRP", WLAN_FRAME_BEAM_REP, "Beamforming Report Poll" },
	{ 'v', "VHTNDP", WLAN_FRAME_VHT_NDP, "VHT NDP Announcement" },
	{ '-', "-RESV-", 0x0064, "RESERVED" },
	{ 'w', "CTWRAP", WLAN_FRAME_CTRL_WRAP, "Control Wrapper" },
	{ 'l', "BACKRQ", WLAN_FRAME_BLKACK_REQ, "Block Ack Request" },
	{ 'L', "BACK",   WLAN_FRAME_BLKACK, "Block Ack" },
	{ 's', "PSPOLL", WLAN_FRAME_PSPOLL, "PS-Poll" },
	{ 'R', "RTS",    WLAN_FRAME_RTS, "RTS" },
	{ 'C', "CTS",    WLAN_FRAME_CTS, "CTS" },
	{ 'k', "ACK",    WLAN_FRAME_ACK, "ACK" },
	{ 'e', "CFEND",  WLAN_FRAME_CF_END, "CF-End" },
	{ 'E', "CFENDK", WLAN_FRAME_CF_END_ACK, "CF-End + CF-Ack" },
}, {
	{ 'D', "DATA",   WLAN_FRAME_DATA, "Data" },
	{ 'F', "DCFACK", WLAN_FRAME_DATA_CF_ACK, "Data + CF-Ack" },
	{ 'g', "DCFPLL", WLAN_FRAME_DATA_CF_POLL, "Data + CF-Poll" },
	{ 'G', "DCFKPL", WLAN_FRAME_DATA_CF_ACKPOLL, "Data + CF-Ack + CF-Poll" },
	{ 'n', "NULL",   WLAN_FRAME_NULL, "Null (no data)" },
	{ 'h', "CFACK",  WLAN_FRAME_CF_ACK, "CF-Ack (no data)" },
	{ 'H', "CFPOLL",  WLAN_FRAME_CF_POLL, "CF-Poll (no data)" },
	{ 'j', "CFCKPL", WLAN_FRAME_CF_ACKPOLL, "CF-Ack + CF-Poll (no data)" },
	{ 'Q', "QDATA",  WLAN_FRAME_QDATA, "QoS Data" },
	{ 'q', "QDCFCK", WLAN_FRAME_QDATA_CF_ACK, "QoS Data + CF-Ack" },
	{ 'K', "QDCFPL", WLAN_FRAME_QDATA_CF_POLL, "QoS Data + CF-Poll" },
	{ 'y', "QDCFKP", WLAN_FRAME_QDATA_CF_ACKPOLL, "QoS Data + CF-Ack + CF-Poll" },
	{ 'N', "QDNULL", WLAN_FRAME_QOS_NULL, "QoS Null (no data)" },
	{ '-', "-RESV-", 0x00D0, "RESERVED" },
	{ 'Y', "QCFPLL", WLAN_FRAME_QOS_CF_POLL, "QoS CF-Poll (no data)" },
	{ 'z', "QCFKPL", WLAN_FRAME_QOS_CF_ACKPOLL, "QoS CF-Ack + CF-Poll (no data)" },
} };

static struct pkt_name unknow = { '?', "UNKNOW", 0, "Unknown" };
static struct pkt_name badfcs = { '*', "BADFCS", 0, "Bad FCS" };

struct pkt_name get_packet_struct(uint16_t type)
{
	int t = WLAN_FRAME_TYPE(type);

	if (type == 1) /* special case for bad FCS */
		return badfcs;
	if (t == 3)
		return unknow;
	else
		return stype_names[WLAN_FRAME_TYPE(type)][WLAN_FRAME_STYPE(type)];

	return unknow;
}

char get_packet_type_char(uint16_t type)
{
	return get_packet_struct(type).c;
}

const char* get_packet_type_name(uint16_t type)
{
	return get_packet_struct(type).name;
}

/* rate in 100kbps */
int rate_to_index(int rate)
{
	switch (rate) {
		case 540: return 12;
		case 480: return 11;
		case 360: return 10;
		case 240: return 9;
		case 180: return 8;
		case 120: return 7;
		case 110: return 6;
		case 90: return 5;
		case 60: return 4;
		case 55: return 3;
		case 20: return 2;
		case 10: return 1;
		default: return 0;
	}
}

/* return rate in 100kbps */
int rate_index_to_rate(int idx)
{
	switch (idx) {
		case 12: return 540;
		case 11: return 480;
		case 10: return 360;
		case 9: return 240;
		case 8: return 180;
		case 7: return 120;
		case 6: return 110;
		case 5: return 90;
		case 4: return 60;
		case 3: return 55;
		case 2: return 20;
		case 1: return 10;
		default: return 0;
	}
}

/* return rate in 100kbps */
int mcs_index_to_rate(int mcs, bool ht20, bool lgi)
{
	/* MCS Index, http://en.wikipedia.org/wiki/IEEE_802.11n-2009#Data_rates */
	switch (mcs) {
		case 0:  return ht20 ? (lgi ? 65 : 72) : (lgi ? 135 : 150);
		case 1:  return ht20 ? (lgi ? 130 : 144) : (lgi ? 270 : 300);
		case 2:  return ht20 ? (lgi ? 195 : 217) : (lgi ? 405 : 450);
		case 3:  return ht20 ? (lgi ? 260 : 289) : (lgi ? 540 : 600);
		case 4:  return ht20 ? (lgi ? 390 : 433) : (lgi ? 810 : 900);
		case 5:  return ht20 ? (lgi ? 520 : 578) : (lgi ? 1080 : 1200);
		case 6:  return ht20 ? (lgi ? 585 : 650) : (lgi ? 1215 : 1350);
		case 7:  return ht20 ? (lgi ? 650 : 722) : (lgi ? 1350 : 1500);
		case 8:  return ht20 ? (lgi ? 130 : 144) : (lgi ? 270 : 300);
		case 9:  return ht20 ? (lgi ? 260 : 289) : (lgi ? 540 : 600);
		case 10: return ht20 ? (lgi ? 390 : 433) : (lgi ? 810 : 900);
		case 11: return ht20 ? (lgi ? 520 : 578) : (lgi ? 1080 : 1200);
		case 12: return ht20 ? (lgi ? 780 : 867) : (lgi ? 1620 : 1800);
		case 13: return ht20 ? (lgi ? 1040 : 1156) : (lgi ? 2160 : 2400);
		case 14: return ht20 ? (lgi ? 1170 : 1300) : (lgi ? 2430 : 2700);
		case 15: return ht20 ? (lgi ? 1300 : 1444) : (lgi ? 2700 : 3000);
		case 16: return ht20 ? (lgi ? 195 : 217) : (lgi ? 405 : 450);
		case 17: return ht20 ? (lgi ? 39 : 433) : (lgi ? 810 : 900);
		case 18: return ht20 ? (lgi ? 585 : 650) : (lgi ? 1215 : 1350);
		case 19: return ht20 ? (lgi ? 78 : 867) : (lgi ? 1620 : 1800);
		case 20: return ht20 ? (lgi ? 1170 : 1300) : (lgi ? 2430 : 2700);
		case 21: return ht20 ? (lgi ? 1560 : 1733) : (lgi ? 3240 : 3600);
		case 22: return ht20 ? (lgi ? 1755 : 1950) : (lgi ? 3645 : 4050);
		case 23: return ht20 ? (lgi ? 1950 : 2167) : (lgi ? 4050 : 4500);
		case 24: return ht20 ? (lgi ? 260 : 288) : (lgi ? 540 : 600);
		case 25: return ht20 ? (lgi ? 520 : 576) : (lgi ? 1080 : 1200);
		case 26: return ht20 ? (lgi ? 780 : 868) : (lgi ? 1620 : 1800);
		case 27: return ht20 ? (lgi ? 1040 : 1156) : (lgi ? 2160 : 2400);
		case 28: return ht20 ? (lgi ? 1560 : 1732) : (lgi ? 3240 : 3600);
		case 29: return ht20 ? (lgi ? 2080 : 2312) : (lgi ? 4320 : 4800);
		case 30: return ht20 ? (lgi ? 2340 : 2600) : (lgi ? 4860 : 5400);
		case 31: return ht20 ? (lgi ? 2600 : 2888) : (lgi ? 5400 : 6000);
	}
	return 0;
}

/* return rate in 100kbps
 *
 * Formula from http://equicom.hu/uploads/file/fluke/pros/how_fast_80211ac_poster.PDF
 * may not be 100% exact, but good enough
 */
int vht_mcs_index_to_rate(enum chan_width width, int streams, int mcs, bool sgi)
{
	int wf;
	float mf;

	switch (width) {
		case CHAN_WIDTH_UNSPEC:
		case CHAN_WIDTH_20_NOHT:
			return -1; /* not supported */
		case CHAN_WIDTH_20:
			wf = 52; break;
		case CHAN_WIDTH_40:
			wf = 108; break;
		case CHAN_WIDTH_80:
			wf = 234; break;
		case CHAN_WIDTH_160:
		case CHAN_WIDTH_8080:
			wf = 468; break;
		default:
			return -1; /* not supported */
	}

	switch (mcs) {
		case 0:	mf = 0.5; break;
		case 1:	mf = 1.0; break;
		case 2:	mf = 1.5; break;
		case 3:	mf = 2.0; break;
		case 4:	mf = 3.0; break;
		case 5:	mf = 4.0; break;
		case 6:	mf = 4.5; break;
		case 7:	mf = 5.0; break;
		case 8:	mf = 6.0; break;
		case 9:	mf = 6.67; break;
		default: return -1; /* not supported */
	}

	/* special unsupported cases */
	if (width == CHAN_WIDTH_20 && mcs == 9 && streams != 3)
		return -1;
	if (width == CHAN_WIDTH_80 && mcs == 6 && streams == 3)
		return -1;
	if (width == CHAN_WIDTH_160 && mcs == 9 && streams == 3)
		return -1;
	if (width < CHAN_WIDTH_80 && streams > 4)
		return -1;
	if (width == CHAN_WIDTH_80 && mcs == 9 && streams == 6)
		return -1;
	if (width == CHAN_WIDTH_80 && mcs == 6 && streams == 7)
		return -1;

	return 10.0 /* kpbs */ * streams * wf * mf / (sgi ? 3.6 : 4.0);
}

enum chan_width chan_width_from_vht_capab(uint32_t vht)
{
	switch (((vht & WLAN_IE_VHT_CAPAB_INFO_CHAN_WIDTH) >> 2)) {
		case WLAN_IE_VHT_CAPAB_INFO_CHAN_WIDTH_80: return CHAN_WIDTH_80;
		case WLAN_IE_VHT_CAPAB_INFO_CHAN_WIDTH_160: return CHAN_WIDTH_160;
		case WLAN_IE_VHT_CAPAB_INFO_CHAN_WIDTH_BOTH: return CHAN_WIDTH_8080;
		default: printf("(reserved)\n"); return CHAN_WIDTH_UNSPEC;
	}
}

/* Note: mcs must be at least 13 bytes long! In theory its 16 byte */
void ht_streams_from_mcs_set(unsigned char* mcs, unsigned char* rx, unsigned char* tx)
{
	int i;
	for (i = 0; i < 4; i++) {
		if (!mcs[i])
			break;
	}
	*rx = i;

	bool tx_mcs_defined = mcs[12] & 0x01;
	bool tx_rx_mcs_not_equal = !!(mcs[12] & 0x02);
	char tx_max_streams = ((mcs[12] & 0x0c) >> 2) + 1;

	if (tx_mcs_defined && !tx_rx_mcs_not_equal)
		*tx = *rx;
	else if (tx_mcs_defined && tx_rx_mcs_not_equal)
		*tx = tx_max_streams;
}

/* Note: mcs must be at least 6 bytes long! In theory its 8 byte */
void vht_streams_from_mcs_set(unsigned char* mcs, unsigned char* rx, unsigned char* tx)
{
	int i;
	/* RX */
	uint16_t tmp = mcs[0] | (mcs[1] << 8);
	for (i = 0; i < 8; i++) {
		if (((tmp >> (i*2)) & 3) == 3)
			break;
	}
	*rx = i;

	/* TX */
	tmp = mcs[4] | (mcs[5] << 8);
	for (i = 0; i < 8; i++) {
		if (((tmp >> (i*2)) & 3) == 3)
			break;
	}
	*tx = i;
}

const char* get_80211std(enum chan_width width, int chan)
{
	switch (width) {
		case CHAN_WIDTH_UNSPEC:
		case CHAN_WIDTH_20_NOHT:
			return chan > 14 ? "a" : "bg";
		case CHAN_WIDTH_20:
		case CHAN_WIDTH_40:
			return "n";
		case CHAN_WIDTH_80:
		case CHAN_WIDTH_160:
		case CHAN_WIDTH_8080:
			return "ac";
		default:
			return "?";
	}
}

/* in 100kbps or -1 when unsupported */
int get_phy_thruput(enum chan_width width, unsigned char streams_rx)
{
	switch (width) {
		case CHAN_WIDTH_UNSPEC:
		case CHAN_WIDTH_20_NOHT:
			return 540;
		case CHAN_WIDTH_20:
			return mcs_index_to_rate(streams_rx * 8 - 1, true, false);
		case CHAN_WIDTH_40:
			return mcs_index_to_rate(streams_rx * 8 - 1, false, false);
		case CHAN_WIDTH_80:
		case CHAN_WIDTH_160:
		case CHAN_WIDTH_8080:
			return vht_mcs_index_to_rate(width, streams_rx, 9, true);
	}
	return 0;
}
