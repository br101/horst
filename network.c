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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <err.h>

#include "main.h"
#include "util.h"
#include "network.h"

extern struct config conf;

int srv_fd = -1;
int cli_fd = -1;
static int netmon_fd;

struct net_packet_info {
	/* general */
	int			pkt_types;	/* bitmask of packet types in this pkt */
	int			pkt_len;	/* packet length */

	/* wlan phy (from radiotap) */
	int			phy_signal;	/* signal strength (usually dBm) */
	int			phy_noise;	/* noise level (usually dBm) */
	int			phy_snr;	/* signal to noise ratio */
	int			phy_rate;	/* physical rate */
	int			phy_freq;	/* frequency (unused) */
	unsigned short		phy_chan;	/* channel from driver */
	int			phy_flags;	/* A, B, G, shortpre */

	/* wlan mac */
	int			wlan_type;	/* frame control field */
	unsigned char		wlan_src[MAC_LEN];
	unsigned char		wlan_dst[MAC_LEN];
	unsigned char		wlan_bssid[MAC_LEN];
	char			wlan_essid[MAX_ESSID_LEN];
	u_int64_t		wlan_tsf;	/* timestamp from beacon */
	unsigned int		wlan_bintval;	/* beacon interval */
	int			wlan_mode;	/* AP, STA or IBSS */
	unsigned char		wlan_channel;	/* channel from beacon, probe */
	unsigned char		wlan_qos_class;	/* for QDATA frames */
	unsigned int		wlan_nav;	/* frame NAV duration */
	unsigned int		wlan_seqno;	/* sequence number */

	/* flags */
	unsigned int		wlan_wep:1,	/* WEP on/off */
				wlan_retry:1;

	/* IP */
	unsigned int		ip_src;
	unsigned int		ip_dst;
	int			olsr_type;
	int			olsr_neigh;
	int			olsr_tc;
} __attribute__ ((packed));


