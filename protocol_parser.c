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
#include <string.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <netinet/ip.h>
#define __FAVOR_BSD
#include <netinet/udp.h>

#include <uwifi/raw_parser.h>
#include <uwifi/util.h>
#include <uwifi/log.h>

#include "olsr_header.h"
#include "batman_header.h"
#include "batman_adv_header-14.h"
#include "main.h"
#include "hutil.h"

static int parse_llc(unsigned char* buf, size_t len, struct uwifi_packet* p);
static int parse_ip_header(unsigned char* buf, size_t len, struct uwifi_packet* p);
static int parse_udp_header(unsigned char* buf, size_t len, struct uwifi_packet* p);
static int parse_olsr_packet(unsigned char* buf, size_t len, struct uwifi_packet* p);
static int parse_batman_packet(unsigned char* buf, size_t len, struct uwifi_packet* p);
static int parse_batman_adv_packet(unsigned char* buf, size_t len, struct uwifi_packet* p);
static int parse_meshcruzer_packet(unsigned char* buf, size_t len, struct uwifi_packet* p, int port);

/* return true if we parsed enough = min ieee header */
bool parse_packet(unsigned char* buf, size_t len, struct uwifi_packet* p)
{
	int ret = uwifi_parse_raw(buf, len, p, conf.intf.arphdr);
	if (ret == 0)
		return true;
	else if (ret < 0)
		return false;

	len -= ret; buf += ret;
	ret = parse_llc(buf, len, p);
	if (ret <= 0)
		return true;

	len -= ret; buf += ret;
	ret = parse_ip_header(buf, len, p);
	if (ret <= 0)
		return true;

	len -= ret; buf += ret;
	parse_udp_header(buf, len, p);
	return true;
}

static int parse_llc(unsigned char* buf, size_t len, struct uwifi_packet* p)
{
	LOG_DBG("* parse LLC");

	if (len < 6)
		return -1;

	/* check type in LLC header */
	buf = buf + 6;

	if (ntohs(*((uint16_t*)buf)) == 0x4305) {
		LOG_DBG("BATMAN-ADV");
		buf++; buf++;
		return parse_batman_adv_packet(buf, len - 8, p);
	}
	else {
		if (*buf != 0x08)
			return -1;
		buf++;
		if (*buf == 0x06) { /* ARP */
			p->pkt_types |= PKT_TYPE_ARP;
			return 0;
		}
		if (*buf != 0x00)  /* not IP */
			return -1;
		buf++;

		LOG_DBG("* parse LLC left %zd", len - 8);
		return 8;
	}
}

static int parse_batman_adv_packet(unsigned char* buf,
				   __attribute__((unused)) size_t len,
				   struct uwifi_packet* p)
{
	struct batman_ogm_packet *bp;
	//batadv_ogm_packet
	bp = (struct batman_ogm_packet*)buf;

	p->pkt_types |= PKT_TYPE_BATMAN;
	p->bat_version = bp->version;
	p->bat_packet_type = bp->packet_type;

	LOG_DBG("parse bat len %zd type %d vers %d", len, bp->packet_type, bp->version);

