/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2015 Bruno Randolf <br1@einfach.org>
 * Copyright (C) 2015 Jeromy Fu <fuji246@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ifctrl.h"
#include "main.h"
#include "ieee80211_util.h"

#import <CoreWLAN/CoreWLAN.h>

bool osx_set_freq(const char *interface, unsigned int freq)
{
    int channel = ieee80211_freq2channel(freq);

    CWWiFiClient * wifiClient = [CWWiFiClient sharedWiFiClient];
    NSString * interfaceName = [[NSString alloc] initWithUTF8String: interface];
    CWInterface * currentInterface = [wifiClient interfaceWithName: interfaceName];
    [interfaceName release];

    NSSet * channels = [currentInterface supportedWLANChannels];
    CWChannel * wlanChannel = nil;
    for (CWChannel * _wlanChannel in channels) {
        if ([_wlanChannel channelNumber] == channel)
            wlanChannel = _wlanChannel;
    }

    bool ret = true;
    if (wlanChannel != nil) {
        NSError *err = nil;
        BOOL result = [currentInterface setWLANChannel:wlanChannel error:&err];
        if( !result ) {
            printlog("set channel %ld err: %s", (long)[wlanChannel channelNumber], [[err localizedDescription] UTF8String]);
            ret = false;
        }
    }

    return ret;
}

enum chan_width get_channel_width(CWChannelWidth width)
{
    enum chan_width current_width = CHAN_WIDTH_UNSPEC;
    switch (width) {
        case kCWChannelWidth20MHz:
            current_width = CHAN_WIDTH_20;
            break;

        case kCWChannelWidth40MHz:
            current_width = CHAN_WIDTH_40;
            break;

        case kCWChannelWidth80MHz:
            current_width = CHAN_WIDTH_80;
            break;

        case kCWChannelWidth160MHz:
            current_width = CHAN_WIDTH_160;
            break;

        default:
            break;
    }
    return current_width;
}

int osx_get_channels(const char* devname, struct channel_list* channels) {
    CWWiFiClient * wifiClient = [CWWiFiClient sharedWiFiClient];
    NSString * interfaceName = [[NSString alloc] initWithUTF8String: devname];
    CWInterface * currentInterface = [wifiClient interfaceWithName: interfaceName];
    [interfaceName release];

    NSSet * supportedChannelsSet = [currentInterface supportedWLANChannels];
    NSSortDescriptor * sort = [NSSortDescriptor sortDescriptorWithKey:@"channelNumber" ascending:YES];
    NSArray * sortedChannels = [supportedChannelsSet sortedArrayUsingDescriptors:[NSArray arrayWithObject:sort]];

    int i = 0;
    for (int i = 0; i < MAX_BANDS; ++i) {
        channels->band[i].num_channels = 0;
        channels->band[i].streams_rx = 0;
        channels->band[i].streams_tx = 0;
        channels->band[i].max_chan_width = CHAN_WIDTH_20;
    }
    channels->num_bands = MAX_BANDS;

    i = 0;
    NSInteger lastNum = -1;
    for( id eachChannel in sortedChannels )
    {
        NSInteger num = [eachChannel channelNumber];
        CWChannelBand band = [eachChannel channelBand];
        CWChannelWidth width = [eachChannel channelWidth];
        printlog("num: %ld, band: %ld, width: %ld", num, (long)band, (long)width);

        if (lastNum != num ) {
            channel_list_add(ieee80211_channel2freq(num));
        }

        int bandIdx = -1;
        if( kCWChannelBand2GHz == band ) {
            bandIdx = 0;
        } else if( kCWChannelBand5GHz == band ) {
            bandIdx = 1;
        }
        if( bandIdx >= 0) {
            if (lastNum != num ) {
                ++(channels->band[bandIdx].num_channels);
            }
            enum chan_width w = get_channel_width(width);
            channels->band[bandIdx].max_chan_width = \
            channels->band[bandIdx].max_chan_width < w ? w : channels->band[bandIdx].max_chan_width;
        }

        lastNum = num;
        if( ++i > MAX_CHANNELS) {
            break;
        }
    }

    printlog("band 0 channels: %d", channels->band[0].num_channels);
    printlog("band 1 channels: %d", channels->band[1].num_channels);

    return i;
}

bool ifctrl_init() {
    CWWiFiClient * wifiClient = [CWWiFiClient sharedWiFiClient];
    NSString * interfaceName = [[NSString alloc] initWithUTF8String: conf.ifname];
    CWInterface * currentInterface = [wifiClient interfaceWithName: interfaceName];
    [interfaceName release];

    [currentInterface disassociate];
    return true;
};

void ifctrl_finish() {
    CWWiFiClient * wifiClient = [CWWiFiClient sharedWiFiClient];
    NSString * interfaceName = [[NSString alloc] initWithUTF8String: conf.ifname];
    CWInterface * currentInterface = [wifiClient interfaceWithName: interfaceName];
    [interfaceName release];

    CWNetwork * _network = [[CWNetwork alloc] init];
    [currentInterface associateToNetwork:_network password:nil error:nil];
};

bool ifctrl_iwadd_monitor(__attribute__((unused))const char *interface, __attribute__((unused))const char *monitor_interface) {
    printlog("add monitor: not implemented");
    return false;
};

bool ifctrl_iwdel(__attribute__((unused))const char *interface) {
    printlog("iwdel: not implemented");
    return false;
};

bool ifctrl_iwset_monitor(__attribute__((unused))const char *interface) {
    printlog("set monitor: not implemented");
    return false;
};

bool ifctrl_iwset_freq(__attribute__((unused))const char *interface, __attribute__((unused))unsigned int freq,
                       __attribute__((unused))enum chan_width width, __attribute__((unused))unsigned int center) {
    if (osx_set_freq(interface, freq))
        return true;
    return false;
};

bool ifctrl_iwget_interface_info(__attribute__((unused))const char *interface) {
    printlog("get interface info: not implemented");
    return false;
};

bool ifctrl_iwget_freqlist(__attribute__((unused))int phy,  struct channel_list* channels) {
    int num_channels = osx_get_channels(conf.ifname, channels);
    if (num_channels)
        return true;
    return false;
};

bool ifctrl_is_monitor() {
    return true;
};
