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

struct sockaddr_in sock_in, cin;
socklen_t cinlen;
int srv_fd=0;
int cli_fd=0;
int i;
fd_set rs;
char line[256];
int llen;
struct timeval to={0,0};
struct timeval tr={0,100};
int on = 1;


void
net_init_socket(int rport)
{
	if (!conf.quiet)
		printf("using remote port %d\n",rport);

	sock_in.sin_family=AF_INET;
	sock_in.sin_port=htons(rport);
	sock_in.sin_addr.s_addr=htonl(INADDR_ANY);

	if ((srv_fd=socket(AF_INET,SOCK_STREAM,0)) < 0) {
		err(1, "socket");
	}
	if (setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		err(1, "setsockopt SO_REUSEADDR");
	}
	if (bind(srv_fd, (struct sockaddr*)&sock_in, sizeof(sock_in)) < 0) {
		err(1, "bind");
	}
	if (listen(srv_fd, 5) < 0) {
		err(1, "listen");
	}
}


int
net_send_packet(void)
{
	struct node_info* n;
	FD_ZERO(&rs);
	FD_SET(srv_fd,&rs);
	if (select(srv_fd+1,&rs,NULL,NULL,&to) && FD_ISSET(srv_fd,&rs))
	{
		cli_fd = accept(srv_fd,(struct sockaddr*)&cin,&cinlen);
		if (!conf.quiet)
			printf("horst: accepting client\n");
		if (!cli_fd)
			return -1;

		// discard stuff which was sent to us e.g. by a http client
		FD_ZERO(&rs);
		FD_SET(cli_fd,&rs);
		while (select(cli_fd+1,&rs,NULL,NULL,&tr) && FD_ISSET(cli_fd,&rs)) {
			read(cli_fd,line,sizeof(line));
			FD_ZERO(&rs);
			FD_SET(cli_fd,&rs);
		}

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
		close(cli_fd);
	}
	return 0;
}


void
net_finish(void) {
	close(srv_fd);
}
