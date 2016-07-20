#include <stdio.h>
#include <string.h>

#include "hutil.h"

void convert_string_to_mac(const char* string, unsigned char* mac)
{
	int c;
	for(c = 0; c < 6 && string; c++) {
		int x = 0;
		if (string)
			sscanf(string, "%x", &x);
		mac[c] = x;
		string = strchr(string, ':');
		if (string)
			string++;
	}
}

const char* kilo_mega_ize(unsigned int val) {
	static char buf[20];
	char c = 0;
	int rest;
	if (val >= 1024) { /* kilo */
		rest = (val & 1023) / 102.4; /* only one digit */
		val = val >> 10;
		c = 'k';
	}
	if (val >= 1024) { /* mega */
		rest = (val & 1023) / 102.4; /* only one digit */
		val = val >> 10;
		c = 'M';
	}
	if (c)
		snprintf(buf, sizeof(buf), "%d.%d%c", val, rest, c);
	else
		snprintf(buf, sizeof(buf), "%d", val);
	return buf;
}
