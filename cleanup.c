/*      cleanup.c
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

#include "backutils.h"
#include "fileops.h"
#include "firstrun.h"	// for dosystem()

// Globals
char progname[NAME_MAX];

int main(int argc, char **argv)
{
	char home[NAME_MAX], user[NAME_MAX];
	struct fdata conf, mtab;
	char dirname[NAME_MAX], bdev[32], wrkdir[PATH_MAX],
				command[NAME_MAX];
	FILE *fpo, *fpe;

	strcpy(progname, basename(argv[0]));
	/* The POS below successfully stops gcc bitching but then gives
	 * cppcheck something to barf about. I'll leave it like that as gcc
	 * is run often but cppcheck much less.
	*/
	if(argc) argc = argc;

	strcpy(home, getenv("HOME"));
	// prepare home to suit rsync.
	if (home[strlen(home) -1 ] != '/') strcat(home, "/");

	// setup my file paths
	sprintf(wrkdir, "%s.config/backup/backup.cfg", home);
	char *cfgfile = strdup(wrkdir);	// config file.
	sprintf(wrkdir, "%slock/backup.lock", home);
	char *lockfile = strdup(wrkdir);	// lock file
	sprintf(wrkdir, "%slog/backup.log", home);
	char *logfile = strdup(wrkdir);	// log file
	sprintf(wrkdir, "%slog", home);
	char *logdir = strdup(wrkdir);	// log dir, to mkdir if needed.
	sprintf(wrkdir, "%slock", home);
	char *lockdir = strdup(wrkdir);	// lock dir, to mkdir if needed.
	sprintf(wrkdir, "%s.config/backup/", home);
	char *cfgdir = strdup(wrkdir);	// config dir.
	sprintf(wrkdir, "%sexcludes", cfgdir);
	char *exclfile = strdup(wrkdir);	// excludes file
	sprintf(wrkdir, "%slog/trace.txt", home);
	char *tracefile = strdup(wrkdir);	// trace wtf
	sprintf(wrkdir, "%slog/errors.log", home);
	char *errlog = strdup(wrkdir);	// log errors.

	strcpy (user, getenv("LOGNAME"));

	// redirect stdout, stderr
	fpo = dofreopen(logfile, "a", stdout);
	fpe = dofreopen(errlog, "a", stderr);

	// nice() this thing, I don't want the slightest delay from it.
	errno = 0;
	if (nice(19) == -1) {
		// -1 may be a legitimate return value.
		if(errno){
			perror("nice()");
			exit(EXIT_FAILURE);
		}
	}
	conf = readfile(cfgfile, 0, 1);	// config stuff.
	getconfig(conf.from, conf.to, bdev, dirname);
	free(conf.from);

	// now see what is going on in mtab
	mtab = readfile("/etc/mtab", 0, 1);
	if (getbupath("/etc/mtab", bdev, dirname, wrkdir) == -1) {
		sprintf(command, "/bin/date");
		dosystem(command);
		fprintf(stderr, "Backup drive is not mounted, quitting.\n");
		exit(EXIT_FAILURE);
	}
	if (wrkdir[strlen(wrkdir) -1 ] != '/') strcat(wrkdir, "/");
	char *bupath = strdup(wrkdir);

	free(mtab.from);

	// Where am I?
	if (!getcwd(wrkdir, PATH_MAX)) {
		perror("getcwd()");
		exit(EXIT_FAILURE);
	}
	if (wrkdir[strlen(wrkdir) -1 ] != '/') strcat(wrkdir, "/");
	if (strcmp(wrkdir, home) != 0) {
		if (chdir(home) == -1) {
			perror(home);
			exit(EXIT_FAILURE);
		}
	}

	// Is an earlier instance of any backup program running?
	/* I will avoid using lockfiles, just look for what is already
	 * running.
	*/
	{
		char *prlist[4] = {
				"backup", "bulogrot", "cleanup", NULL
		};
		int res = isrunning(prlist);
		// res will be > 1 if another program in the list is running.
		if (res > 1) {
			logthis(progname, "An earlier backup program is running."
					" Will quit.", stderr);
			goto finis;
		}
	}

	// record beginning date
	logthis(progname, "Begin backup.", stdout);
	logthis("Backup destination:", bupath, stdout);
	fclose(fpo);	// Force the writes in correct order
	fpo = dofreopen(logfile, "a", stdout);
	sync();

	// formulate the command to rsync
	sprintf(command, "/usr/bin/rsync -av --links --hard-links --del "
	"--exclude-from=%s %s %s", exclfile, home, bupath);
	dosystem(command);

	fclose(fpo);	// Force the writes in correct order
	fpo = dofreopen(logfile, "a", stdout);
	sync();

	// put date finished and a ruleoff.
	logthis(progname, "Ended backup.", stdout);
	fprintf(stdout, "------\n\n");

	// clear the lock
	dounlink(lockfile);

finis:
	fclose(fpo);	// closes the logfile.
	fclose(fpe);	// the error log.
	// free my heap strings
	free(bupath);
	free(errlog);
	free(tracefile);
	free(cfgdir);
	free(exclfile);
	free(lockdir);
	free(logdir);
	free(logfile);
	free(lockfile);
	free(cfgfile);

	return 0;
} // main()
