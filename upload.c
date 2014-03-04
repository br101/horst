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

#include "main.h"
#include "util.h"


#define UPLOAD_BUF_SIZE 2000

static char buffer[UPLOAD_BUF_SIZE];
static struct timeval last_time;
static int seqNo = 0;
static CURL *curl;


void
upload_init() {
	struct curl_slist *headers=NULL;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl) {
		DEBUG("XXXX curl not init");
		exit(1);
	}
	curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.4.9/posttest.php");

	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}


void
upload_finish(void) {
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


size_t nodes_info_json(char *buf) {
	struct node_info* n;
	struct packet_info* p;
	int len = 0;

	//len += snprintf(buf+len, UPLOAD_BUF_SIZE, "{ \"id\": \"%s\", ", ether_sprintf(p->wlan_src)); //TODO
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"time\": %d, ", (int)the_time.tv_sec);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"seq\": %d, ", seqNo++);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"window\": %d, ", conf.upload_interval);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"maclist\": [");

	list_for_each_entry(n, &nodes, list) {
		if (n->last_seen <= (the_time.tv_sec - conf.node_timeout))
			continue;
		p = &n->last_pkt;
		len += snprintf(buf+len, UPLOAD_BUF_SIZE, "{\"mac\": \"%s\", \"snr\": %ld}, ", ether_sprintf(p->wlan_src), ewma_read(&n->phy_snr_avg));
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
	if ((the_time.tv_sec - last_time.tv_sec) < conf.upload_interval) {
		return;
	}
	last_time = the_time;

	DEBUG("XXX uploading\n");

	ret = nodes_info_json(buffer);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, ret);

	ret = curl_easy_perform(curl);
	if (ret != CURLE_OK) {
		DEBUG("CURL failed: %s\n", curl_easy_strerror(ret));
	}

	curl_easy_getinfo (curl, CURLINFO_HTTP_CODE, &ret);
	DEBUG("*** ret %d", ret);
}
