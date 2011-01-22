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

#define PROTO_VERSION	1


enum pkt_type {
	PROTO_PKT_INFO		= 0,
	PROTO_CHAN_LIST		= 1,
	PROTO_COMMAND		= 2,
};


struct net_header {
	unsigned char version;
	unsigned char type;
} __attribute__ ((packed));


#if 0
enum net_command {
	NET_CMD_RESERVED	= 0,
};

struct net_cmd {
	struct net_header	proto;

	int command;
	int status;
} __attribute__ ((packed));
#endif


struct net_chan_list {
	struct net_header	proto;

	unsigned char num_channels;
	struct {
		unsigned char chan;
		unsigned char freq;
	} channel[1];
} __attribute__ ((packed));


struct net_packet_info {
	struct net_header	proto;

	/* general */
	int			pkt_types;	/* bitmask of packet types */
	int			pkt_len;	/* packet length */

	/* wlan phy (from radiotap) */
	int			phy_signal;	/* signal strength (usually dBm) */
	int			phy_noise;	/* noise level (usually dBm) */
	int			phy_snr;	/* signal to noise ratio */
	int			phy_rate;	/* physical rate */
	int			phy_freq;	/* frequency (unused) */
	unsigned char		phy_chan;	/* channel from driver */
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
				wlan_retry:1,
				_reserved:30;

	/* IP */
	unsigned int		ip_src;
	unsigned int		ip_dst;
	int			olsr_type;
	int			olsr_neigh;
	int			olsr_tc;
} __attribute__ ((packed));


static int
net_write(int fd, unsigned char* buf, size_t len)
{
	int ret;
	ret = write(fd, buf, len);
	if (ret == -1) {
		if (errno == EPIPE) {
			printlog("Client has closed");
			close(fd);
			if (fd == cli_fd)
				cli_fd = -1;
			net_init_server_socket(conf.port);
		}
		else
			printlog("ERROR: write in net_send_packet");
		return 0;
	}
	return 1;
}


int
net_send_packet(struct packet_info *p)
{
	struct net_packet_info np;

	np.proto.version = PROTO_VERSION;
	np.proto.type	= PROTO_PKT_INFO;

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
	np._reserved	= 0; /* don't send out uninitialized memory */
	np.ip_src	= p->ip_src;
	np.ip_dst	= p->ip_dst;
	np.olsr_type	= htole32(p->olsr_type);
	np.olsr_neigh	= htole32(p->olsr_neigh);
	np.olsr_tc	= htole32(p->olsr_tc);

	net_write(cli_fd, (unsigned char *)&np, sizeof(np));
	return 0;
}


static int
net_receive_packet(unsigned char *buffer, int len)
{
	struct net_packet_info *np;
	struct packet_info pkt;
	struct packet_info* p = &pkt;

	if (len < sizeof(struct net_packet_info))
		return 0;

	np = (struct net_packet_info *)buffer;

	if (np->phy_rate == 0)
		return 0;

	memset(&pkt, 0, sizeof(pkt));
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

	handle_packet(p);
	return 1;
}


#if 0
static int
net_send_cmd(int fd, enum net_command cmd)
{
	struct net_cmd nc;

	nc.proto.version = PROTO_VERSION;
	nc.proto.type	= PROTO_COMMAND;
	nc.command = cmd;

	net_write(fd, (unsigned char *)&nc, sizeof(nc));
	return 0;
}


static void
net_receive_command(unsigned char *buffer, int len)
{
	struct net_cmd *nc;

	if (len < sizeof(struct net_cmd))
		return;

	nc = (struct net_cmd *)buffer;
}
#endif


static int
net_send_chan_list(int fd)
{
	char* buf;
	struct net_chan_list *nc;
	int i;

	buf = malloc(sizeof(struct net_chan_list) + 2*(conf.num_channels - 1));
	if (buf == NULL)
		return 0;

	nc = (struct net_chan_list *)buf;
	nc->proto.version = PROTO_VERSION;
	nc->proto.type	= PROTO_CHAN_LIST;

	for (i = 0; i < conf.num_channels && i < MAX_CHANNELS; i++) {
		nc->channel[i].chan = channels[i].chan;
		nc->channel[i].freq = channels[i].freq;
	}
	nc->num_channels = i;

	net_write(fd, (unsigned char *)buf, sizeof(struct net_chan_list) + 2*(i - 1));
	free(buf);
	return 0;
}


