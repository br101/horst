/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2013 Bruno Randolf (br1@einfach.org)
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
#include "control.h"

#define MAX_CMD 255

/* FIFO (named pipe) */

int ctlpipe = -1;


void
control_init_pipe()
{
	mkfifo(conf.control_pipe, 0666);
	ctlpipe = open(conf.control_pipe, O_RDONLY|O_NONBLOCK);
}


void
control_send_command(char* cmd)
{
	ctlpipe = open(conf.control_pipe, O_WRONLY|O_NONBLOCK);
	if (ctlpipe < 0)
		err(1, "Could not open control socket '%s'", conf.control_pipe);

	printf("Sending command: %s\n", cmd);
	write(ctlpipe, cmd, strlen(cmd));
	close(ctlpipe);
}


static void
parse_command(char* in, int len) {
	char* cmd;
	char* val;
	int n;

	cmd = strsep(&in, "=");
	val = in;
	//printlog("RECV CMD %s VAL %s", cmd, val);

	/* commands without value */

	if (strcmp(cmd, "pause") == 0) {
		horst_pause(1);
	}
	else if (strcmp(cmd, "resume") == 0) {
		horst_pause(0);
	}

	/* all other commands require a value */

	if (val == NULL)
		return;

	else if (strcmp(cmd, "channel") == 0) {
		n = atoi(val);
		printlog("CHANNEL = %d", n);
		conf.do_change_channel = 0;
		change_channel(find_channel_index(n));
	}
	else if (strcmp(cmd, "channel_auto") == 0) {
		n = (strcmp(val, "1") == 0);
		printlog("CHANNEL AUTO = %d", n);
		conf.do_change_channel = n;
	}
	else if (strcmp(cmd, "channel_dwell") == 0) {
		n = atoi(val);
		printlog("CHANNEL DWELL = %d", n);
		conf.channel_time = n*1000;
	}
	else if (strcmp(cmd, "channel_upper") == 0) {
		n = atoi(val);
		printlog("CHANNEL MAX = %d", n);
		conf.channel_max = n;
	}
}


void
control_receive_command() {
	char buf[MAX_CMD];
	int len;

	len = read(ctlpipe, buf, MAX_CMD);

	if (len > 0) {
		if (buf[len-1] == '\n')
			len--;
		buf[len] = '\0';
		parse_command(buf, len);
	}
}


void
control_finish(void)
{
	close(ctlpipe);
	unlink(conf.control_pipe);
	ctlpipe = -1;
}
