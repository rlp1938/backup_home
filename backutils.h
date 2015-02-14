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

#include "runutils.h"
#include "greputils.h"

void getconfig(const char *from, const char *to, char *thedev,
						char *mountpoint);
int getbupath(char *from, char *to, char *dev, char *budir,
						char *bupath);
char *recursedir(char *topdir, char *searchfor);
void logthis(const char *who, const char *msg, FILE *fplog);
int checkps(void);

#endif /* backutils.h */
