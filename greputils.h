/*
 * greputils.h
 * 	Copyright 2011 Bob Parker <rlp1938@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	MA 02110-1301, USA.
*/

#ifndef GREPUTILS_H
# define GREPUTILS_H
#define _GNU_SOURCE 1		/* for getline() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <regex.h>
#include <sys/types.h>

char *myname;		/* for error messages */
int ignore_case;	/* -i option: ignore case */
int extended;		/* -E option: use extended RE's */
int errors;			/* number of errors */
int invert;			/* find lines that don't match */

regex_t pattern;	/* pattern to match */

void compile_pattern(const char *pat);
void process(const char *name, FILE *fp, FILE *fpo);
void dogrep(const char *file2write, const char *file2grep,
			const char *regex, ...);

#endif
