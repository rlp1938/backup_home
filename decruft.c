/*      decruft.c
 *
 *	Copyright 2015 Bob Parker rlp1938@gmail.com
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>

#include "fileops.h"
#include "greputils.h"
char progname[NAME_MAX];
static void usage(void);
static void stripcomments(char *pathfrom, char *pathto);
static void cpfile(const char *fr, const char *to);
static void stripws(char *target);

int main(int argc, char **argv)
{

	char home[NAME_MAX], config[NAME_MAX];

	// init grep paramaters
	ignore_case = 0;	/* -i option: ignore case */
	extended = 0;		/* -E option: use extended RE's */
	errors = 0;			/* number of errors */
	invert = 0;			/* find lines that don't match */


	strcpy(progname, basename(argv[0]));
	if (argc != 2) usage();

	if (fileexists(argv[1]) == -1) usage();

	char *logfile = argv[1];

	// find my configuration file.
	strcpy(home, getenv("HOME"));
	sprintf(config, "%s/.config/backup/cruft", home);
	if (fileexists(config) == -1) {
		fprintf(stderr, "No such file: %s\n", config);
		exit(EXIT_FAILURE);
	}

	// I need to make a 'cruft' in /tmp stripped of comments
	pid_t mypid = getpid();
	char tmpcruft[NAME_MAX];
	sprintf(tmpcruft, "/tmp/%s%dcruft", progname, mypid);
	stripcomments(config, tmpcruft);

	// read my comment stripped regexes into memory
	struct fdata rxdat = readfile(tmpcruft, 0, 1);
	dounlink(tmpcruft);
	mem2str(rxdat.from, rxdat.to);

	// I need 2 workfiles in /tmp
	char tmpwork_fr[NAME_MAX], tmpwork_to[NAME_MAX];
	sprintf(tmpwork_fr, "/tmp/%s%dfr", progname, mypid);
	sprintf(tmpwork_to, "/tmp/%s%dto", progname, mypid);

	// Initialise tmpwork_fr with content of logfile.
	cpfile(logfile, tmpwork_fr);

	// loop over the regexes 'grepping' the data within tmpwork_fr
	char *cp = rxdat.from;
	while (cp < rxdat.to) {
		//fprintf(stderr. "%s %d\n", tmpwork_fr, number);
		dogrep(tmpwork_to, tmpwork_fr, cp, "-E", "-v", NULL);
		dorename(tmpwork_to, tmpwork_fr);
		cp += strlen(cp) + 1;
	} // while()
	free(rxdat.from);

	// show results
	sync();
	cpfile(tmpwork_fr, "-");	// to stdout
	return 0;
}

void usage(void)
{
	fprintf(stderr, "\n\tUsage:\t%s logfilename\n", progname);
	exit(EXIT_FAILURE);

} //usage()

void stripcomments(char *pathfrom, char *pathto)
{
	FILE *fpo;
	struct fdata fdat;
	char line[NAME_MAX];

	fdat = readfile(pathfrom, 0, 1);
	mem2str(fdat.from, fdat.to);

	fpo = dofopen(pathto, "w");
	char *cp = fdat.from;
	while (cp < fdat.to) {
		if (cp[0] != '#') {
			char *cmt;
			strcpy(line, cp);
			cmt = strchr(line, '#');
			if (cmt) *cmt = '\0';
			stripws(line);
			fprintf(fpo, "%s\n", line);
		}
		cp += strlen(cp) + 1;
	} // while()
	free(fdat.from);
	fclose(fpo);
} // stripcomments()

void cpfile(const char *fr, const char *to)
{
	struct fdata frdat = readfile(fr, 0, 1);
	writefile(to, frdat.from, frdat.to, "w");
	free(frdat.from);
}

void stripws(char *target)
{	// strip white space if any from target
	size_t len;
	char *wrk, *cpf, *cpt;

	len = strlen(target);
	wrk = malloc(len + 1);
	if (!wrk) {
		perror("malloc()");
		exit(EXIT_FAILURE);
	}
	cpf = target;
	cpt = wrk;
	while(*cpf) {
		if (isspace(*cpf) == 0) {
			*cpt = *cpf;
			cpt++;
		}
		cpf++;
	} // while()
	*cpt = '\0';
	strcpy(target, wrk);
	free(wrk);
} // stripws()
