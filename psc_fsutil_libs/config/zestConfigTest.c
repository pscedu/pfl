/* $Id: zestConfigTest.c 1283 2007-07-17 16:43:48Z yanovich $ */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zestConfig.h"
#include "zestLog.h"

char *f;

int getOptions(int argc,  char *argv[]);

int main(int argc,  char *argv[])
{
	zest_setloglevel(ZLL_NOTICE);

	getOptions(argc, argv);

	zestGetConfig(f);

	exit(0);
} 

int getOptions(int argc,  char *argv[])
{
#define ARGS "i:l:"
	int c, err = 0;
	optarg = NULL;

	while ( !err && ((c = getopt(argc, argv, ARGS)) != -1))
		switch (c) {

		case 'l':
			zest_setloglevel(atoi(optarg));
                        break;
			
		case 'i':
			f = optarg;
			break;

		default :
			fprintf(stderr, "what?\n");
		}

	return 0;
} 
