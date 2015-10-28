#include <errno.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ifctrl.h"

static bool ifctrl_ifupdown(const char *const interface, char up)
{
	int fd;
	struct ifreq ifreq;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		return false;

	memset(&ifreq, 0, sizeof(ifreq));
	strncpy(ifreq.ifr_name, interface, IF_NAMESIZE - 1);

	if (ioctl(fd, SIOCGIFFLAGS, &ifreq) == -1) {
		int orig_errno = errno;
		close(fd);
		errno = orig_errno;
		return false;
	}

	if (up)
		ifreq.ifr_flags |= IFF_UP;
	else
		ifreq.ifr_flags &= ~IFF_UP;

	if (ioctl(fd, SIOCSIFFLAGS, &ifreq) == -1) {
		int orig_errno = errno;
		close(fd);
		errno = orig_errno;
		return false;
	}

	if (close(fd))
		return false;

	return true;
}

bool ifctrl_ifdown(const char *const interface)
{
	return ifctrl_ifupdown(interface, 0);
}

bool ifctrl_ifup(const char *const interface)
{
	return ifctrl_ifupdown(interface, 1);
}
