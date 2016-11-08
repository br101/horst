#ifndef _HORST_UTIL_H_
#define _HORST_UTIL_H_

void convert_string_to_mac(const char* string, unsigned char* mac);
const char* kilo_mega_ize(unsigned int val);
const char* mac_sprint_short(const unsigned char *mac);
const char* ip_sprintf(const unsigned int ip);
const char* ip_sprintf_short(const unsigned int ip);
int normalize(float val, int max_val, int max);

static inline int normalize_db(int val, int max)
{
	if (val <= 30)
		return 0;
	else if (val >= 100)
		return max;
	else
		return normalize(val - 30, 70, max);
}

#endif