	/* version 14 */
	if (bp->version == 14) {
		switch (bp->packet_type) {
		case BAT_OGM:
			/* set GW flags only for "original" (not re-sent) OGMs */
			if (bp->gw_flags != 0 && memcmp(bp->orig, p->wlan_src, WLAN_MAC_LEN) == 0)
				p->bat_gw = 1;
			LOG_DBG("OGM %d %d", bp->gw_flags, p->bat_gw);
			return 0;
		case BAT_ICMP:
			LOG_DBG("ICMP");
			break;
		case BAT_UNICAST:
			LOG_DBG("UNI %zu", sizeof(struct unicast_packet));
			return sizeof(struct unicast_packet) + 14;
		case BAT_BCAST:
			LOG_DBG("BCAST");
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

static int parse_ip_header(unsigned char* buf, size_t len, struct uwifi_packet* p)
{
	struct ip* ih;

	LOG_DBG("* parse IP");

	if (len < sizeof(struct ip))
		return -1;

	ih = (struct ip*)buf;

	LOG_DBG("*** IP SRC: %s", ip_sprintf(ih->ip_src.s_addr));
	LOG_DBG("*** IP DST: %s", ip_sprintf(ih->ip_dst.s_addr));
	p->ip_src = ih->ip_src.s_addr;
	p->ip_dst = ih->ip_dst.s_addr;
	p->pkt_types |= PKT_TYPE_IP;

	LOG_DBG("IP proto: %d", ih->ip_p);
	switch (ih->ip_p) {
		case IPPROTO_UDP: p->pkt_types |= PKT_TYPE_UDP; break;
		/* all others set the type and return. no more parsing */
		case IPPROTO_ICMP: p->pkt_types |= PKT_TYPE_ICMP; return 0;
		case IPPROTO_TCP: p->pkt_types |= PKT_TYPE_TCP; return 0;
	}

	return ih->ip_hl * 4;
}

static int parse_udp_header(unsigned char* buf, size_t len, struct uwifi_packet* p)
{
	struct udphdr* uh;

	if (len < sizeof(struct udphdr))
		return -1;

	uh = (struct udphdr*)buf;

	LOG_DBG("UPD dest port: %d", ntohs(uh->uh_dport));
	p->tcpudp_port = ntohs(uh->uh_dport);

	buf = buf + 8;
	len = len - 8;

	if (p->tcpudp_port == 698) /* OLSR */
		return parse_olsr_packet(buf, len, p);

	if (p->tcpudp_port == BAT_PORT) /* batman */
		return parse_batman_packet(buf, len, p);

	if (p->tcpudp_port == 9256 || p->tcpudp_port == 9257 ) /* MeshCruzer */
		return parse_meshcruzer_packet(buf, len, p, p->tcpudp_port);

	return 0;
}

static int parse_olsr_packet(unsigned char* buf, size_t len, struct uwifi_packet* p)
{
	struct olsr* oh;
	int number, msgtype;

	if (len < sizeof(struct olsr))
		return -1;

	oh = (struct olsr*)buf;

	// TODO: more than one olsr messages can be in one packet
	msgtype = oh->olsr_msg[0].olsr_msgtype;

	LOG_DBG("OLSR msgtype: %d*** ", msgtype);

	p->pkt_types |= PKT_TYPE_OLSR;
	p->olsr_type = msgtype;

	//if (msgtype == LQ_HELLO_MESSAGE || msgtype == LQ_TC_MESSAGE )
	//	p->pkt_types |= PKT_TYPE_OLSR_LQ;

	if (msgtype == HELLO_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize) - 12) / sizeof(struct hellomsg);
		LOG_DBG("HELLO %d", number);
		p->olsr_neigh = number;
	}

	if (msgtype == LQ_HELLO_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize) - 16) / 12;
		LOG_DBG("LQ_HELLO %d (%d)", number, (ntohs(oh->olsr_msg[0].olsr_msgsize) - 16));
		p->olsr_neigh = number;
	}
#if 0
/*	XXX: tc messages are relayed. so we would have to find the originating node (IP)
	and store the information there. skip for now */

	if (msgtype == TC_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-12) / sizeof(struct tcmsg);
		LOG_DBG("TC %d", number);
		p->olsr_tc = number;
	}

	if (msgtype == LQ_TC_MESSAGE) {
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize)-16) / 8;
		LOG_DBG("LQ_TC %d (%d)", number, (ntohs(oh->olsr_msg[0].olsr_msgsize)-16));
		p->olsr_tc = number;
	}

	if (msgtype == HNA_MESSAGE) {
		/* same here, but we assume that nodes which relay a HNA with a default gateway
		know how to contact the gw, so have a indirect connection to a GW themselves */
		struct hnapair* hna;
		number = (ntohs(oh->olsr_msg[0].olsr_msgsize) - 12) / sizeof(struct hnapair);
		LOG_DBG("HNA NUM: %d (%d) [%d]", number, ntohs(oh->olsr_msg[0].olsr_msgsize),
			(int)sizeof(struct hnapair) );
		for (i = 0; i < number; i++) {
			hna = &(oh->olsr_msg[0].message.hna.hna_net[i]);
			LOG_DBG("HNA %s", ip_sprintf(hna->addr));
			LOG_DBG("/%s", ip_sprintf(hna->netmask));
			if (hna->addr == 0 && hna->netmask == 0)
				p->pkt_types |= PKT_TYPE_OLSR_GW;
		}
	}
#endif
	/* done for good */
	return 0;
}

static int parse_batman_packet(__attribute__((unused)) unsigned char* buf,
			       __attribute__((unused)) size_t len,
			       __attribute__((unused)) struct uwifi_packet* p)
{
	p->pkt_types |= PKT_TYPE_BATMAN;
	return 0;
}

static int parse_meshcruzer_packet(__attribute__((unused)) unsigned char* buf,
				   __attribute__((unused)) size_t len,
				   __attribute__((unused)) struct uwifi_packet* p,
				   __attribute__((unused)) int port)
{
	p->pkt_types |= PKT_TYPE_MESHZ;
	return 0;
}
