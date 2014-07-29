/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2014 Bruno Randolf (br1@einfach.org)
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <err.h>

#include "main.h"
#include "channel.h"
#include "control.h"

#define MAX_CMD 255

/* FIFO (named pipe) */

int ctlpipe = -1;

void
control_init_pipe(void)
{
	mkfifo(conf.control_pipe, 0666);
	ctlpipe = open(conf.control_pipe, O_RDWR|O_NONBLOCK);
}


void
control_send_command(char* cmd)
{
	int len = strlen(cmd);
	char new[len+1];
	char* pos;

	while (access(conf.control_pipe, F_OK) < 0) {
		printf("Waiting for control pipe...\n");
		sleep(1);
	}

	ctlpipe = open(conf.control_pipe, O_WRONLY);
	if (ctlpipe < 0)
		err(1, "Could not open control socket '%s'", conf.control_pipe);

	/* always terminate command with newline */
	strncpy(new, cmd, len);
	new[len] = '\n';
	new[len+1] = '\0';

	/* replace : with newline */
	while ((pos = strchr(new, ':')) != NULL) {
		*pos = '\n';
	}

	printf("Sending command: %s\n", new);

	write(ctlpipe, new, len+1);
	close(ctlpipe);
}


static void
parse_command(char* in) {
	char* cmd;
	char* val;
	int n;

	cmd = strsep(&in, "=");
	val = in;
	//printlog("RECV CMD %s VAL %s", cmd, val);

	/* commands without value */

	if (strcmp(cmd, "pause") == 0) {
		main_pause(1);
	}
	else if (strcmp(cmd, "resume") == 0) {
		main_pause(0);
	}

	/* all other commands require a value */

	if (val == NULL)
		return;

	else if (strcmp(cmd, "channel") == 0) {
		n = atoi(val);
		printlog("- CHANNEL = %d", n);
		conf.do_change_channel = 0;
		channel_change(channel_find_index_from_chan(n));
	}
	else if (strcmp(cmd, "channel_auto") == 0) {
		n = (strcmp(val, "1") == 0);
		printlog("- CHANNEL AUTO = %d", n);
		conf.do_change_channel = n;
	}
	else if (strcmp(cmd, "channel_dwell") == 0) {
		n = atoi(val);
		printlog("- CHANNEL DWELL = %d", n);
		conf.channel_time = n*1000;
	}
	else if (strcmp(cmd, "channel_upper") == 0) {
		n = atoi(val);
		printlog("- CHANNEL MAX = %d", n);
		conf.channel_max = n;
	}
	else if (strcmp(cmd, "outfile") == 0) {
		dumpfile_open(val);
	}
}


void
control_receive_command(void) {
	char buf[MAX_CMD];
	char *pos = buf;
	char *end;
	int len;

	len = read(ctlpipe, buf, MAX_CMD);
	if (len > 0) {
		buf[len] = '\0';
		/* we can receive multiple \n separated commands */
		while ((end = strchr(pos, '\n')) != NULL) {
			*end = '\0';
			parse_command(pos);
			pos = end + 1;
		}
	}
}


void
control_finish(void)
{
	if (ctlpipe == -1)
		return;

	close(ctlpipe);
	unlink(conf.control_pipe);
	ctlpipe = -1;
}