static void
net_receive_chan_list(unsigned char *buffer, int len)
{
	struct net_chan_list *nc;
	int i;

	if (len < sizeof(struct net_chan_list))
		return;

	nc = (struct net_chan_list *)buffer;

	for (i = 0; i < nc->num_channels && i < MAX_CHANNELS; i++) {
		channels[i].chan = nc->channel[i].chan;
		channels[i].freq = nc->channel[i].freq;
	}
	conf.num_channels = i;
	init_channels();
}


int
net_receive(int fd, unsigned char* buffer, size_t bufsize)
{
	struct net_header *nh;
	int len;

	len = recv(fd, buffer, bufsize, MSG_DONTWAIT);

	if (len < sizeof(struct net_header))
		return 0;

	nh = (struct net_header *)buffer;

	if (nh->version != PROTO_VERSION) {
		printlog("ERROR: protocol version %x", nh->version);
		return 0;
	}

	if (nh->type == PROTO_PKT_INFO)
		net_receive_packet(buffer, len);
	else if (nh->type == PROTO_CHAN_LIST)
		net_receive_chan_list(buffer, len);
#if 0
	else if (nh->type == PROTO_COMMAND)
		net_receive_command(buffer, len);
#endif
	return 1;
}


int net_handle_server_conn( void )
{
	struct sockaddr_in cin;
	socklen_t cinlen;

	cinlen = sizeof(cin);
	memset(&cin, 0, sizeof(struct sockaddr_in));
	cli_fd = accept(srv_fd, (struct sockaddr*)&cin, &cinlen);

	printlog("Accepting client");
	net_send_chan_list(cli_fd);
	close(srv_fd);
	srv_fd = -1;

	//read(cli_fd,line,sizeof(line));
	return 0;
}


void
net_init_server_socket(char* rport)
{
	struct sockaddr_in sock_in;
	int reuse = 1;

	printlog("Initializing server port %s", rport);

	memset(&sock_in, 0, sizeof(struct sockaddr_in));
	sock_in.sin_family = AF_INET;
	sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_in.sin_port = htons(atoi(rport));

	if ((srv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err(1, "Could not open server socket");

	if (setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
		err(1, "setsockopt SO_REUSEADDR");

	if (bind(srv_fd, (struct sockaddr*)&sock_in, sizeof(sock_in)) < 0)
		err(1, "bind");

	if (listen(srv_fd, 0) < 0)
		err(1, "listen");
}


int
net_open_client_socket(char* serveraddr, char* rport)
{
	struct addrinfo saddr;
	struct addrinfo *result, *rp;
	int ret;

	printlog("Connecting to server %s port %s", serveraddr, rport);

	/* Obtain address(es) matching host/port */
	memset(&saddr, 0, sizeof(struct addrinfo));
	saddr.ai_family = AF_INET;
	saddr.ai_socktype = SOCK_STREAM;
	saddr.ai_flags = 0;
	saddr.ai_protocol = 0;

	ret = getaddrinfo(serveraddr, rport, &saddr, &result);
	if (ret != 0) {
		fprintf(stderr, "Could not resolve: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	 * Try each address until we successfully connect. */
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		netmon_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (netmon_fd == -1)
			continue;

		if (connect(netmon_fd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Success */

		close(netmon_fd);
	}

	if (rp == NULL) {
		/* No address succeeded */
		freeaddrinfo(result);
		err(1, "Could not connect");
	}

	freeaddrinfo(result);

	printlog("Connected to server %s", serveraddr);
	return netmon_fd;
}


void
net_finish(void) {
	if (srv_fd != -1)
		close(srv_fd);

	if (cli_fd != -1)
		close(cli_fd);

	if (netmon_fd)
		close(netmon_fd);
}
