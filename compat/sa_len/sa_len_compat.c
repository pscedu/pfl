#include <sys/socket.h>

#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	struct sockaddr sa;

	(void)argc;
	(void)argv;
	memset(&sa, 0, sizeof(sa));
	(void)sa.sa_len;
	exit(0);
}
