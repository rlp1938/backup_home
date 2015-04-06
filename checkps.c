/*      checkps.c
 *
 *	Copyright 23.12.2014 Bob Parker <rlp1938@gmail.com>
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

//#include "backutils.h"
#include "fileutil.h"
//#include "greputils.h"
#include "checkps.h"
#include "runutils.h"



int checkps(void)
{	// get a list of ps and search for 'backup' and 'rsync' within it.
	char command[NAME_MAX], fn[NAME_MAX];
	struct fdata dat;
	FILE *fpo;
	char *cp;
	pid_t mypid = getpid();

	sprintf(fn, "%s%d", progname, mypid);

	fpo = dofreopen(fn, "w", stdout);
	sprintf(command, "ps -ef");
	dosystem(command);
	fclose(fpo);
	dat = readfile(fn, 0, 1);
	dounlink(fn);

	// Who am I? Right now I don't care, just see if I exist more than
	// once.
	cp = memmem(dat.from, dat.to - dat.from, progname,
					strlen(progname));
	if (cp) {
		cp += strlen(progname);
		cp = memmem(cp, dat.to - cp, progname, strlen(progname));
			if (cp) goto found;
	}

	// Who am I? It matters now.
	if (strcmp(progname, "bulogrot") == 0) {
	cp = memmem(dat.from, dat.to - dat.from, "cleanup",
					strlen("cleanup"));
		if (cp) goto found;
		cp = memmem(dat.from, dat.to - dat.from, "backup",
					strlen("backup"));
		if (cp) goto found;
	} else if (strcmp(progname, "backup") == 0) {
		cp = memmem(dat.from, dat.to - dat.from, "cleanup",
					strlen("cleanup"));
		if (cp) goto found;
		cp = memmem(dat.from, dat.to - dat.from, "bulogrot",
					strlen("bulogrot"));
		if (cp) goto found;
	} else { // it's  cleanup
		cp = memmem(dat.from, dat.to - dat.from, "backup",
					strlen("backup"));
		if (cp) goto found;
		cp = memmem(dat.from, dat.to - dat.from, "bulogrot",
					strlen("bulogrot"));
		if (cp) goto found;
	}

	free(dat.from);
	return -1;

found:
	free(dat.from);
	return 0;
} // checkps()
