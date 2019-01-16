#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>

#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	struct rt_msghdr m;

	(void)argc;
	(void)argv;
	memset(&m, 0, sizeof(m));
	(void)m.rtm_hdrlen;
	exit(0);
}
