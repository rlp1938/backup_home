#ifndef _CHECKPS_H
#define _CHECKPS_H 1

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>
#include <time.h>

//#include "runutils.h"
//#include "greputils.h"
void *memmem(const void *haystack, size_t haystacklen,
                    const void *needle, size_t needlelen);

int checkps(void);

#endif /* checkps.h */
