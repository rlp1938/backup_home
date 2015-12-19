/*      greputils.c
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

#include "greputils.h"
#include "fileops.h"

void compile_pattern(const char *pat)
{
	int flags = REG_NOSUB;	/* don't need where-matched info */
	int ret;

	if (ignore_case)
		flags |= REG_ICASE;
	if (extended)
		flags |= REG_EXTENDED;

	ret = regcomp(& pattern, pat, flags);
	if (ret != 0) {
#define MSGBUFSIZE	512	/* arbitrary */
		char error[MSGBUFSIZE];
		(void) regerror(ret, & pattern, error, sizeof error);
		fprintf(stderr, "%s: pattern `%s': %s\n", myname, pat, error);
		errors++;
	}
}

/* process --- read lines of text and match against the pattern */

void process(const char *name, FILE *fp, FILE *fpo)
{
	char *buf = NULL;
	size_t size = 0;
	char error[MSGBUFSIZE];

	while (getline(& buf, &size, fp) != -1) {
		int ret;
		ret = regexec(& pattern, buf, 0, NULL, 0);
		switch(ret) {
			case REG_NOERROR:	// found a match.
			if (invert == 0)
				fprintf(fpo, "%s", buf);	// matching lines.
			break;
			case REG_NOMATCH:	// nothing matched.
			if (invert == 1)
				fprintf(fpo, "%s", buf);	// non-matching lines.
			break;
			default:	// catch all for all other failures.
				(void) regerror(ret, & pattern, error, sizeof error);
				fprintf(stderr, "%s: file %s: %s\n",
						myname, name, error);
				errors++;
			break;
		} // switch(ret)
	} // while()
	free(buf);
} // process()

void dogrep(const char *file2write, const char *file2grep,
			const char *regex, ...)
{	/* Will be a bit similar to a main() for grep but much simplified.
	* invoke as dogrep(filename, regex, -option1, -option2, ...)
	* options are presently limited to any or all of -E, -v, -i, or
	* none at all.
	* I suspect that I need do nothing for '^regex' or 'regex$' but what
	* about -w (whole word option). So TODO: Maybe wrap the given regex
	* between appropriate character classes. Or at least find out what
	* to do instead.
	*/
	va_list argp;
	FILE *fp, *fpo;

	va_start(argp, regex);
	while(1) {
		char *cp = va_arg(argp, char*);
		if(!cp) break;
		if (cp[0] != '-') {
			fprintf(stderr, "Invalid option format: %s\n", cp);
			exit (EXIT_FAILURE);
		}
		switch (cp[1]) {
			case 'i':
			ignore_case = 1;
			break;
			case 'E':
			extended = 1;
			break;
			case 'v':
			invert = 1;
			break;
			default:
			break;
			fprintf(stderr, "Unsupported option: %c\n", cp[1]);
			exit(EXIT_FAILURE);
			break;
		} //switch()
	} // while()

	va_end(argp);

	// compile the pattern
	compile_pattern(regex);
	if (errors) {	// report done inside compile_pattern()
		exit(EXIT_FAILURE);
	}
	fpo = dofopen(file2write, "w");
	fp = dofopen(file2grep, "r");
	process("", fp, fpo);
	regfree(& pattern);
	fclose(fp);
	fclose(fpo);
} // dogrep()
