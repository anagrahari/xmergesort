#ifndef XMERGE_H_
#define XMERGE_H__


struct xmerge_args {
	char *inputfile1;
	char *inputfile2;
	char *outputfile;
	unsigned int flags;
	unsigned int *data;
};


#endif
