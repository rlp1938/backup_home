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
#include "greputils.h"


void getconfig(const char *from, const char *to, char *regex,
						char *budirdir)
{	// from, to data in. device, dirname data out.
	char *theregex="regex=";
	char *thedir="backupdir=";
	char *cpf, *cpt, *cp;

	// the device
	cp = memmem(from, to - from, theregex, strlen(theregex));
	if(!cp) {
		fprintf(stderr, "%s\n", "Corrupted config file.");
		fwrite(from, 1, to-from, stderr);
		exit(EXIT_FAILURE);
	}
	cpf = cp + strlen(theregex);
	cpt = memchr(cpf, '\n', to - cpf);
	if (!cpt) {
		fprintf(stderr, "%s\n", "Corrupted config file.");
		fwrite(cpf, 1, to-cpf, stderr);
		exit(EXIT_FAILURE);
	}
	*cpt = '\0';
	strcpy(regex, cpf);

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
	strcpy(budirdir, cpf);
} // getconfig()

int getbupath(char *mtabfile, char *bregex, char *budir,
			char *bupath)
{	// return -1 if not, 0 otherwise.
	// I want a workfile in /tmp
	struct stat sb;
	char outfil[NAME_MAX];
	char myregex[120];
	sprintf(outfil, "/tmp/grepresult%i",getpid());
	// build the real regex
	strcpy(myregex, bregex);
	if (myregex[strlen(myregex) - 1] != '/') strcat(myregex, "/");
	// myregex should look something like "/media/"
	dogrep(outfil, mtabfile, myregex, "-E", NULL);
	sync();
	if (stat(outfil, &sb) == -1) goto finis;	// Should be impossible
	if (sb.st_size == 0) goto finis;	// grep came up empty
	// The file will have 1 or more lines.
	char *cp;
	filedata fdat = readfile(outfil, 0, 1);
	unlink(outfil);

	// loop over the file data and extract dir search path
	cp = fdat.from;
	while (cp < fdat.to) {
		char buf[PATH_MAX];
		char *mtdir = strstr(cp, myregex);	// get past /dev...
		char *end = strchr(mtdir, ' ');		// end of the dir part
		*end = '\0';
		strcpy(buf, mtdir);
		*end = ' ';	// restore input data
		char *bup = recursedir(buf, budir);
		if (bup) {
			strcpy(bupath, bup);
			return 0;
		}
		// next line
		end = strchr(cp, '\n');
		cp = end + 1;
	}
	return -1;	// never found the backup path
finis:
	unlink(outfil);
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
