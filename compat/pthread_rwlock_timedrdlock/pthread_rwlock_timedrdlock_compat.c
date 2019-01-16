#include <pthread.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)pthread_rwlock_timedrdlock;
	exit(0);
}
