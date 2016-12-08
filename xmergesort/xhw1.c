#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "xmergesort.h"

#ifndef __NR_xmergesort
#error xmergesort system call not defined
#endif


int main(int argc, char *argv[])
{
	extern int optind;
	unsigned int flag = 0;
	char *inputfile1 = NULL;
	char *inputfile2 = NULL;
	char *outputfile = NULL;
	unsigned int *data = NULL;
	char c;
	int rc;
	int u = 0, a = 0, i = 0, t = 0, d = 0;
	int badflag = 0;

	while ((c = getopt(argc, argv, "uaitd")) != -1)
		switch (c) {
		case 'u':
			flag += 1;
			u += 1;
			break;
		case 'a':
			flag += 2;
			a += 1;
			break;
		case 'i':
			flag += 4;
			i += 1;
			break;
		case 't':
			flag += 16;
			t += 1;
			break;
		case 'd':
			flag += 32;
			d += 1;
			data = (unsigned int *)malloc(sizeof(unsigned int));
			break;
		case '?':
			badflag += 1;
			break;
		}

	if (u > 1 || a > 1 || i > 1 || t > 1 || d > 1) {
		printf("bad flags. syscall execution failed\n");
		goto out;
	}

	if (badflag > 0) {
		printf("bad flags. syscall execution failed\n");
		goto out;
	}

	if (u == 0 && a == 0)
		flag += 1;

	argc = argc - optind;
	argv = argv + optind;

	if (argc != 3) {
	printf("invalid file input. syscall execution failed\n");
	goto out;
	}

	outputfile = *argv;
	if (!outputfile) {
		printf(" output file missing. syscall execution failed\n");
		goto out;
	}
	argv++;


	inputfile1 = *argv;
	if (!inputfile1) {
		printf(" invalid file input. syscall execution failed\n");
		goto out;
	}
	argv++;

	inputfile2 = *argv;
	if (!inputfile2) {
		printf("invalid file input. syscall execution failed\n");
		goto out;
	}

	struct xmerge_args *args = (struct xmerge_args *)malloc(sizeof(struct xmerge_args));

	if (!args) {
		printf(" memory allocation failed. syscall execution failed\n");
		goto out;
	}

	args->inputfile1 = (char *)inputfile1;
	args->inputfile2 = (char *)inputfile2;
	args->outputfile = (char *)outputfile;
	args->flags = flag;
	args->data = data;

	void *dummy = (void *) args;

	rc = syscall(__NR_xmergesort, dummy);
	if (rc == 0) {
		printf("syscall returned %d\n", rc);
		if (flag & 0x20)
			printf("no. of sorted records are %d \n", *args->data);
	} else {
		printf("syscall returned %d (errno = %d)\n", rc, errno);
		perror("xmergesort error ");
	}

out:
	if (args)
		free(args);
	if (data)
		free(data);
	exit(rc);
}
