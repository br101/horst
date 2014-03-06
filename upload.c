/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2014 Bruno Randolf (br1@einfach.org)
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

#include <curl/curl.h>
#include <err.h>
#include <string.h>

#include "main.h"
#include "util.h"


#define UPLOAD_BUF_SIZE 2000

static char buffer[UPLOAD_BUF_SIZE];
static struct timeval last_time;
static int seqNo = 0;
static CURL *curl;


static size_t curl_write_function(char *ptr, size_t size, size_t nmemb, void *userdata) {
	//printf("recv: %.*s (%d)", (int)(size*nmemb), ptr, (int)(size*nmemb));

	// check response
	if ((size*nmemb) != 4 || strncmp(ptr, "true", 4) != 0) {
		printlog("ERROR: Server returned %.*s", (int)(size*nmemb), ptr);
	}
	return size*nmemb;
}


void
upload_init() {
	struct curl_slist *headers=NULL;

	if (conf.upload_server == NULL)
		return;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl) {
		err(1, "Couldn't initialize CURL");
	}
	curl_easy_setopt(curl, CURLOPT_URL, conf.upload_server);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);

	headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
	headers = curl_slist_append(headers, "Accept: application/json;");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}


void
upload_finish(void) {
	if (conf.upload_server == NULL)
		return;

	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


int nodes_info_json(char *buf) {
	struct node_info* n;
	int len = 0;
	int prev = 0;

	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "{\"ak\":\"65dd9657-b9f5-40d1-a697-3dc5dc31bbf4\",");
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"ch\":%d,", conf.current_channel);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"nm\":\"%s\",", ether_sprintf(conf.my_mac_addr));
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"ts\":%d,", (int)the_time.tv_sec);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"sn\":%d,", seqNo++);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"sw\":%d,", conf.upload_interval);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"scans\":[");

	list_for_each_entry(n, &nodes, list) {
		/* only send nodes we have seen in the last interval */
		if (n->last_seen <= (the_time.tv_sec - conf.upload_interval))
			continue;

		len += snprintf(buf+len, UPLOAD_BUF_SIZE, "%s{\"mac\":\"%s\",\"rssi\":%ld,\"ssid\":\"%s\"}",
				(prev ? ", " : ""),
				ether_sprintf(n->last_pkt.wlan_src),
				ewma_read(&n->phy_snr_avg),
				(n->essid != NULL) ? n->essid->essid : "");
		prev = 1;
	}

	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "]}");
	return len;
}


void
upload_check(void)
{
	int ret;
	/*
	 * upload only in conf.upload_interval intervals (seconds)
	 */
	if ((conf.upload_server == NULL) ||
	    (the_time.tv_sec - last_time.tv_sec) < conf.upload_interval)
		return;

	last_time = the_time;

	DEBUG("Uploading to %s\n", conf.upload_server);

	ret = nodes_info_json(buffer);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, ret);

	ret = curl_easy_perform(curl);
	if (ret != CURLE_OK) {
		printlog("ERROR: Upload failed: %s\n", curl_easy_strerror(ret));
	}

	curl_easy_getinfo (curl, CURLINFO_HTTP_CODE, &ret);
	if (ret != 200) {
		printlog("ERROR: Upload server status code %d\n", ret);
	}
}
