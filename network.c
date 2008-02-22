/* horst - olsr scanning tool
 *
 * Copyright (C) 2005-2007  Bruno Randolf
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
	int			len;		/* packet length */

	/* wlan phy (from radiotap) */
	int			signal;		/* signal strength (usually dBm) */
	int			noise;		/* noise level (usually dBm) */
	int			snr;		/* signal to noise ratio */
	int			rate;		/* physical rate */
	int			phy_freq;	/* frequency (unused) */
	int			phy_flags;	/* A, B, G, shortpre */

	/* wlan mac */
	int			wlan_type;	/* frame control field */
	unsigned char		wlan_src[MAC_LEN];
	unsigned char		wlan_dst[MAC_LEN];
	unsigned char		wlan_bssid[MAC_LEN];
	char			wlan_essid[MAX_ESSID_LEN];
	u_int64_t		wlan_tsf;	/* timestamp from beacon */
	int			wlan_mode;	/* AP, STA or IBSS */
	unsigned char		wlan_channel;	/* channel from beacon, probe */
	int			wlan_wep;	/* WEP on/off */

	/* IP */
	unsigned int		ip_src;
	unsigned int		ip_dst;
	int			olsr_type;
	int			olsr_neigh;
	int			olsr_tc;
} __attribute__ ((packed));


void
net_init_server_socket(int rport)
{
	struct sockaddr_in sock_in;
	int reuse = 1;

	printf("using server port %d\n", rport);

	sock_in.sin_family = AF_INET;
	sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_in.sin_port = htons(rport);

	if ((srv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err(1, "socket");

	if (setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
		err(1, "setsockopt SO_REUSEADDR");

	if (bind(srv_fd, (struct sockaddr*)&sock_in, sizeof(sock_in)) < 0)
		err(1, "bind");

	if (listen(srv_fd, 0) < 0)
		err(1, "listen");
}


int
net_send_packet(struct packet_info *pkt)
{
	int ret;
	struct net_packet_info np;

	np.pkt_types	= pkt->pkt_types;
	np.len		= pkt->len;
	np.signal	= pkt->signal;
	np.noise	= pkt->noise;
	np.snr		= pkt->snr;
	np.rate		= pkt->rate;
	np.phy_freq	= pkt->phy_freq;
	np.phy_flags	= pkt->phy_flags;
	np.wlan_type	= pkt->wlan_type;
	np.wlan_tsf	= pkt->wlan_tsf;
	np.wlan_mode	= pkt->wlan_mode;
	np.wlan_channel = pkt->wlan_channel;
	np.wlan_wep	= pkt->wlan_wep;
	np.ip_src	= pkt->ip_src;
	np.ip_dst	= pkt->ip_dst;
	np.olsr_type	= pkt->olsr_type;
	np.olsr_neigh	= pkt->olsr_neigh;
	np.olsr_tc	= pkt->olsr_tc;
	memcpy(np.wlan_src, pkt->wlan_src, MAC_LEN);
	memcpy(np.wlan_dst, pkt->wlan_dst, MAC_LEN);
	memcpy(np.wlan_bssid, pkt->wlan_bssid, MAC_LEN);
	memcpy(np.wlan_essid, pkt->wlan_essid, MAX_ESSID_LEN);

	ret = write(cli_fd, &np, sizeof(np));
	if (ret == -1) {
		if (errno == EPIPE) {
			printf("client has closed\n");
			close(cli_fd);
			cli_fd = -1;
		}
		else
			perror("write");
	}
	return 0;
}


/*
 * return 0 - error
 *	  1 - ok
 */
int
net_receive_packet(unsigned char *buffer, int len, struct packet_info *pkt)
{
	struct net_packet_info *np;

	if (len < sizeof(struct net_packet_info)) {
		return 0;
	}

	np = (struct net_packet_info *)buffer;

	if (np->rate == 0) {
		return 0;
	}

	pkt->pkt_types	= np->pkt_types;
	pkt->len	= np->len;
	pkt->signal	= np->signal;
	pkt->noise	= np->noise;
	pkt->snr	= np->snr;
	pkt->rate	= np->rate;
	pkt->phy_freq	= np->phy_freq;
	pkt->phy_flags	= np->phy_flags;
	pkt->wlan_type	= np->wlan_type;
	pkt->wlan_tsf	= np->wlan_tsf;
	pkt->wlan_mode	= np->wlan_mode;
	pkt->wlan_channel = np->wlan_channel;
	pkt->wlan_wep	= np->wlan_wep;
	pkt->ip_src	= np->ip_src;
	pkt->ip_dst	= np->ip_dst;
	pkt->olsr_type	= np->olsr_type;
	pkt->olsr_neigh	= np->olsr_neigh;
	pkt->olsr_tc	= np->olsr_tc;
	memcpy(pkt->wlan_src, np->wlan_src, MAC_LEN);
	memcpy(pkt->wlan_dst, np->wlan_dst, MAC_LEN);
	memcpy(pkt->wlan_bssid, np->wlan_bssid, MAC_LEN);
	memcpy(pkt->wlan_essid, np->wlan_essid, MAX_ESSID_LEN);

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

#if 0
		// satisfy http clients (wget)
		static const char hdr[]="HTTP/1.0 200 ok\r\nContent-Type: text/plain\r\n\r\n";
 		write (cli_fd,hdr,sizeof(hdr));
		list_for_each_entry(n, &nodes, list) {
			char src_eth[18];
			strcpy(src_eth,ether_sprintf(n->last_pkt.wlan_src));
			/* ip sig noise snr source bssid lq gw neigh olsrcount count tsf */
			llen=snprintf(line,sizeof(line)-1,"%s %d %d %d %s %s %d %d %d %d %d %016llx\r\n",
				ip_sprintf(n->ip_src),
				n->last_pkt.signal, n->last_pkt.noise, n->last_pkt.snr,
				src_eth,
				ether_sprintf(n->wlan_bssid),
				n->pkt_types & PKT_TYPE_OLSR_LQ,
				n->pkt_types & PKT_TYPE_OLSR_GW,
				n->olsr_neigh,
				n->olsr_count, n->pkt_count,
				n->tsf);
			write(cli_fd,line,llen);
		}
#endif


int
net_open_client_socket(unsigned int serverip, unsigned int rport)
{
	struct sockaddr_in sock_in;

	printf("connecting to server %x port %d\n", serverip, rport);

	sock_in.sin_family = AF_INET;
	sock_in.sin_addr.s_addr = serverip;
	sock_in.sin_port = htons(rport);

	if ((netmon_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err(1, "socket");

	if (connect(netmon_fd, (struct sockaddr*)&sock_in, sizeof(sock_in)) < 0)
		err(1, "connect");

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
