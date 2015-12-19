#ifndef _BACKUTILS_H
#define _BACKUTILS_H 1

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
#include "fileops.h"
#include "greputils.h"

void *memmem(const void *haystack, size_t haystacklen,
                    const void *needle, size_t needlelen);

void getconfig(const char *from, const char *to, char *theregex,
						char *thedir);
int getbupath(char *mtabfile, char *bregex, char *budir,
						char *bupath);
char *recursedir(char *topdir, char *searchfor);
void logthis(const char *who, const char *msg, FILE *fplog);

#endif /* backutils.h */
