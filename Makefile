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

# dependencies, generated with 'gcc -MM *.c' and pasted here
average.o: average.c average.h util.h
capture.o: capture.c capture.h util.h
capture-pcap.o: capture-pcap.c capture.h util.h
channel.o: channel.c main.h list.h average.h util.h ieee80211_util.h \
 ieee80211.h wext.h channel.h
control.o: control.c main.h list.h average.h control.h
display.o: display.c display.h main.h list.h average.h ieee80211.h
display-channel.o: display-channel.c display.h main.h list.h average.h \
 network.h
display-essid.o: display-essid.c display.h main.h list.h average.h util.h
display-filter.o: display-filter.c display.h main.h list.h average.h \
 util.h ieee80211.h network.h
display-help.o: display-help.c display.h main.h list.h average.h util.h
display-history.o: display-history.c display.h main.h list.h average.h \
 util.h
display-main.o: display-main.c display.h main.h list.h average.h util.h \
 ieee80211.h olsr_header.h listsort.h
display-spectrum.o: display-spectrum.c display.h main.h list.h average.h \
 util.h
display-statistics.o: display-statistics.c display.h main.h list.h \
 average.h util.h ieee80211_util.h ieee80211.h
essid.o: essid.c main.h list.h average.h util.h ieee80211.h essid.h
ieee80211_util.o: ieee80211_util.c ieee80211.h ieee80211_util.h main.h \
 list.h average.h util.h
listsort.o: listsort.c list.h listsort.h
main.o: main.c main.h list.h average.h util.h capture.h protocol_parser.h \
 network.h display.h ieee80211.h ieee80211_util.h wext.h control.h \
 channel.h node.h essid.h upload.h
network.o: network.c main.h list.h average.h util.h network.h
node.o: node.c main.h list.h average.h util.h ieee80211.h essid.h
protocol_parser.o: protocol_parser.c prism_header.h ieee80211.h \
 ieee80211_util.h olsr_header.h batman_header.h protocol_parser.h main.h \
 list.h average.h util.h radiotap/radiotap.h radiotap/radiotap_iter.h \
 radiotap/radiotap.h
upload.o: upload.c main.h list.h average.h util.h
util.o: util.c util.h ieee80211.h
wext.o: wext.c wext.h main.h list.h average.h util.h

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): .buildflags

clean:
	-rm -f *.o radiotap/*.o *~
	-rm -f $(NAME)
	-rm -f .buildflags

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
