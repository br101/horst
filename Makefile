# horst - Highly Optimized Radio Scanning Tool
#
# Copyright (C) 2005-2014 Bruno Randolf (br1@einfach.org)
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
DEBUG=0
PCAP=0
UPLOAD=0

NAME=horst
OBJS=main.o capture$(if $(filter 1,$(PCAP)),-pcap).o protocol_parser.o \
	network.o wext.o node.o essid.o channel.o \
	util.o ieee80211_util.o listsort.o average.o \
	display.o display-main.o display-filter.o display-help.o \
	display-statistics.o display-essid.o display-history.o \
	display-spectrum.o display-channel.o control.o \
	radiotap/radiotap.o $(if $(filter 1,$(UPLOAD)),upload.o)
LIBS=-lncurses -lm
CFLAGS+=-Wall -Wextra -g

ifeq ($(DEBUG),1)
CFLAGS+=-DDO_DEBUG
endif

ifeq ($(PCAP),1)
CFLAGS+=-DPCAP
LIBS+=-lpcap
endif

ifeq ($(UPLOAD),1)
CFLAGS+=-DUPLOAD
LIBS+=-lcurl -lpthread
endif

.PHONY: force

all: $(NAME)

# include dependencies
average.o: average.h util.h
capture.o: capture.h util.h
capture-pcap.o: capture.h util.h
channel.o: main.h util.h ieee80211_util.h wext.h
display.o: display.h main.h ieee80211.h
display-channel.o: display.h main.h
display-essid.o: display.h main.h util.h
display-filter.o: display.h main.h util.h ieee80211.h
display-help.o: display.h main.h util.h
display-history.o: display.h main.h util.h
display-main.o: display.h main.h util.h ieee80211.h olsr_header.h listsort.h
display-spectrum.o: display.h main.h util.h
display-statistics.o: display.h main.h util.h
essid.o: main.h util.h ieee80211.h
ieee80211_util.o: ieee80211.h ieee80211_util.h main.h util.h
listsort.o: list.h listsort.h
main.o: protocol_parser.h display.h network.h main.h capture.h util.h \
	ieee80211.h ieee80211_util.h wext.h average.h
node.o: main.h ieee80211.h util.h
network.o: main.h util.h network.h
protocol_parser.o: prism_header.h ieee80211.h \
	radiotap/radiotap.h radiotap/radiotap_iter.h \
	ieee80211_util.h olsr_header.h batman_header.h protocol_parser.h \
	main.h util.h
radiotap/radiotap.o: radiotap/radiotap.h radiotap/radiotap_iter.h \
	radiotap/platform.h
util.o: util.h ieee80211.h
wext.o: wext.h util.h
upload.o: main.h util.h

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): .buildflags

clean:
	-rm -f *.o radiotap/*.o *~
	-rm -f $(NAME)
	-rm -f .buildflags

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
