/*      cleanup.c
 *
 *	Copyright 2014/12/23 Bob Parker <rlp1938@gmail.com>
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

#include "fileutil.h"
#include "runutils.h"
#include "backutils.h"

// Globals


int main(int argc, char **argv)
{
	char home[NAME_MAX], user[NAME_MAX];
	struct fdata conf, mtab;
	char dirname[NAME_MAX], bdev[32], wrkdir[PATH_MAX],
				command[NAME_MAX];
	off_t fsize;
	FILE *fpo, *fpe;

	strcpy(progname, basename(argv[0]));
	/* The POS below successfully stops gcc bitching but then gives
	 * cppcheck something to barf about. I'll leave it like that as gcc
	 * is run often but cppcheck much less.
	*/
	if(argc) argc = argc;

	dogetenv("HOME", home);
	// prepare home to suit rsync.
	if (home[strlen(home) -1 ] != '/') strcat(home, "/");

	// setup my file paths
	sprintf(wrkdir, "%s.config/backup/backup.cfg", home);
	char *cfgfile = dostrdup(wrkdir);	// config file.
	sprintf(wrkdir, "%slock/backup.lock", home);
	char *lockfile = dostrdup(wrkdir);	// lock file
	sprintf(wrkdir, "%slog/backup.log", home);
	char *logfile = dostrdup(wrkdir);	// log file
	sprintf(wrkdir, "%slog", home);
	char *logdir = dostrdup(wrkdir);	// log dir, to mkdir if needed.
	sprintf(wrkdir, "%slock", home);
	char *lockdir = dostrdup(wrkdir);	// lock dir, to mkdir if needed.
	sprintf(wrkdir, "%s.config/backup/", home);
	char *cfgdir = dostrdup(wrkdir);	// config dir.
	sprintf(wrkdir, "%sexcludes", cfgdir);
	char *exclfile = dostrdup(wrkdir);	// excludes file
	sprintf(wrkdir, "%slog/trace.txt", home);
	char *tracefile = dostrdup(wrkdir);	// trace wtf
	sprintf(wrkdir, "%slog/errors.log", home);
	char *errlog = dostrdup(wrkdir);	// log errors.

	dogetenv("LOGNAME", user);

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
	if (getbupath(mtab.from, mtab.to, bdev, dirname, wrkdir) == -1) {
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

	// Is an earlier instance of backup running?
	if (filexists(lockfile, &fsize) == 0) {
		// rsync may have crashed us out on an earlier run.
		fclose(fpo);	// checkps needs stdout
		if (checkps() == 0) {
			// found something.
			logthis(progname, "An earlier instance is running."
					"Will quit.", stderr);
			goto finis;
		} else {
			logthis(progname, "An earlier instance had crashed."
					"Will continue.", stderr);
		}
		fpo = dofreopen(logfile, "a", stdout);
	} else {
		// Install the lock file
		FILE *fpl = dofopen(lockfile, "w");
		fclose(fpl);
	}
	sync();

	// record beginning date
	logthis(progname, "Begin backup.", stdout);
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
