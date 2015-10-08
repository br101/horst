# horst - Highly Optimized Radio Scanning Tool
#
# Copyright (C) 2005-2015 Bruno Randolf (br1@einfach.org)
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

# build options
DEBUG=1
PCAP=0
LIBNL_VERSION=3.0

NAME=horst
OBJS=						   \
	average.o				   \
	capture$(if $(filter 1,$(PCAP)),-pcap).o   \
	channel.o				   \
	conf_options.o				   \
	control.o				   \
	display-channel.o			   \
	display-essid.o				   \
	display-filter.o			   \
	display-help.o				   \
	display-history.o			   \
	display-main.o				   \
	display-spectrum.o			   \
	display-statistics.o			   \
	display.o				   \
	essid.o					   \
	ieee80211_util.o			   \
	ifctrl-nl80211.o			   \
	listsort.o				   \
	main.o					   \
	network.o				   \
	node.o					   \
	protocol_parser.o			   \
	protocol_parser_wlan.o			   \
	radiotap/radiotap.o			   \
	util.o					   \
	wext.o					   \
	wlan_util.o
LIBS=-lncurses -lm
CFLAGS+=-Wall -Wextra -g -I.

ifeq ($(DEBUG),1)
CFLAGS+=-DDO_DEBUG
endif

ifeq ($(PCAP),1)
CFLAGS+=-DPCAP
LIBS+=-lpcap
endif

ifeq ($(LIBNL_VERSION),tiny)
LIBS+=-lnl-tiny
else
LIBS+=-lnl-3 -lnl-genl-3
endif

.PHONY: all check clean force

all: $(NAME)

.objdeps.mk: $(OBJS:%.o=%.c)
	gcc -MM $^ >$@

-include .objdeps.mk

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): .buildflags

# libnl-tiny header files somehow pollute, so just include them for this file:
ifctrl-nl80211.o:
	$(CC) -c $(CFLAGS) $(shell pkg-config --cflags libnl-$(LIBNL_VERSION)) -o $@ $<

check:
	sparse *.[ch]

clean:
	-rm -f *.o radiotap/*.o *~
	-rm -f $(NAME)
	-rm -f .buildflags

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
