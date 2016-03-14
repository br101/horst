#include <errno.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <err.h>

#include "ifctrl.h"

bool ifctrl_flags(const char *const interface, bool up, bool promisc)
{
	int fd;
	struct ifreq ifreq;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		return false;

	memset(&ifreq, 0, sizeof(ifreq));
	strncpy(ifreq.ifr_name, interface, IF_NAMESIZE - 1);

	if (ioctl(fd, SIOCGIFFLAGS, &ifreq) == -1) {
		warn("Could not get flags for %s", interface);
		int orig_errno = errno;
		close(fd);
		errno = orig_errno;
		return false;
	}

	if (up)
		ifreq.ifr_flags |= IFF_UP;
	else
		ifreq.ifr_flags &= ~IFF_UP;

	if (promisc)
		ifreq.ifr_flags |= IFF_PROMISC;
	else
		ifreq.ifr_flags &= ~IFF_PROMISC;

	if (ioctl(fd, SIOCSIFFLAGS, &ifreq) == -1) {
		warn("Could not set flags for %s", interface);
		int orig_errno = errno;
		close(fd);
		errno = orig_errno;
		return false;
	}

	if (close(fd))
		return false;

	return true;
}
