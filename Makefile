# horst - Highly Optimized Radio Scanning Tool
#
# Copyright (C) 2005-2016 Bruno Randolf (br1@einfach.org)
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

NAME = horst

# build options
DEBUG = 0
LIBUWIFI_SUBMODULE = 1

OBJS += conf_options.o
OBJS += control.o
OBJS += display-channel.o
OBJS += display-essid.o
OBJS += display-filter.o
OBJS += display-help.o
OBJS += display-history.o
OBJS += display-main.o
OBJS += display-spectrum.o
OBJS += display-statistics.o
OBJS += display.o
OBJS += hutil.o
OBJS += ieee80211_duration.o
OBJS += listsort.o
OBJS += main.o
OBJS += network.o
OBJS += protocol_parser.o

LIBS = -lncurses -lm -luwifi
INCLUDES = -I.
CFLAGS += -std=gnu99 -Wall -Wextra -g $(INCLUDES) -DVERSION=\"$(shell git describe --tags)\"

ifeq ($(LIBUWIFI_SUBMODULE),1)
	INCLUDES += -I./libuwifi/inst/include
	LDFLAGS += -L./libuwifi
	UWIFI_DEPEND = libuwifi/libuwifi.so.1
endif

ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG=1
endif

DESTDIR ?= /usr/local

.PHONY: all check clean force

all: $(NAME)

.objdeps.mk: $(OBJS:%.o=%.c)
	gcc -MM $(INCLUDES) $^ >$@

-include .objdeps.mk

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): .buildflags $(UWIFI_DEPEND)

libuwifi/libuwifi.so.1:
	make -C libuwifi INST_PATH=inst install

check:
	sparse $(CFLAGS)  -D__linux__ *.[ch]

clean:
	-rm -f *.o *~
	-rm -f $(NAME)
	-rm -f .buildflags
	-rm -f .objdeps.mk
	-rm -r libuwifi/inst
	-make -C libuwifi clean

install:
	mkdir -p $(DESTDIR)/sbin/
	mkdir -p $(DESTDIR)/etc
	mkdir -p $(DESTDIR)/man/man8/
	mkdir -p $(DESTDIR)/man/man5
	cp horst $(DESTDIR)/sbin/
	cp horst.conf $(DESTDIR)/etc/
	gzip horst.8 -c > $(DESTDIR)/man/man8/horst.8.gz
	gzip horst.conf.5 -c > $(DESTDIR)/man/man5/horst.conf.5.gz

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
