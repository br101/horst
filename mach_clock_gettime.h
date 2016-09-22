/*
 * mach_clock_gettime --- an emulation of POSIX's `clock_gettime` for
 * Mach / Mac OS X.
 *
 * See mach_clock_gettime.c for details.
 */

#ifndef _MACH_CLOCK_GETTIME_H_
#define _MACH_CLOCK_GETTIME_H_

#include <time.h>

#define CLOCK_MONOTONIC 1	/* Per Linux's time.h */
typedef int clockid_t;		/* Per Linux's types.h, posix_types.h */

/* Per the POSIX Realtime Extensions */
int clock_gettime(clockid_t clock_id, struct timespec *tp);

#endif

