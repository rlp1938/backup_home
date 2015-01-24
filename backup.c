/*      backup.c
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
static char *frfmt =
"This the first run of backup. Backup needs to copy two files, "
"%s and %s from "
"/usr/local/share. The copies will be installed in %s. You should edit "
"these files to ensure they meet your needs. In the configuration file "
"you name the device on which the backup directory resides and also "
"name the actual backup directory. There is no need to name the device "
"number. Backup will find the named backup dir if indeed a backup "
"device is mounted. The minimum content of the exclusions file is "
"the set of files that are altered by the backup program itself. You "
"can add to that anything you don't want to backup. If you used a "
"prefix other than /usr/local when you built the program you will need "
"to copy the above named files into your config directory by hand."
;

int main(int argc, char **argv)
{
	char home[NAME_MAX], user[NAME_MAX];
	struct fdata conf, mtab, logs;
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
	sprintf(wrkdir, "%slog/dryrun.log", home);
	char *cruftfile = dostrdup(wrkdir);	// cruft
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
	sprintf(wrkdir, "%slog/bulog.bak", home);
	char *bulog = dostrdup(wrkdir);	// safety copy of the log.
	sprintf(wrkdir, "%slog/errors.log", home);
	char *errlog = dostrdup(wrkdir);	// log errors.

	dogetenv("LOGNAME", user);

	// redirect stdout, stderr
	fpo = dofreopen(logfile, "a", stdout);
	fpe = dofreopen(errlog, "a", stderr);


	// Do we have both the excludes file and config file in place?
	if (filexists(cfgfile, &fsize) == -1 ||
			filexists(exclfile, &fsize) == -1) {
		// the first run text
		sprintf(wrkdir, frfmt, "excludes", "backup.cfg", cfgdir);
		char *txt = dostrdup(wrkdir);
		firstrun(txt, "/usr/local/share/", cfgdir, "backup.cfg",
					"excludes", NULL);
		free(txt);
		fputs("\nPlease edit these files and run backup again.\n",
				stdout);
		exit(EXIT_SUCCESS);
	}

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

	// now see what is going on in mtab
	mtab = readfile("/etc/mtab", 0, 1);
	if (getbupath(mtab.from, mtab.to, bdev, dirname, wrkdir) == -1) {
		logthis(progname, "Backup drive is not mounted. Will quit.\n",
				fpe);
		exit(EXIT_FAILURE);
	}
	if (wrkdir[strlen(wrkdir) -1 ] != '/') strcat(wrkdir, "/");
	char *bupath = strdup(wrkdir);

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

	// do the log and lock dirs exist in $HOME? If not make 'em.
	if (direxists(logdir) == -1) {
		// make it then.
		sprintf(command, "mkdir %s", logdir);
		dosystem(command);
	}
	if (direxists(lockdir) == -1) {
		// make it then.
		sprintf(command, "mkdir %s", lockdir);
		dosystem(command);
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

	// formulate the first command to rsync
	/* This first command does not use the --del option because I do
	 * not want to run the risk of propagating a 'hole', ie missing
	 * file or dir through the backup set.
	* */
	sprintf(command, "/usr/bin/rsync -av --links --hard-links "
	"--exclude-from=%s %s %s", exclfile, home, bupath);
	dosystem(command);

	fclose(fpo);	// Force the writes in correct order
	fpo = dofreopen(logfile, "a", stdout);
	sync();

	// put date finished and a ruleoff.
	logthis(progname, "Ended backup.", stdout);
	fprintf(stdout, "------\n\n");

	fclose(fpo);	// Force the writes in correct order
	fpo = dofreopen(logfile, "a", stdout);
	sync();

	// want to split the dry run stuff out of backup.log to record it
	// separately. So put xml markers around that stuff.
	fprintf(stdout, "%s\n", "<dryrun>");
	logthis(progname, "Begin dry run.", stdout);

	fclose(fpo);	// Force the writes in correct order
	fpo = dofreopen(logfile, "a", stdout);
	sync();

	// formulate the second command to rsync
	/* This second version uses the --del option but also uses
	 * --dry-run and directs the output to $HOME/log/dryrun.log for
	 * manual review of what would have been deleted. Unfortunately
	 * this file is overloaded with browser crap. Run decrap to strip
	 * that out and then manually review what is left before running
	 * cleanup.
	 * */
	sprintf(command, "/usr/bin/rsync -av --dry-run --links --hard-links"
			" --del --exclude-from=%s %s %s", exclfile, home, bupath);
	dosystem(command);

	fclose(fpo);	// Force the writes in correct order
	fpo = dofreopen(logfile, "a", stdout);
	sync();

	// put date finished and a ruleoff.
	logthis(progname, "Ended dry run.", stdout);
	fprintf(stdout, "------\n\n");
	fprintf(stdout, "</dryrun>\n");
	fclose(fpo);	// closes the logfile.
	sync();

	// now split the log
	logs = readfile(logfile, 0, 1);
	if (logs.from) {
		char *lgfr, *lgto, *drfr, *drto;
		dorename(logfile, bulog);	// backup copy of log
		lgfr = logs.from;
		lgto = memmem(lgfr, logs.to - lgfr, "<dryrun>",
						strlen("<dryrun>"));
		if (!lgto) {
			dorename(bulog, logfile);	// keep existing log
			exit(EXIT_FAILURE);
		}
		writefile(logfile, lgfr, lgto);	// new copy of logfile.
		drfr = lgto + strlen("<dryrun>") + 1;
		drto = memmem(drfr, logs.to - drfr, "</dryrun>",
						strlen("</dryrun>"));
		writefile(cruftfile, drfr, drto);	// new copy of cruftfile.
		/* Ok, we made two out of the one. Using the pointers as I have
		 * means that there will be no xml tags in either file. They
		 * are not needed. Apart from that the entire original log is
		 * a concatenation of the two subfiles. I can now safely rm the
		 * original log. NB I write a new copy of the cruft file as all
		 * deleting notifications written to it will repeat until such
		 * time as the cleanup program is run.
		*/
		dounlink(bulog);	// done with it.
	}

	// clear the lock
	dounlink(lockfile);

finis:
	fclose(fpe);	// the error log.
	// free my heap strings
	free(bupath);
	free(errlog);
	free(bulog);
	free(tracefile);
	free(cfgdir);
	free(exclfile);
	free(lockdir);
	free(logdir);
	free(cruftfile);
	free(logfile);
	free(lockfile);
	free(cfgfile);

	return 0;
} // main()
