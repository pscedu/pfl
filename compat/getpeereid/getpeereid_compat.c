#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)getpeereid;
	exit(0);
}
