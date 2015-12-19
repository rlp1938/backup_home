/*      bulogrot.c - rotate the backup log files.
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

#include "fileops.h"


static void rotate(const char *rootname);

int main()
{
	/* I have no knowledge whether the binary backup or the bash script
	 * backup.sh may be running so I'll need to do several tests.
	*/

	char pathname[PATH_MAX];
	char user[NAME_MAX], home[NAME_MAX];
	char *logfile, *errlog;

	// set up my file names
	strcpy (user, getenv("LOGNAME"));
	strcpy(home, getenv("HOME"));
	sprintf(pathname, "%s/log/backup.log", home);
	logfile = strdup(pathname);
	sprintf(pathname, "%s/log/errors.log", home);
	errlog = strdup(pathname);

	// Is an earlier instance of any backup program running?
	/* I will avoid using lockfiles, just look for what is already
	 * running.
	*/
	{
		char *prlist[3] = {
			"backup", "cleanup", NULL
		};
		int res = isrunning(prlist);
		while(res) {
			sleep(60);
			res = isrunning(prlist);
		}
	}
	rotate(logfile);
	rotate(errlog);
	free(errlog);
	free(logfile);
	return 0;
}

void rotate(const char *rootname)
{
	FILE *fpo;
	char thenames[8][PATH_MAX];
	// dropped the procedure to avoid rotating small files.
	// now the array of filenames for the rotation
	strcpy(thenames[0], rootname);
	int i;
	for (i=1; i<8; i++) {
		sprintf(thenames[i], "%s.%d", thenames[0], i);
	}
	// do the actual rotation
	if (fileexists(thenames[7]) == 0) dounlink(thenames[7]);
	for (i=6; i>-1; i--) {
		if (fileexists(thenames[i]) == 0)
			dorename(thenames[i], thenames[i+1]);
	} // for()
	fpo = dofopen(rootname, "w");	// touch the new logfile.
	fclose(fpo);
} // rotate()
