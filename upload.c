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

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include "main.h"
#include "util.h"


#define MAX_TIMEOUTS 3
#define UPLOAD_BUF_SIZE 2000

static char buffer[UPLOAD_BUF_SIZE];
static struct timeval last_time;
static int seqNo = 0;
static int timeouts = 0;

static CURL *curl;
static char errBuf[CURL_ERROR_SIZE+1];

static pthread_t thread;

/* when quit_mutex is unlocked, this signals the thread to quit */
static pthread_mutex_t quit_mutex;

/* upload_mutex is held while upload is in progress */
static pthread_mutex_t upload_mutex;

/* used to signal an upload was requested */
static pthread_cond_t upload_cond;
static int upload_requested = 0;


/*
 * CURL: check server response
 */
static size_t
curl_write_function(char *ptr, size_t size, size_t nmemb, void *userdata) {
	//printf("recv: %.*s (%d)", (int)(size*nmemb), ptr, (int)(size*nmemb));

	// check response
	if ((size*nmemb) != 4 || strncmp(ptr, "true", 4) != 0) {
		printlog("ERROR: Server returned %d bytes: %.*s", (int)(size*nmemb), (int)(size*nmemb), ptr);
	} else {
		printlog("Received 'true' from Server");
	}
	return size*nmemb;
}


/*
 * CURL: this is used to speed up stopping the thread when an upload is in progress
 * and we would have to wait for a timeout otherwise
 */
static int
curl_progress_function(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
	if (pthread_mutex_trylock(&quit_mutex) != EBUSY) {
		/* lock again for thead to actually finish  */
		pthread_mutex_unlock(&quit_mutex);
		return 1; /* abort */
	}
	return 0;
}


/*
 * CURL: do upload and wait for result
 */
static void
do_upload(void) {
	int ret;
	long code;

	printlog("Uploading to %s ...", conf.upload_server);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);

	/* add jitter < 900ms to avoid uploads at exactly the same time */
	usleep(rand() % 900000);

	ret = curl_easy_perform(curl);

	if (ret == CURLE_OPERATION_TIMEDOUT) {
		printlog("Upload timed out");
		if (++timeouts >= MAX_TIMEOUTS) {
			printlog("Too many timeouts, requesting fresh connection");
			timeouts = 0;
			curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);
		}
		return;
	} else if (ret == CURLE_ABORTED_BY_CALLBACK) {
		printlog("Upload aborted by user");
		return;
	} else if (ret != CURLE_OK) {
		printlog("ERROR: Upload failed: %s (%d)", curl_easy_strerror(ret), ret);
		printlog("ERROR: %s", errBuf);
		return;
	}

	ret = curl_easy_getinfo (curl, CURLINFO_HTTP_CODE, &code);
	if (ret != CURLE_OK) {
		printlog("ERROR: Could not get HTTP Response code");
		return;
	}
	if (code == 200) {
		printlog("Server sent HTTP Status code OK");
	} else {
		printlog("ERROR: Upload server status code %ld", code);
	}
}


/*
 * pthread: upload thread loop
 *
 * the whole upload is done under the upload_mutex
 */
static void *
upload_thread_run(void *arg)
{
	/* run until the quit_mutex is unlocked */
	while (pthread_mutex_trylock(&quit_mutex) == EBUSY) {
		/* wait for upload condition */
		pthread_mutex_lock(&upload_mutex);
		while (!upload_requested) {
			printlog("Upload thread: Waiting...");
			pthread_cond_wait(&upload_cond, &upload_mutex);
		}

		/* check quit mutex after wakeup again, it may have changed */
		if (pthread_mutex_trylock(&quit_mutex) != EBUSY)
			break;

		do_upload();
		upload_requested = 0;
		pthread_mutex_unlock(&upload_mutex);
	}
	printlog("Upload thread stopped");
	pthread_exit(NULL);
}


void
upload_init(void) {
	struct curl_slist *headers=NULL;
	int ret;

	if (conf.upload_server == NULL)
		return;

	/*
	 * initialize CURL
	 */
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl == NULL) {
		err(1, "Couldn't initialize CURL");
	}
	curl_easy_setopt(curl, CURLOPT_URL, conf.upload_server);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_function);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/cacert.pem");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf);

	headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
	headers = curl_slist_append(headers, "Accept: application/json;");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	/* initialize random number generator, used for upload jitter */
	srand((int)the_time.tv_usec);

	/*
	 * initialize pthread and start upload thread
	 */
	pthread_cond_init (&upload_cond, NULL);
	pthread_mutex_init(&upload_mutex, NULL);
	pthread_mutex_init(&quit_mutex, NULL);
	pthread_mutex_lock(&quit_mutex);

	ret = pthread_create(&thread, NULL, upload_thread_run, NULL);
	if (ret != 0) {
		err(1, "Couldn't create Upload thread");
	}
}


/*
 * called from exit signal handler
 * signal thread to finish and cleanup
 */
void
upload_finish(void) {
	if (conf.upload_server == NULL)
		return;

	/* signal thread to quit */
	pthread_mutex_unlock(&quit_mutex);

	/* wake up */
	pthread_mutex_unlock(&upload_mutex);
	upload_requested = 1;
	pthread_cond_signal(&upload_cond);
	pthread_mutex_unlock(&upload_mutex);

	printlog("exit: waiting for upload thread...");
	pthread_join(thread, NULL);

	pthread_mutex_destroy(&quit_mutex);
	pthread_mutex_destroy(&upload_mutex);
	pthread_cond_destroy(&upload_cond);

	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


/*
 * convert scan results to JSON
 */
static int
nodes_info_to_json(char *buf) {
	struct node_info* n;
	int len = 0;
	int count = 0;

	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "{\"ak\":\"65dd9657-b9f5-40d1-a697-3dc5dc31bbf4\",");
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"ch\":%d,", conf.current_channel);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"nm\":\"%s\",", ether_sprintf(conf.my_mac_addr));
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"ts\":%d,", (int)the_time.tv_sec);
	len += snprintf(buf+len, UPLOAD_BUF_SIZE, "\"sn\":%d,", seqNo);
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

	printlog("Sending %d with %d results", seqNo, count);
	seqNo++;
	return len;
}


/*
 * check if it's time to upload, if no upload is in progress,
 * signal upload thread to do its work
 */
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

	/*
	 * if we can't get the lock, the upload is still in progress.
	 * if not we can initiate a new upload
	 */
	ret = pthread_mutex_trylock(&upload_mutex);
	if (ret == EBUSY) {
		printlog("Previous upload still in progress, not uploading");
		return;
	} else if (ret == 0) {
		/* lock acquired */
		ret = nodes_info_to_json(buffer);
		upload_requested = 1;
		pthread_cond_signal(&upload_cond);
		pthread_mutex_unlock(&upload_mutex);
	} else {
		printlog("ERROR: acquiring mutex");
	}
}
