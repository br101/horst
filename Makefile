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
LIBUWIFI	= libuwifi
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
LDFLAGS		+= -Wl,-rpath,/usr/local/lib

INCLUDES	= -I.
CFLAGS		+= -std=gnu99 -Wall -Wextra -g
CHECK_FLAGS	+= -D__linux__

ifneq ($(LIBUWIFI),)
	INCLUDES += -I./build/include/
	LDFLAGS	+= -L./build/lib/
	LIBUWIFI_DEPEND = build/lib/libuwifi.so.1
	LDFLAGS += -Wl,-rpath,\$$ORIGIN/lib
endif

all: $(LIBUWIFI_DEPEND) bin
check:
clean:

include Makefile.default

$(LIBUWIFI)/Makefile:
	git submodule update --init --recursive

build/lib/libuwifi.so.1: $(LIBUWIFI)/Makefile $(BUILD_DIR)/buildflags
	make -C $(LIBUWIFI) DEBUG=$(DEBUG) BUILD_DIR=$(CURDIR)/build/libuwifi INST_PATH=$(CURDIR)/build install

install:
	mkdir -p $(DESTDIR)/sbin/
	mkdir -p $(DESTDIR)/etc
	mkdir -p $(DESTDIR)/man/man8/
	mkdir -p $(DESTDIR)/man/man5
	cp build/horst $(DESTDIR)/sbin/
	cp horst.conf $(DESTDIR)/etc/
	gzip horst.8 -c > $(DESTDIR)/man/man8/horst.8.gz
	gzip horst.conf.5 -c > $(DESTDIR)/man/man5/horst.conf.5.gz
  ifneq ($(LIBUWIFI),)
	mkdir -p $(DESTDIR)/lib/
	cp -a build/lib/libuwifi.so* $(DESTDIR)/lib/
  endif
