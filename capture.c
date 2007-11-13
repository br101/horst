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
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <err.h>

#include "capture.h"
#include "util.h"


#ifdef PCAP

#include <pcap.h>

#define PCAP_TIMEOUT 200

static unsigned char* pcap_buffer;
static size_t pcap_bufsize;
static pcap_t *pcap_fp = NULL;


void handler(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
	*((int *)user) = h->len;
	if (pcap_bufsize < h->len)
	{
		fprintf(stderr, "Buffer(%d) too small for %d bytes\n", pcap_bufsize, h->len);
		*((int *)user) = pcap_bufsize;
	}
	memmove(pcap_buffer, bytes, *((int *)user));
}


int
open_packet_socket(char* devname, size_t bufsize, int* device_arp_type)
{
	char error[PCAP_ERRBUF_SIZE];
	if (NULL == (pcap_fp = pcap_open_live(devname, bufsize, 1, PCAP_TIMEOUT, error)))
	{
		return -1;
	}
	if (NULL != device_arp_type) {
		switch (pcap_datalink(pcap_fp)) {
			case DLT_IEEE802_11_RADIO:
				*device_arp_type = 803;
				break;
			case DLT_PRISM_HEADER:
				*device_arp_type = 802;
				break;
			default:
				*device_arp_type = 801;
				break;
		} // switch
	}
	return 0;
}


int
recv_packet(unsigned char* buffer, size_t bufsize)
{
	int ret = 0; 
	pcap_buffer = buffer;
	pcap_bufsize = bufsize;
	if (0 == pcap_dispatch(pcap_fp, 1, handler, (u_char *)&ret))
	{
		return -1;
	}
	return ret;
}


void
close_packet_socket(char* devname)
{
	pcap_close(pcap_fp);
}


#else /* use PACKET SOCKET */


static int mon_fd = 0;


static int
device_index(int fd, const char *devname)
{
	struct ifreq req;

	strncpy(req.ifr_name, devname, IFNAMSIZ);
	req.ifr_addr.sa_family = AF_INET;

	if (ioctl(fd, SIOCGIFINDEX, &req) < 0)
		err(1, "interface %s not found", devname);

	if (req.ifr_ifindex<0) {
		err(1, "interface %s not found", devname);
	}
	DEBUG("index %d\n", req.ifr_ifindex);
	return req.ifr_ifindex;
}


static void
device_promisc(int fd, const char *devname, int on)
{
	struct ifreq req;

	strncpy(req.ifr_name, devname, IFNAMSIZ);
	req.ifr_addr.sa_family = AF_INET;

	if (ioctl(fd, SIOCGIFFLAGS, &req) < 0) {
		err(1, "could not get device flags for %s", devname);
	}

	/* put interface up in any case */
	req.ifr_flags |= IFF_UP;

	if (on)
		req.ifr_flags |= IFF_PROMISC;
	else
		req.ifr_flags &= ~IFF_PROMISC;

	if (ioctl(fd, SIOCSIFFLAGS, &req) < 0) {
		err(1, "could not set promisc mode for %s", devname);
	}
}


/*
 *  Get the hardware type of the given interface as ARPHRD_xxx constant.
 */
static int
device_get_arptype(int fd, const char *device)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		err(1, "could not get arptype");
	}
	DEBUG("ARPTYPE %d\n", ifr.ifr_hwaddr.sa_family);
	return ifr.ifr_hwaddr.sa_family;
}


int
open_packet_socket(char* devname, size_t bufsize, int* device_arp_type)
{
	int ret;
	int ifindex;

	mon_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (mon_fd < 0)
		err(1, "could not create socket");

	/* bind only to one interface */
	ifindex = device_index(mon_fd, devname);

	struct sockaddr_ll sall;
	sall.sll_ifindex = ifindex;
	sall.sll_family = AF_PACKET;
	sall.sll_protocol = htons(ETH_P_ALL);

	ret = bind(mon_fd, (struct sockaddr*)&sall, sizeof(sall));
	if (ret != 0)
		err(1, "bind failed");

	device_promisc(mon_fd, devname, 1);
	if (NULL != device_arp_type) {
		*device_arp_type = device_get_arptype(mon_fd, devname);
	}

	return (0 <= mon_fd);
}


inline int
recv_packet(unsigned char* buffer, size_t bufsize)
{
	return recv(mon_fd, buffer, bufsize, MSG_DONTWAIT);
}


void
close_packet_socket(char* devname)
{
	device_promisc(mon_fd, devname, 0);
	close(mon_fd);
}


#if 0
static void
device_wireless_channel(int fd, const char* devname, int chan)
{
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, devname, IFNAMSIZ);
	iwr.u.freq.m = chan * 100000;
	iwr.u.freq.e = 1;

	if (ioctl(fd, SIOCSIWFREQ, &iwr) < 0) {
		perror("ioctl[SIOCSIWFREQ]");
		ret = -1;
	}
}
#endif


#endif // PCAP
