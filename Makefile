# olsr scanning tool
#
# Copyright (C) 2005  Bruno Randolf
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
VERSION=1.2.2
BUILDDATE=$(shell date +"%Y-%m-%e")
OBJS=protocol_parser.o main.o display.o
LIBS=-l ncurses
CFLAGS=-Wall -DPACKAGE_VERSION=\"$(VERSION)\" -DPACKAGE_BUILDDATE=\"$(BUILDDATE)\"

all: $(NAME)

protocol_parser.o: ieee80211_header.h prism_header.h olsr_header.h
main.o: main.h
display.o: display.h

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm -f *.o
	-rm -f $(NAME)
