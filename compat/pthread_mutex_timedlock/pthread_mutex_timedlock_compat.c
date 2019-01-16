#include <pthread.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)pthread_mutex_timedlock;
	exit(0);
}
