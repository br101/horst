# horst - Highly Optimized Radio Scanning Tool
#
# Copyright (C) 2005-2010 Bruno Randolf (br1@einfach.org)
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

NAME=horst
DEBUG=0
PCAP=0
OBJS=main.o capture.o protocol_parser.o network.o wext.o \
	util.o ieee80211_util.o listsort.o \
	display.o display-main.o display-filter.o display-help.o \
	display-statistics.o display-essid.o display-history.o \
	display-spectrum.o

LIBS=-lncurses -lm
CFLAGS+=-Wall -DDO_DEBUG=$(DEBUG) -g

ifeq ($(PCAP),1)
CFLAGS+=-DPCAP
LIBS+=-lpcap
endif

buildstamp=.build_debug$(DEBUG)pcap$(PCAP)

all: $(buildstamp) $(NAME)

# include dependencies
protocol_parser.o: protocol_parser.h ieee80211.h ieee80211_radiotap.h ieee80211_util.h \
		   prism_header.h olsr_header.h batman_header.h util.h main.h
main.o: main.h ieee80211.h protocol_parser.h display.h network.h util.h capture.h
display.o: display.h main.h util.h ieee80211.h olsr_header.h
network.o: network.h main.h util.h
util.o: util.h ieee80211.h
capture.o: capture.h util.h

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm -f *.o *~
	-rm -f $(NAME)
	-rm -f .build_*

$(buildstamp):
	make clean
	touch $@
