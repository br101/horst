/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2015 Bruno Randolf (br1@einfach.org)
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

#ifndef _WLAN_HEADER_H_
#define _WLAN_HEADER_H_

#include <sys/types.h>

struct wlan_frame {
	u_int16_t	fc;
	u_int16_t	duration;
	u_int8_t	addr1[6];
	u_int8_t	addr2[6];
	u_int8_t	addr3[6];
	u_int16_t	seq;
	union {
		u_int16_t	qos;
		u_int8_t	addr4[6];
		struct {
			u_int16_t	qos;
			u_int32_t	ht;
		} __attribute__ ((packed)) ht;
		struct {
			u_int8_t	addr4[6];
			u_int16_t	qos;
			u_int32_t	ht;
		} __attribute__ ((packed)) addr4_qos_ht;
	} u;
} __attribute__ ((packed));

#define WLAN_FRAME_FC_VERSION_MASK	0x0003
#define WLAN_FRAME_FC_TYPE_MASK		0x000C
#define WLAN_FRAME_FC_STYPE_MASK	0x00F0
#define WLAN_FRAME_FC_STYPE_QOS		0x0080
#define WLAN_FRAME_FC_TO_DS		0x0100
#define WLAN_FRAME_FC_FROM_DS		0x0200
#define WLAN_FRAME_FC_MORE_FRAG		0x0400
#define WLAN_FRAME_FC_RETRY		0x0800
#define WLAN_FRAME_FC_POWER_MGMT	0x1000
#define WLAN_FRAME_FC_MORE_DATA		0x2000
#define WLAN_FRAME_FC_PROTECTED		0x4000
#define WLAN_FRAME_FC_ORDER		0x8000

#define WLAN_FRAME_FC_MASK		(WLAN_FRAME_FC_TYPE_MASK | WLAN_FRAME_FC_STYPE_MASK)

#define WLAN_FRAME_TYPE(_fc)		((_fc & WLAN_FRAME_FC_TYPE_MASK) >> 2)
#define WLAN_FRAME_STYPE(_fc)		((_fc & WLAN_FRAME_FC_STYPE_MASK) >> 4)
#define WLAN_FRAME_FC(_type, _stype)	((((_type) << 2) | ((_stype) << 4)) & WLAN_FRAME_FC_MASK)
#define WLAN_FRAME_TYPE_MGMT		0x0
#define WLAN_FRAME_TYPE_CTRL		0x1
#define WLAN_FRAME_TYPE_DATA		0x2

#define WLAN_NUM_TYPES			3
#define WLAN_NUM_STYPES			16

#define WLAN_FRAME_IS_MGMT(_fc)		(WLAN_FRAME_TYPE(_fc) == WLAN_FRAME_TYPE_MGMT)
#define WLAN_FRAME_IS_CTRL(_fc)		(WLAN_FRAME_TYPE(_fc) == WLAN_FRAME_TYPE_CTRL)
#define WLAN_FRAME_IS_DATA(_fc)		(WLAN_FRAME_TYPE(_fc) == WLAN_FRAME_TYPE_DATA)
#define WLAN_FRAME_IS_QOS(_fc)		(((_fc) & WLAN_FRAME_FC_STYPE_MASK) == WLAN_FRAME_FC_STYPE_QOS)

/*** management ***/
#define WLAN_FRAME_ASSOC_REQ		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x0)
#define WLAN_FRAME_ASSOC_RESP		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x1)
#define WLAN_FRAME_REASSOC_REQ		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x2)
#define WLAN_FRAME_REASSOC_RESP		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x3)
#define WLAN_FRAME_PROBE_REQ		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x4)
#define WLAN_FRAME_PROBE_RESP		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x5)
#define WLAN_FRAME_TIMING		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x6)
/* (reserved)								    0x7 */
#define WLAN_FRAME_BEACON		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x8)
#define WLAN_FRAME_ATIM			WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0x9)
#define WLAN_FRAME_DISASSOC		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0xa)
#define WLAN_FRAME_AUTH			WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0xb)
#define WLAN_FRAME_DEAUTH		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0xc)
#define WLAN_FRAME_ACTION		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0xd)
#define WLAN_FRAME_ACTION_NOACK		WLAN_FRAME_FC(WLAN_FRAME_TYPE_MGMT, 0xe)
/* (reserved)								    0xf */

