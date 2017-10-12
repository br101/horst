# HORST - Highly Optimized Radio Scanning Tool
or "Horsts OLSR Radio Scanning Tool"

Copyright (C) 2005-2016 Bruno Randolf (br1@einfach.org) and licensed under the 
GNU Public License (GPL) V2


## Links

* Main page: https://github.com/br101/horst
* Issue tracker: https://github.com/br101/horst/issues
* Download Stable (Version 5.1): https://github.com/br101/horst/archive/v5.1.tar.gz
* Download Development (MASTER): https://github.com/br101/horst/tarball/master


## Overview

`horst` is a small, lightweight IEEE802.11 WLAN analyzer with a text interface. 
Its basic function is similar to tcpdump, Wireshark or Kismet, but it's much 
smaller and shows different, aggregated information which is not easily 
available from other tools. It is made for debugging wireless LANs with a focus 
on getting a quick overview instead of deep packet inspection and has special 
features for Ad-hoc (IBSS) mode and mesh networks. It can be useful to get a 
quick overview of what's going on all wireless LAN channels and to identify 
problems.

* Shows signal (RSSI) values per station, something hard to get, especially in 
  IBSS mode
* Calculates channel utilization (“usage”) by adding up the amount of time the 
  packets actually occupy the medium
* “Spectrum Analyzer” shows signal levels and usage per channel
* Graphical packet history, with signal, packet type and physical rate
* Shows all stations per ESSID and the live TSF per node as it is counting
* Detects IBSS “splits” (same ESSID but different BSSID – this is/was a common 
  driver problem on IBSS mode)
* Statistics of packets/bytes per physical rate and per packet type
* Has some support for mesh protocols (OLSR and batman)
* Can filter specific packet types, operating modes, source addresses or BSSIDs
* Client/server support for monitoring on remote nodes
* Automatically adds and removes monitor interface

`horst` is a Linux program and can be used on any wireless LAN interface which 
supports monitor mode.


## Checkout

If you just want to use `horst`, the recommended way is to download the latest
stable version from https://github.com/br101/horst/releases or to use the
stable branch:

	git clone -b stable https://github.com/br101/horst

Note: The `master` branch is in heavy restructuring mode right now, as it is
switching to use `libuwifi` (https://github.com/br101/libuwifi).

The master branch of `horst` builds on `libuwifi` as a git submodule and
`libuwifi` in turn includes `radiotap` as a submodule. With newer versions of
git the easiest way to check out is:

	git clone --recursive https://github.com/br101/horst

For older versions of git, or if you have already cloned horst before, you can
use:

	git submodule update --init --recursive


## Dependencies

`horst` is just a simple tool, and `libncurses` and header files is the only
hard requirement as well as the `pkg-config` tool. Recently we have added support
for `nl80211` via `libnl`, so on Linux normally you need `libnl3` + header files 
as well. On Debian/Ubuntu based distros you can install them with:

	sudo apt-get install libncurses5-dev libnl-3-dev libnl-genl-3-dev pkg-config


## Building

Building is normally done with "make" (optional `V=1` or `DEBUG=1`). This checks out
`libuwifi` as a submodule if necessary:

	make

If you want to maintain `libuwifi` not as a submodule but in a directory outside
of `horst` you can specify it with:

	make LIBUWIFI=../my/path/to/libuwifi

Should you expect on `libuwifi` in the system path (`/usr/local/include/` and
`/usr/local/lib/` or similar) you can do:

	make LIBUWIFI=

To install (with optional `DESTDIR=/path`):

	sudo make install


## Config and other files

By default `horst` reads a config file `/etc/horst.conf`. The location of the file
can be changed with the `-c file` command line option. See the file itself or
`man horst.conf` for a description of the options.

You can use `-Mfilename` to define a MAC address to host name mapping file which
can either be a `dhcp.leases` file or simply contain `MAC-Address<whitesspace>Name`
one each line.

`-o outfile` can write the packets to a comma separated list file.

`-X[filename]` is not a real file, but allows a control socket named pipe which can
later be used with `-x command` to send commands in the same format as the options
in the config file.


## Usage notes

Starting with version 5.0 `horst` can automatically set the WLAN interface into
monitor mode or add a monitor interface. But you can still set the interface into
monitor mode manually before you start `horst` as well. With most standard 
Linux (mac80211) drivers you can use the `iw` command to add an additional 
monitor interface while you can continue to use the existing interface.

	iw wlan0 interface add mon0 type monitor

Please note that while the main interface (`wlan0`) is in use, either as a client
to an AP, in Ad-hoc mode, or creating an AP, the wifi driver does not allow 
`horst` to change the channel because that would disrupt connectivity. If you 
want `horst` to be able to change channels (`horst -s` or `channel_scan` 
option, or setting a channel manually in the `horst` UI) you need to set the main
interface to monitor mode. This is how it is usually done:

	ifconfig wlan0 down
	iw wlan0 set type monitor

Optionally you could also set an initial channel, and it sometimes may be necessary
to unblock the interface first:

	rfkill unblock all
	ifconfig wlan0 up
	iw wlan0 set channel 6

If you still have to use the deprecated WEXT interface can put the interface into
monitor mode with `iwconfig wlan0 mode monitor channel X`).

Usually you have to start `horst` as root:

	sudo horst -i wlan0

To do remote monitoring over the network you can start a server (-q without a 
user interface), usually on your AP or device with

	horst -i wlan0 -N -q

and connect a client (only one client is allowed at a time), usually from your 
PC with

	horst -n IP

Please read the man page for more details about the options, output and 
abbreviations. It should be be part of your distribution package, but you can 
read it in the source code locally with:

	man -l horst.8
	man -l horst.conf.5

Please contact me if you have any problems or questions. New feature ideas, 
patches and feedback are always welcome. Please create GitHub issues at 
https://github.com/br101/horst/issues for problem reports and support. 


## Background and history

`horst` was created in 2005 to fill a need in the Wireless Mesh networking and 
Freifunk community of Berlin but has since grown to be a useful tool for
debugging any kind of wireless network.

A notorious Berlin Freifunk community member known as "Offline Horst" had enough
persistence to convince me that such a tool is necessary and thus started the
development and gave the name to the `horst` tool.

With the usual wireless tools like iw, iwconfig and iwspy and even kismet or 
WireShark it is hard to measure the received signal strength (RSSI) of
all available access points, stations and ad-hoc networks in a given location. 
It's especially difficult to differentiate the different nodes which form an 
ad-hoc network. This information however is very important for setting up, 
debugging and optimizing wireless mesh networks and antenna positions.

`horst` aims to fill this gap and lists each single node of an ad-hoc network
separately, showing the signal strength (RSSI) of the last received packet. This
way you can see which nodes are part of a specific ad-hoc cell (BSSID), 
discover problems with ad-hoc cell merging ("cell splitting", a problem of 
many WLAN drivers) and get a general overview of what's going on in the "air".

To do this, `horst` uses the monitor mode including radiotap headers (or before 
prism2 headers) for the signal strength information of the wlan cards and 
listens to all packets which come in the wireless interface. The packets are 
summarized by the MAC address of the sending node, analyzed and aggregated and 
displayed in a simple text (ncurses) interface.


## Contributors

Thanks to the following persons for contributions:

* Horst Krause
* Sven-Ola Tuecke
* Robert Schuster
* Jonathan Guerin
* David Rowe
* Antoine Beaupré
* Rami Refaeli
* Joerg Albert
* Tuomas Räsänen
* Jiantao Fu
