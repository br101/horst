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

const char* mac_sprint_short(const unsigned char *mac)
{
	static char etherbuf[5];
	sprintf(etherbuf, "%02x%02x",
		mac[4], mac[5]);
	return etherbuf;
}

const char* ip_sprintf(const unsigned int ip)
{
	static char ipbuf[18];
	unsigned char* cip = (unsigned char*)&ip;
	sprintf(ipbuf, "%d.%d.%d.%d",
		cip[0], cip[1], cip[2], cip[3]);
	return ipbuf;
}

const char* ip_sprintf_short(const unsigned int ip)
{
	static char ipbuf[5];
	unsigned char* cip = (unsigned char*)&ip;
	sprintf(ipbuf, ".%d", cip[3]);
	return ipbuf;
}

int normalize(float oval, int max_val, int max) {
	int val;
	val= (oval / max_val) * max;
	if (val > max) /* cap if still bigger */
		val = max;
	if (val == 0 && oval > 0)
		val = 1;
	if (val < 0)
		val = 0;
	return val;
}