/*** control ***/
/* (reserved)								    0-3 */
#define WLAN_FRAME_BEAM_REP		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0x4)
#define WLAN_FRAME_VHT_NDP		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0x5)
/* (reserved)								    0x6 */
#define WLAN_FRAME_CTRL_WRAP		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0x7)
#define WLAN_FRAME_BLKACK_REQ		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0x8)
#define WLAN_FRAME_BLKACK		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0x9)
#define WLAN_FRAME_PSPOLL		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0xa)
#define WLAN_FRAME_RTS			WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0xb)
#define WLAN_FRAME_CTS			WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0xc)
#define WLAN_FRAME_ACK			WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0xd)
#define WLAN_FRAME_CF_END		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0xe)
#define WLAN_FRAME_CF_END_ACK		WLAN_FRAME_FC(WLAN_FRAME_TYPE_CTRL, 0xf)

/*** data ***/
#define WLAN_FRAME_DATA			WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x0)
#define WLAN_FRAME_DATA_CF_ACK		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x1)
#define WLAN_FRAME_DATA_CF_POLL		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x2)
#define WLAN_FRAME_DATA_CF_ACKPOLL	WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x3)
#define WLAN_FRAME_NULL			WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x4)
#define WLAN_FRAME_CF_ACK		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x5)
#define WLAN_FRAME_CF_POLL		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x6)
#define WLAN_FRAME_CF_ACKPOLL		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x7)
#define WLAN_FRAME_QDATA		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x8)
#define WLAN_FRAME_QDATA_CF_ACK		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0x9)
#define WLAN_FRAME_QDATA_CF_POLL	WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0xa)
#define WLAN_FRAME_QDATA_CF_ACKPOLL	WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0xb)
#define WLAN_FRAME_QOS_NULL		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0xc)
/* (reserved)								    0xd */
#define WLAN_FRAME_QOS_CF_POLL		WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0xe)
#define WLAN_FRAME_QOS_CF_ACKPOLL	WLAN_FRAME_FC(WLAN_FRAME_TYPE_DATA, 0xf)

#define WLAN_FRAME_QOS_TID_MASK		0x7
#define WLAN_FRAME_QOS_AMSDU_PRESENT	0x80

#define WLAN_FRAME_HT_VHT		0x1
#define WLAN_FRAME_VHT_BW_MASK		0x30000
#define WLAN_FRAME_VHT_BW_20MHZ		0x0
#define WLAN_FRAME_VHT_BW_40MHZ		0x10000
#define WLAN_FRAME_VHT_BW_80MHZ		0x20000
#define WLAN_FRAME_VHT_BW_160MHZ	0x30000

/*** individual frame formats ***/

/* beacon + probe response */
struct wlan_frame_beacon {
	u_int64_t	tsf;
	u_int16_t	bintval;
	u_int16_t	capab;
	unsigned char	ie[0];
} __attribute__ ((packed));


/*** capabilities ***/
#define WLAN_CAPAB_ESS		0x0001
#define WLAN_CAPAB_IBSS		0x0002
#define WLAN_CAPAB_CF_POLL	0x0004
#define WLAN_CAPAB_CF_POLL_REQ	0x0008
#define WLAN_CAPAB_PRIVACY	0x0010
#define WLAN_CAPAB_SHORT_PRE	0x0020
#define WLAN_CAPAB_PBCC		0x0040
#define WLAN_CAPAB_CHAN_AGILIY	0x0080
#define WLAN_CAPAB_SPECT_MGMT	0x0100
#define WLAN_CAPAB_QOS		0x0200
#define WLAN_CAPAB_SHORT_SLOT	0x0400
#define WLAN_CAPAB_APSD		0x0800
#define WLAN_CAPAB_RADIO_MEAS	0x1000
#define WLAN_CAPAB_OFDM		0x2000
#define WLAN_CAPAB_DEL_BLKACK	0x4000
#define WLAN_CAPAB_IMM_BLKACK	0x8000

/*** information elements ***/
struct information_element {
	u_int8_t	id;
	u_int8_t	len;
	unsigned char	var[0];
};

/* only the information element IDs we are interested in */
#define WLAN_IE_ID_SSID		0
#define WLAN_IE_ID_DSSS_PARAM	3
#define WLAN_IE_ID_RSN		48
#define WLAN_IE_ID_VHT_CAPAB	191
#define WLAN_IE_ID_VHT_OPER	192
#define WLAN_IE_ID_VHT_OMN	199
#define WLAN_IE_ID_VENDOR	221

#define WLAN_MAX_SSID_LEN	34

#endif
