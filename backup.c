/*      backup.c
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
#include "firstrun.h"
#include "backutils.h"



// Globals

char *helpmsg = "\n\tUsage: backup [option]\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message and exits.\n"
  "\t-b program_or_script. Runs the named program before the actual\n"
  "\t   backup starts. The option may be repeated for as many programs\n"
  "\t   are required to run. There is no limit other than available\n"
  "\t   memory. \n"
  "\t-a program_or_script. Runs the named program after the backup\n"
  "\t   completes. The option may be repeated as above.\n"
  ;

void dohelp(int forced);
char progname[NAME_MAX];

int main(int argc, char **argv)
{
	char home[NAME_MAX], user[NAME_MAX];
	struct fdata conf, logs;
	char dirname[NAME_MAX], bdev[32], wrkdir[PATH_MAX],
				command[NAME_MAX];
	FILE *fpo, *fpe;
	int opt;

	char **runbefore = malloc(10 * sizeof(char *));
	int beforeruns = 0;
	int beforemax = 10;
	char **runafter = malloc(10 * sizeof(char *));
	int afterruns = 0;
	int aftermax = 10;
	memset(runbefore, 0, beforemax * sizeof(char *));
	memset(runafter, 0, aftermax * sizeof(char *));

	while((opt = getopt(argc, argv, ":hb:a:")) != -1) {
		switch(opt){
		case 'h':
			dohelp(0);
		break;
		case 'b': // run before
		if (beforeruns > beforemax - 1) {
			// always leave a terminating NULL at the end of the list.
			runbefore = realloc(runbefore, (beforemax + 10) *
								sizeof(char *));
			// beforemax is the offset into new runbefore to be zeroed.
			memset(runbefore + beforemax, 0, 10 * sizeof(char *));
			beforemax += 10;
		}
		runbefore[beforeruns] = strdup(optarg);
		beforeruns++;
		break;
		case 'a': // run after
		if (afterruns > aftermax - 1) {
			// always leave a terminating NULL at the end of the list.
			runafter = realloc(runafter, (aftermax + 10) *
								sizeof(char *));
			// aftermax is the offset into new runafter to be zeroed.
			memset(runafter + aftermax, 0, 10 * sizeof(char *));
			aftermax += 10;
		}
		runafter[afterruns] = strdup(optarg);
		afterruns++;
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			dohelp(1);
		break;
		} //switch()
	}//while()

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
	sprintf(wrkdir, "%slog/backup.log", home);
	char *logfile = strdup(wrkdir);	// log file
	sprintf(wrkdir, "%slog/dryrun.log", home);
	char *cruftfile = strdup(wrkdir);	// cruft
	sprintf(wrkdir, "%slog", home);
	char *logdir = strdup(wrkdir);	// log dir, to mkdir if needed.
	sprintf(wrkdir, "%s.config/backup/", home);
	char *cfgdir = strdup(wrkdir);	// config dir.
	sprintf(wrkdir, "%sexcludes", cfgdir);
	char *exclfile = strdup(wrkdir);	// excludes file
	sprintf(wrkdir, "%slog/trace.txt", home);
	char *tracefile = strdup(wrkdir);	// trace wtf
	sprintf(wrkdir, "%slog/bulog.bak", home);
	char *bulog = strdup(wrkdir);	// safety copy of the log.
	sprintf(wrkdir, "%slog/errors.log", home);
	char *errlog = strdup(wrkdir);	// log errors.
	sprintf(wrkdir, "%s.config/backup/cruft", home);
	char *cruftregex = strdup(wrkdir);	// describe cruft by regex.

	strcpy (user, getenv("LOGNAME"));

	// Is this the first run of backup?
	if (fileexists(cfgfile) == -1 ||
		fileexists(exclfile) == -1 ||
		fileexists(cruftregex) == -1 )
	{
		firstrun("backup", "excludes", "backup.cfg", "cruft", NULL);
		// the log dir most likely needs to be made
		char cmdbuf[NAME_MAX];
		sprintf(cmdbuf, "mkdir -p %s", logdir);
		dosystem(cmdbuf);
		fprintf(stderr,
		"3 files, excludes, backup.cfg and cruft have been installed "
		"at %s\n"
		"You will need to edit these files to suit your system.\n"
		"Next run of backup will do the backup.\n", logdir);
		goto finis;
	}

	// redirect stdout, stderr
	fpo = dofreopen(logfile, "a", stdout);
	fpe = dofreopen(errlog, "a", stderr);

	// nice() this thing, I don't want it to delay anything else.
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

	if (getbupath("/etc/mtab", bdev, dirname, wrkdir) == -1) {
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

	// processes to run before the actual backup.
	int i = 0;
	while(runbefore[i]) {
		dosystem(runbefore[i]);
		i++;
	}

	// record beginning date
	logthis(progname, "Begin backup.", stdout);
	logthis("Backup destination:", bupath, stdout);
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

	// I want to split the dry run stuff out of backup.log to record it
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
	 * this file is overloaded with browser crap. Run decruft to strip
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
		writefile(logfile, lgfr, lgto, "w");	// new copy of logfile.
		drfr = lgto + strlen("<dryrun>") + 1;
		drto = memmem(drfr, logs.to - drfr, "</dryrun>",
						strlen("</dryrun>"));
		writefile(cruftfile, drfr, drto, "w"); // new copy of cruftfile.
		/* Ok, we made two out of the one. Using the pointers as I have
		 * means that there will be no xml tags in either file. They
		 * are not needed. Apart from the xml the entire original log is
		 * a concatenation of the two subfiles. I can now safely rm the
		 * original log. NB I write a new copy of the dryrun file as all
		 * deleting notifications written to it will repeat until such
		 * time as the cleanup program is run.
		*/
		dounlink(bulog);	// done with it.
	}

	// process the run after programs/scripts
	i = 0;
	while(runafter[i]) {
		char buf[PATH_MAX];
		(void)realpath(runafter[i], buf);
		dosystem(buf);
		i++;
	}

	free(logs.from);
	fclose(fpe);	// the error log.
	free(bupath);

finis:
	// free my heap strings
	free(cruftregex);
	free(errlog);
	free(bulog);
	free(tracefile);
	free(cfgdir);
	free(exclfile);
	free(logdir);
	free(cruftfile);
	free(logfile);
	free(cfgfile);

	return 0;
} // main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}
