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

NAME=horst

# build options
DEBUG=0

OBJS=	conf_options.o				\
	control.o				\
	display-channel.o			\
	display-essid.o				\
	display-filter.o			\
	display-help.o				\
	display-history.o			\
	display-main.o				\
	display-spectrum.o			\
	display-statistics.o			\
	display.o				\
	essid.o					\
	hutil.o					\
	ieee80211_duration.o			\
	listsort.o				\
	main.o					\
	network.o				\
	protocol_parser.o			\

LIBS=-lncurses -lm -luwifi
INCLUDES=-I. -I./libuwifi/inst/include
CFLAGS+=-std=gnu99 -Wall -Wextra -g $(INCLUDES)

ifeq ($(DEBUG),1)
	CFLAGS+=-DDEBUG=1
endif

.PHONY: all check clean force

all: $(NAME)

.objdeps.mk: $(OBJS:%.o=%.c)
	gcc -MM $(INCLUDES) $^ >$@

-include .objdeps.mk

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): .buildflags libuwifi/libuwifi.so.1

libuwifi/libuwifi.so.1:
	make -C libuwifi INST_PATH=inst install

check:
	sparse $(CFLAGS) *.[ch]

clean:
	-rm -f *.o *~
	-rm -f $(NAME)
	-rm -f .buildflags
	-rm -f .objdeps.mk
	-rm -r libuwifi/inst
	-make -C libuwifi clean

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
