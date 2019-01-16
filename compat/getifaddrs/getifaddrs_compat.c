#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <ifaddrs.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)getifaddrs; 
	exit(0);
}
