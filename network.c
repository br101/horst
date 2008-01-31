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
#include <err.h>

#include "main.h"
#include "util.h"

extern struct config conf;

int srv_fd = 0;
int cli_fd = 0;
static int netmon_fd;

#define CLI_BACKLOG 5

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

	if (listen(srv_fd, CLI_BACKLOG) < 0)
		err(1, "listen");
}


int
net_send_packet(struct packet_info *pkt)
{
	write(cli_fd, &current_packet, sizeof(struct packet_info));
	return 0;
}

int net_handle_server_conn()
{
	struct sockaddr_in cin;
	socklen_t cinlen;

	cli_fd = accept(srv_fd, (struct sockaddr*)&cin, &cinlen);

	if (!cli_fd)
		return -1;

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
	if (srv_fd) {
		shutdown(srv_fd, SHUT_RDWR);
		close(srv_fd);
	}
	if (cli_fd) {
		shutdown(cli_fd, SHUT_RDWR);
		close(cli_fd);
	}
	if (netmon_fd) {
		shutdown(netmon_fd, SHUT_RDWR);
		close(netmon_fd);
	}
}
