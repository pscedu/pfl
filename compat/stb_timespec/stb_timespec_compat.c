#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	struct stat stb;

	(void)argc;
	(void)argv;
	memset(&stb, 0, sizeof(stb));
	(void)stb.st_atimespec.tv_nsec;
	exit(0);
}
