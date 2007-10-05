/* olsr scanning tool
 *
 * Copyright (C) 2005  Bruno Randolf
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

#ifndef _OLSR_HEADER_H_
#define _OLSR_HEADER_H_

#include <sys/types.h>
#include <netinet/in.h>

typedef u_int8_t olsr_u8_t;
typedef u_int16_t olsr_u16_t;
typedef u_int32_t olsr_u32_t;

/* from olsr olsr_protocol.h */

/***********************************************
 *           OLSR packet definitions           *
 ***********************************************/


/*
 *Hello info
 */
struct hellinfo 
{
  olsr_u8_t   link_code;
  olsr_u8_t   reserved;
  olsr_u16_t  size;
  olsr_u32_t  neigh_addr[1]; /* neighbor IP address(es) */
} __attribute__ ((packed));

struct hellomsg 
{
  olsr_u16_t      reserved;
  olsr_u8_t       htime;
  olsr_u8_t       willingness;
  struct hellinfo hell_info[1];
} __attribute__ ((packed));

/*
 *IPv6
 */

struct hellinfo6
{
  olsr_u8_t       link_code;
  olsr_u8_t       reserved;
  olsr_u16_t      size;
  struct in6_addr neigh_addr[1]; /* neighbor IP address(es) */
} __attribute__ ((packed));

struct hellomsg6
{
  olsr_u16_t         reserved;
  olsr_u8_t          htime;
  olsr_u8_t          willingness;
  struct hellinfo6   hell_info[1];
} __attribute__ ((packed));

/*
 * Topology Control packet
 */

struct neigh_info
{
  olsr_u32_t       addr;
} __attribute__ ((packed));


struct tcmsg 
{
  olsr_u16_t        ansn;
  olsr_u16_t        reserved;
  struct neigh_info neigh[1];
} __attribute__ ((packed));



/*
 *IPv6
 */

struct neigh_info6
{
  struct in6_addr      addr;
} __attribute__ ((packed));


struct tcmsg6
{
  olsr_u16_t           ansn;
  olsr_u16_t           reserved;
  struct neigh_info6   neigh[1];
} __attribute__ ((packed));





/*
 *Multiple Interface Declaration message
 */

/* 
 * Defined as s struct for further expansion 
 * For example: do we want to tell what type of interface
 * is associated whit each address?
 */
struct midaddr
{
  olsr_u32_t addr;
} __attribute__ ((packed));


struct midmsg 
{
  struct midaddr mid_addr[1];
} __attribute__ ((packed));


/*
 *IPv6
 */
struct midaddr6
{
  struct in6_addr addr;
} __attribute__ ((packed));


struct midmsg6
{
  struct midaddr6 mid_addr[1];
} __attribute__ ((packed));






/*
 * Host and Network Association message
 */
struct hnapair
{
  olsr_u32_t   addr;
  olsr_u32_t   netmask;
} __attribute__ ((packed));

struct hnamsg
{
  struct hnapair hna_net[1];
} __attribute__ ((packed));

/*
 *IPv6
 */

struct hnapair6
{
  struct in6_addr   addr;
  struct in6_addr   netmask;
} __attribute__ ((packed));

struct hnamsg6
{
  struct hnapair6 hna_net[1];
} __attribute__ ((packed));





/*
 * OLSR message (several can exist in one OLSR packet)
 */

struct olsrmsg
{
  olsr_u8_t     olsr_msgtype;
  olsr_u8_t     olsr_vtime;
  olsr_u16_t    olsr_msgsize;
  olsr_u32_t    originator;
  olsr_u8_t     ttl;
  olsr_u8_t     hopcnt;
  olsr_u16_t    seqno;

  union 
  {
    struct hellomsg hello;
    struct tcmsg    tc;
    struct hnamsg   hna;
    struct midmsg   mid;
  } message;

} __attribute__ ((packed));

/*
 *IPv6
 */

struct olsrmsg6
{
  olsr_u8_t        olsr_msgtype;
  olsr_u8_t        olsr_vtime;
  olsr_u16_t       olsr_msgsize;
  struct in6_addr  originator;
  olsr_u8_t        ttl;
  olsr_u8_t        hopcnt;
  olsr_u16_t       seqno;

  union 
  {
    struct hellomsg6 hello;
    struct tcmsg6    tc;
    struct hnamsg6   hna;
    struct midmsg6   mid;
  } message;

} __attribute__ ((packed));



/*
 * Generic OLSR packet
 */

struct olsr 
{
  olsr_u16_t	  olsr_packlen;		/* packet length */
  olsr_u16_t	  olsr_seqno;
  struct olsrmsg  olsr_msg[1];          /* variable messages */
} __attribute__ ((packed));


struct olsr6
{
  olsr_u16_t	    olsr_packlen;        /* packet length */
  olsr_u16_t	    olsr_seqno;
  struct olsrmsg6   olsr_msg[1];         /* variable messages */
} __attribute__ ((packed));


/* IPv4 <-> IPv6 compability */

union olsr_message
{
  struct olsrmsg  v4;
  struct olsrmsg6 v6;
} __attribute__ ((packed));

union olsr_packet
{
  struct olsr  v4;
  struct olsr6 v6;
} __attribute__ ((packed));

/*
 *Message Types
 */

#define HELLO_MESSAGE         1
#define TC_MESSAGE            2
#define MID_MESSAGE           3
#define HNA_MESSAGE           4

#define LQ_HELLO_MESSAGE      201
#define LQ_TC_MESSAGE         202
/*
 *Link Types
 */

#define UNSPEC_LINK           0
#define ASYM_LINK             1
#define SYM_LINK              2
#define LOST_LINK             3
#define HIDE_LINK             4
#define MAX_LINK              4

/*
 *Neighbor Types
 */

#define NOT_NEIGH             0
#define SYM_NEIGH             1
#define MPR_NEIGH             2
#define MAX_NEIGH             2

/*
 *Neighbor status
 */

#define NOT_SYM               0
#define SYM                   1



// serialized IPv4 OLSR header

struct olsr_header_v4
{
  olsr_u8_t  type;
  olsr_u8_t  vtime;
  olsr_u16_t size;
  olsr_u32_t orig;
  olsr_u8_t  ttl;
  olsr_u8_t  hops;
  olsr_u16_t seqno;
};

// serialized IPv6 OLSR header

struct olsr_header_v6
{
  olsr_u8_t     type;
  olsr_u8_t     vtime;
  olsr_u16_t    size;
  unsigned char orig[16];
  olsr_u8_t     ttl;
  olsr_u8_t     hops;
  olsr_u16_t    seqno;
};

// serialized LQ_HELLO

struct lq_hello_info_header
{
  olsr_u8_t  link_code;
  olsr_u8_t  reserved;
  olsr_u16_t size;
};

struct lq_hello_header
{
  olsr_u16_t reserved;
  olsr_u8_t  htime;
  olsr_u8_t  will;
};

// serialized LQ_TC

struct lq_tc_header
{
  olsr_u16_t ansn;
  olsr_u16_t reserved;
};

#endif
