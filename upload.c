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
#include <pthread.h>

#include "main.h"
#include "util.h"


#define UPLOAD_BUF_SIZE 2000

static char buffer[UPLOAD_BUF_SIZE];
static struct timeval last_time;
static int seqNo = 0;
static int timeouts = 0;
static CURL *curl;
static pthread_t thread;


static size_t curl_write_function(char *ptr, size_t size, size_t nmemb, void *userdata) {
	//printf("recv: %.*s (%d)", (int)(size*nmemb), ptr, (int)(size*nmemb));

	// check response
	if ((size*nmemb) != 4 || strncmp(ptr, "true", 4) != 0) {
		printlog("ERROR: Server returned %.*s", (int)(size*nmemb), ptr);
	} else {
		printlog("Received 'true' from Server");
	}
	return size*nmemb;
}

static char errBuf[CURL_ERROR_SIZE +1];

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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	//curl_easy_setopt(curl, CURLOPT_CAINFO, "/root/ca/infsoft.crt");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf);

	headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
	headers = curl_slist_append(headers, "Accept: application/json;");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}


void
upload_finish(void) {
	if (conf.upload_server == NULL)
		return;

	printlog("waiting for thread...");
	pthread_join(thread, NULL);
	printlog("done.");

	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


int nodes_info_json(char *buf) {
	struct node_info* n;
	int len = 0;
	int count = 0;

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
				(count > 0 ? "," : ""),
				ether_sprintf(n->last_pkt.wlan_src),
				-ewma_read(&n->phy_sig_avg),
				(n->essid != NULL) ? n->essid->essid : "");
		count++;
	}

	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "]}");

	printlog("Sending %d scan results", count);
	return len;
}


void *thread_function(void *arg)
{
	int ret;
	long code;

	printlog("in thread2");

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char*)arg);
	//curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, ret);
	//CURLOPT_FRESH_CONNECT

	ret = curl_easy_perform(curl);
	if (ret != CURLE_OK) {
		printlog("ERROR: Upload failed: %s (%d)\n", curl_easy_strerror(ret), ret);
		printlog("ERROR: %s", errBuf);
		if (ret == CURLE_OPERATION_TIMEDOUT)
			timeouts++;
		goto err_out;
	}

	ret = curl_easy_getinfo (curl, CURLINFO_HTTP_CODE, &code);
	if (ret != CURLE_OK) {
		printlog("ERROR: Could not get HTTP Response code");
		goto err_out;
	}
	if (code == 200) {
		printlog("Server sent HTTP Status code OK");
	} else {
		printlog("ERROR: Upload server status code %ld\n", code);
	}
err_out:
	printlog("thread done");
	pthread_exit(NULL);
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

	printlog("Uploading to %s ...", conf.upload_server);

	ret = nodes_info_json(buffer);

	ret = pthread_create(&thread, NULL, thread_function, (void *)buffer);
	if (ret != 0) {
		printlog("could not create thread");
	}
	printlog("upload done");
}
