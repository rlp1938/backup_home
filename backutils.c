/*      backutils.c
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

#include "backutils.h"
#include "fileutil.h"


void getconfig(const char *from, const char *to, char *device,
						char *dirname)
{	// from, to data in. device, dirname data out.
	char *thedev="dev=";
	char *thedir="backupdir=";
	char *cpf, *cpt, *cp;

	// the device
	cp = memmem(from, to - from, thedev, strlen(thedev));
	if(!cp) {
		fprintf(stderr, "%s\n", "Corrupted config file.");
		fwrite(from, 1, to-from, stderr);
		exit(EXIT_FAILURE);
	}
	cpf = cp + strlen(thedev);
	cpt = memchr(cpf, '\n', to - cpf);
	if (!cpt) {
		fprintf(stderr, "%s\n", "Corrupted config file.");
		fwrite(cpf, 1, to-cpf, stderr);
		exit(EXIT_FAILURE);
	}
	*cpt = '\0';
	strcpy(device, cpf);

	// the backup dir to search for.
	cp = memmem(from, to - from, thedir, strlen(thedir));
	if(!cp) {
		fprintf(stderr, "%s\n", "Corrupted config file.");
		fwrite(from, 1, to-from, stderr);
		exit(EXIT_FAILURE);
	}
	cpf = cp + strlen(thedir);
	cpt = memchr(cpf, '\n', to - cpf);
	if (!cpt) {
		fprintf(stderr, "%s\n", "Corrupted config file.");
		fwrite(cpf, 1, to-cpf, stderr);
		exit(EXIT_FAILURE);
	}
	*cpt = '\0';
	strcpy(dirname, cpf);
} // getconfig()

int getbupath(char *from, char *to, char *dev, char *budir,
			char *bupath)
{	// return -1 if not, 0 otherwise.
	char *cp;

	cp = from;
	while((cp = memmem(cp, to - cp, dev, strlen(dev)))) {
		/* immaterial if the dev is dev1, dev2 etc, all that
		   matters is the mountpoint, and if the backup dir
		   is to be found within it.
		*/
		char fline[NAME_MAX];
		char *retpath, *cpf, *cpt;

		strncpy(fline, cp, to - cp);
		fline[to - cp] = '\0';
		// now operate on fline
		cpf = strchr(fline, ' ');	// first space, mountpoint after.
		cpf++;	// now at mountpoint
		cpt = strchr(cpf, ' ');
		*cpt = '\0';
		retpath = recursedir(cpf, budir);
		if(retpath) {
			strcpy(bupath, retpath);
			return 0;
		}
		// there may more than 1 dev in the file.
		cp += strlen(fline);
	} // while(cp...)
	return -1;
} // getbupath()

char *recursedir(char *topdir, char *searchfor)
{	// the first object presented will not be "lost+found"
	DIR *dp;
	struct dirent *de;

	dp = opendir(topdir);
	if(!dp) {
		perror(topdir);
		return NULL;
	}
	while ((de = readdir(dp))){
		if (strcmp(de->d_name, ".") == 0) continue;
		if (strcmp(de->d_name, "..") == 0) continue;
		if (de->d_type != DT_DIR){
			continue;
		} else {	// Ok this is a dir.
			static char path[PATH_MAX];
			if (strcmp(de->d_name, "lost+found") == 0) continue;
			// formulate the new path.
			strcpy(path, topdir);
			if (path[strlen(path) -1] != '/') strcat(path, "/");
			strcat(path, de->d_name);
			if (strcmp(searchfor, de->d_name) == 0) {
				closedir(dp);
				return path;
			} else {
				char *retp;
				retp = recursedir(path, searchfor);
				if (retp) {
					closedir(dp);
					return retp;
				} // if(retp)
			} // else...
		} // else...
	} // while(de...)
	closedir(dp);
	return NULL;
} // recursedir()

void logthis(const char *who, const char *msg, FILE *fplog)
{
	time_t tm;
	tm = time(0);
	fprintf(fplog, "%s%s: %s\n", ctime(&tm), who, msg);
} // logthis()

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
