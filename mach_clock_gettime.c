/*
 * mach_clock_gettime --- an emulation of POSIX's `clock_gettime` for
 * Mach / Mac OS X.
 *
 * This library emulates a function from the POSIX Realtime Extensions
 * for getting monotonically increasing clock readings (`clock_gettime`
 * with a `clock_id` of `CLOCK_MONOTONIC`) using Mac OS X's `clock_get_time`
 * call on the monotonically increasing `SYSTEM_CLOCK` clock service.
 *
 * It is designed as a drop-in replacement for that specific purpose:
 * Do a conditional include of this library on systems that define
 * __MACH__, then use `clock_gettime(CLOCK_MONOTONIC, &...)` as if on
 * a system that implements the POSIX Realtime Extensions.
 *
 * Other clock sources, alarms, etc. are currently not supported.
 *
 * This library is based on material by
 * - The Open Group, see
 *     http://pubs.opengroup.org/onlinepubs/9699919799/functions/clock_gettime.html
 * - the Linux kernel headers, see time.h, posix_types.h and types.h
 * - The XNU man pages, see host_get_clock_service, clock_get_time and
     tvalspec under https://opensource.apple.com/source/xnu/xnu-3248.60.10/osfmk/man/
 * - https://stackoverflow.com/questions/11680461/monotonic-clock-on-osx
 *
 * Other similar (and probably more complete) approaches exist and inspired
 * this implementation, see for instance
 * - https://gist.github.com/jbenet/1087739
 * - https://gist.github.com/alfwatt/3588c5aa1f7a1ef7a3bb
 * - https://github.com/ChisholmKyle/PosixMachTiming
 *
 * Licensing note: Given its vanishing amount of originality, this library
 * is placed under a CC0 license, i.e. I release it into the public domain.
 * See also https://creativecommons.org/publicdomain/zero/1.0/legalcode
 *
 * Please send bug reports to albert.rafetseder@univie.ac.at
 */

#include <sys/types.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <err.h>

#include "mach_clock_gettime.h"

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	/* The XNU docs prescribe `clock_t` and `tvalspec_t` for the
	 * first two variables; we however use what the header file
	 * in /usr/include/mach/clock.h tells us to (or else the
	 * preprocessor complains). */
	clock_serv_t clock_name;
	mach_timespec_t current_clock_value;
        kern_return_t retval;

	/* Bail out on clock_id's that we don't emulate */
	if (clock_id != CLOCK_MONOTONIC)
		err(1, "Unsupported clock_id for mach_clock_gettime emulation");

	/* XXX This could use some more error checking... */
	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock_name);
	retval = clock_get_time(clock_name, &current_clock_value);
	mach_port_deallocate(mach_task_self(), clock_name);
	tp->tv_sec = current_clock_value.tv_sec;
	tp->tv_nsec = current_clock_value.tv_nsec;

	return retval;
}