void
net_init_server_socket(char* rport)
{
	struct sockaddr_in sock_in;
	int reuse = 1;

	printf("using server port %s\n", rport);

	sock_in.sin_family = AF_INET;
	sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_in.sin_port = htons(atoi(rport));

	if ((srv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		err(1, "socket");
	}

	if (setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		err(1, "setsockopt SO_REUSEADDR");
	}

	if (bind(srv_fd, (struct sockaddr*)&sock_in, sizeof(sock_in)) < 0) {
		err(1, "bind");
	}

	if (listen(srv_fd, 0) < 0) {
		err(1, "listen");
	}
}


int
net_send_packet(struct packet_info *p)
{
	int ret;
	struct net_packet_info np;

	np.pkt_types	= htole32(p->pkt_types);
	np.pkt_len	= htole32(p->pkt_len);
	np.phy_signal	= htole32(p->phy_signal);
	np.phy_noise	= htole32(p->phy_noise);
	np.phy_snr	= htole32(p->phy_snr);
	np.phy_rate	= htole32(p->phy_rate);
	np.phy_freq	= htole32(p->phy_freq);
	np.phy_chan	= p->phy_chan;
	np.phy_flags	= htole32(p->phy_flags);
	np.wlan_type	= htole32(p->wlan_type);
	memcpy(np.wlan_src, p->wlan_src, MAC_LEN);
	memcpy(np.wlan_dst, p->wlan_dst, MAC_LEN);
	memcpy(np.wlan_bssid, p->wlan_bssid, MAC_LEN);
	memcpy(np.wlan_essid, p->wlan_essid, MAX_ESSID_LEN);
	np.wlan_tsf	= htole64(p->wlan_tsf);
	np.wlan_bintval	= htole32(p->wlan_bintval);
	np.wlan_mode	= htole32(p->wlan_mode);
	np.wlan_channel = p->wlan_channel;
	np.wlan_qos_class = p->wlan_qos_class;
	np.wlan_nav	= htole32(p->wlan_nav);
	np.wlan_seqno	= htole32(p->wlan_seqno);
	np.wlan_wep	= p->wlan_wep;
	np.wlan_retry	= p->wlan_retry;
	np.ip_src	= p->ip_src;
	np.ip_dst	= p->ip_dst;
	np.olsr_type	= htole32(p->olsr_type);
	np.olsr_neigh	= htole32(p->olsr_neigh);
	np.olsr_tc	= htole32(p->olsr_tc);

	ret = write(cli_fd, &np, sizeof(np));
	if (ret == -1) {
		if (errno == EPIPE) {
			printf("client has closed\n");
			close(cli_fd);
			cli_fd = -1;
		}
		else {
			perror("write");
		}
	}
	return 0;
}


/*
 * return 0 - error
 *	  1 - ok
 */
int
net_receive_packet(unsigned char *buffer, int len, struct packet_info *p)
{
	struct net_packet_info *np;

	if (len < sizeof(struct net_packet_info)) {
		return 0;
	}

	np = (struct net_packet_info *)buffer;

	if (np->phy_rate == 0) {
		return 0;
	}

	p->pkt_types	= le32toh(np->pkt_types);
	p->pkt_len	= le32toh(np->pkt_len);
	p->phy_signal	= le32toh(np->phy_signal);
	p->phy_noise	= le32toh(np->phy_noise);
	p->phy_snr	= le32toh(np->phy_snr);
	p->phy_rate	= le32toh(np->phy_rate);
	p->phy_freq	= le32toh(np->phy_freq);
	p->phy_chan	= np->phy_chan;
	p->phy_flags	= le32toh(np->phy_flags);
	p->wlan_type	= le32toh(np->wlan_type);
	memcpy(p->wlan_src, np->wlan_src, MAC_LEN);
	memcpy(p->wlan_dst, np->wlan_dst, MAC_LEN);
	memcpy(p->wlan_bssid, np->wlan_bssid, MAC_LEN);
	memcpy(p->wlan_essid, np->wlan_essid, MAX_ESSID_LEN);
	p->wlan_tsf	= le64toh(np->wlan_tsf);
	p->wlan_bintval	= le32toh(np->wlan_bintval);
	p->wlan_mode	= le32toh(np->wlan_mode);
	p->wlan_channel = np->wlan_channel;
	p->wlan_qos_class = np->wlan_qos_class;
	p->wlan_nav	= le32toh(np->wlan_nav);
	p->wlan_seqno	= le32toh(np->wlan_seqno);
	p->wlan_wep	= np->wlan_wep;
	p->wlan_retry	= np->wlan_retry;
	p->ip_src	= np->ip_src;
	p->ip_dst	= np->ip_dst;
	p->olsr_type	= le32toh(np->olsr_type);
	p->olsr_neigh	= le32toh(np->olsr_neigh);
	p->olsr_tc	= le32toh(np->olsr_tc);

	return 1;
}


int net_handle_server_conn( void )
{
	struct sockaddr_in cin;
	socklen_t cinlen;

	if (cli_fd != -1) {
		printf("can only handle one client\n");
		return -1;
	}

	cli_fd = accept(srv_fd, (struct sockaddr*)&cin, &cinlen);

	printf("horst: accepting client\n");

	//read(cli_fd,line,sizeof(line));
	return 0;
}


int
net_open_client_socket(char* serveraddr, char* rport)
{
	struct addrinfo saddr;
	struct addrinfo *result, *rp;
	int ret;

	printf("connecting to server %s port %s\n", serveraddr, rport);

	/* Obtain address(es) matching host/port */
	memset(&saddr, 0, sizeof(struct addrinfo));
	saddr.ai_family = AF_INET;
	saddr.ai_socktype = SOCK_STREAM;
	saddr.ai_flags = 0;
	saddr.ai_protocol = 0;

	ret = getaddrinfo(serveraddr, rport, &saddr, &result);
	if (ret != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect. */
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		netmon_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (netmon_fd == -1) {
			continue;
		}

		if (connect(netmon_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break; /* Success */
		}

		close(netmon_fd);
	}

	if (rp == NULL) {
		/* No address succeeded */
		freeaddrinfo(result);
		err(1, "could not connect");
	}

	freeaddrinfo(result);

	printf("connected\n");
	return netmon_fd;
}


void
net_finish(void) {
	if (srv_fd != -1) {
		close(srv_fd);
	}
	if (cli_fd != -1) {
		close(cli_fd);
	}
	if (netmon_fd) {
		close(netmon_fd);
	}
}
