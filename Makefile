# horst - Highly Optimized Radio Scanning Tool
#
# Copyright (C) 2005-2017 Bruno Randolf (br1@einfach.org)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

NAME		= horst

# build options
DEBUG		= 0
LIBUWIFI_SUBMOD	= 0
DESTDIR		?= /usr/local

SRC		+= conf_options.c
SRC		+= control.c
SRC		+= display-channel.c
SRC		+= display-essid.c
SRC		+= display-filter.c
SRC		+= display-help.c
SRC		+= display-history.c
SRC		+= display-main.c
SRC		+= display-spectrum.c
SRC		+= display-statistics.c
SRC		+= display.c
SRC		+= hutil.c
SRC		+= ieee80211_duration.c
SRC		+= listsort.c
SRC		+= main.c
SRC		+= network.c
SRC		+= protocol_parser.c

LIBS		= -lncurses -lm -luwifi

INCLUDES	= -I.
CFLAGS		+= -std=gnu99 -Wall -Wextra -g
CHECK_FLAGS	+= -D__linux__

ifeq ($(LIBUWIFI_SUBMOD),1)
	INCLUDES += -I./libuwifi/inst/include
	LDFLAGS	+= -L./libuwifi
	UWIFI_DEPEND = libuwifi/libuwifi.so.1
endif

ifeq ($(DEBUG),1)
	DEFS	+= -DDO_DEBUG=1
endif

all: bin
check:
clean:

libuwifi/libuwifi.so.1:
	make -C libuwifi INST_PATH=inst install

install:
	mkdir -p $(DESTDIR)/sbin/
	mkdir -p $(DESTDIR)/etc
	mkdir -p $(DESTDIR)/man/man8/
	mkdir -p $(DESTDIR)/man/man5
	cp horst $(DESTDIR)/sbin/
	cp horst.conf $(DESTDIR)/etc/
	gzip horst.8 -c > $(DESTDIR)/man/man8/horst.8.gz
	gzip horst.conf.5 -c > $(DESTDIR)/man/man5/horst.conf.5.gz

include Makefile.default
