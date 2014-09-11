/* 
   64net/2 Module - globbing routines
   (C)opyright Paul Gardner-Stephen 1995, All Rights Reserved
 */

#include "config.h"
#include "fs.h"

/* structure for glob match list generation */
typedef struct {
	int gap;
	int c;
	int star;
}

glob_record;

int glob_p_comp (uchar glob_array[17][32], uchar *pattern) {
	/* glob the file (ignoring filetype obviously!) */
	int i;

	/* glob each individual match line */
	for (i = 1; i < 17; i++) {
		if (strlen (glob_array[i])) {
			/* okay, this match line is non-trivial */
			return (glob_match (glob_array[i], pattern));
		}
	}

	/* no match */
	return (0);
}

int glob_match (uchar *glob, uchar *pattern) {
	//fixed bug. now we can't load "foo01" anymore 
	//by just loading "foo". now we need to load
	//"foo*", "foo01" or alike to make that happen. TB
	int i;
	//XXX pattern with characters above $e0 are same as value ^0x20 as they appear twice in petscii set
	for(i=0; i < (int)strlen(pattern); i++) {
		//still within glob, so compare
		if(i<(int)strlen(glob)) {
			if(glob[i]=='?') {
				//we can assume any char, but take care we have not
				//reached the padding already
				if(pattern[i]==0xa0) {
					return 0;
				}
			}
			else {
				//great, a star, so no need to care about the rest
				if(glob[i]=='*' || glob[i]==0xa0) {
					return 1;
				}
				else if(glob[i]!=pattern[i]) {
					if(pattern[i]>=0xe0) {
						if((pattern[i]^0x40)!=glob[i]) {
							return 0;
						}
					}
					else {
						return 0;
					}
				}
			}
		}
		
		//end of glob. now rest of pattern must be empty or we fail
		//as pattern would be longer than glob then.
		else {
			if(pattern[i]!=0xa0) {
				return 0;
			}
		}
	}
	return 1;
}

int parse_glob (uchar glob_array[17][32], uchar *pattern) {
	/* BUGS: This procedure cant differentiate between a search glob and an
	   open glob - this means at the moment you could quite happily load a 
	   REL or USR file! */

	/* parse a glob string into a form ready for happyness */
	int i, flag = 1, snum, sposs, dir_search = 0;
	uchar fnord[1024];

	/* work on a copy so we dont muck it up */
	strcpy ((char*)fnord, (char*)pattern);


	/* clear glob_array */
	for (i = 0; i < 16; i++) {
		glob_array[0][i] = 0;
		glob_array[i + 1][0] = 0;
	}

	/* check right hand side for special delimiters
	   ,P - match PRG   ,S - match SEQ
	   ,U - match USR   ,L - match REL (for read/open)
	   ,D - match DIR   ,C - match CBM
	   ,UFS - match UFS ,$ - dir search, not OPEN
	   ,ALL - All files ,- - match none
	   ,* - All         ,N - match NET
	   ,L, - REL for write(create)

	   AND - Remove and ignore ,R & ,W

	*/

	while (flag) {
		flag = 0;
		i = strlen (fnord) - 1;
		/* this may be bad.... */
		/* REL files can be openned two ways, 
		   1) Specifying the record size (for create only) */
		if ((fnord[i - 3] == ',') && (fnord[i - 2] == 'L') && (fnord[i - 1] == ',')) {
			glob_array[0][cbm_REL] = 1;
			glob_array[0][30] = 1;	/* REL record length is valid */
			glob_array[0][31] = fnord[i];	/* rel record size */
			fnord[strlen (fnord) - 4] = 0;
			flag = 1;
		}
		/* or 
		   2) For read mode, with no record size */
		if ((fnord[i - 2] == ',') && (fnord[i - 1] == 'L')) {
			glob_array[0][cbm_REL] = 1;
			glob_array[0][30] = 0;	/* rel record length is invalid */
			glob_array[0][31] = 0;	/* rel record size (just to make sure) */
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'W')) {
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'R')) {
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == '$')) {
			dir_search = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == '*')) {
			glob_array[0][cbm_SEQ] = 1;
			glob_array[0][cbm_PRG] = 1;
			glob_array[0][cbm_USR] = 1;
			glob_array[0][cbm_REL] = 1;
			glob_array[0][cbm_CBM] = 1;
			glob_array[0][cbm_DIR] = 1;
			glob_array[0][cbm_UFS] = 1;
			glob_array[0][cbm_NET] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'P')) {
			glob_array[0][cbm_PRG] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'S')) {
			glob_array[0][cbm_SEQ] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'U')) {
			glob_array[0][cbm_USR] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'C')) {
			glob_array[0][cbm_CBM] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'D')) {
			glob_array[0][cbm_DIR] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 1] == ',') && (fnord[i] == 'N')) {
			glob_array[0][cbm_NET] = 1;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
		if ((fnord[i - 3] == ',') && (fnord[i - 2] == 'U') && (fnord[i - 1] == 'F') && (fnord[i] == 'S')) {
			glob_array[0][cbm_UFS] = 1;
			fnord[strlen (fnord) - 4] = 0;
			flag = 1;
		}
		if ((fnord[i - 3] == ',') && (fnord[i - 2] == 'A') && (fnord[i - 1] == 'L') && (fnord[i] == 'L')) {
			/* everything but DEL files */
			glob_array[0][cbm_SEQ] = 1;
			glob_array[0][cbm_PRG] = 1;
			glob_array[0][cbm_USR] = 1;
			glob_array[0][cbm_REL] = 1;
			glob_array[0][cbm_CBM] = 1;
			glob_array[0][cbm_DIR] = 1;
			glob_array[0][cbm_UFS] = 1;
			glob_array[0][cbm_NET] = 1;
			fnord[strlen (fnord) - 4] = 0;
			flag = 1;
		}

		if ((fnord[i - 1] == ',') && (fnord[i] == '-')) {
			/* nothing */
			glob_array[0][cbm_SEQ] = 0;
			glob_array[0][cbm_PRG] = 0;
			glob_array[0][cbm_USR] = 0;
			glob_array[0][cbm_REL] = 0;
			glob_array[0][cbm_CBM] = 0;
			glob_array[0][cbm_DIR] = 0;
			glob_array[0][cbm_UFS] = 0;
			glob_array[0][cbm_NET] = 0;
			fnord[strlen (fnord) - 2] = 0;
			flag = 1;
		}
	}

	for (i = 0; i < 8; i++) flag += glob_array[0][i];
	if (flag == 0) {
		if (dir_search) {
			/* no UFS, but all else */
			glob_array[0][cbm_SEQ] = 1;
			glob_array[0][cbm_PRG] = 1;
			glob_array[0][cbm_USR] = 1;
			glob_array[0][cbm_REL] = 1;
			glob_array[0][cbm_CBM] = 1;
			glob_array[0][cbm_DIR] = 1;
			glob_array[0][cbm_NET] = 1;
		}
		/* only PRG files matched by default for open */
		else glob_array[0][cbm_PRG] = 1;
	}


	/* seperate seperate "globables" */
	/* This bit is just as easy - all we do is look for commas */
	snum = 1;
	sposs = 0;
	i = 0;
	while (i < (int)strlen (fnord)) {
		if (sposs > 31) {
			/* yick! a globable which is over-globulated and wont fit into its glob-hole */
			return (-2);
		}

		if (fnord[i] == ',') {
			/* next globable! */
			glob_array[snum][sposs] = 0;
			snum++;
			sposs = 0;
			i++;
		}
		else {
			/* add to string */
			glob_array[snum][sposs++] = fnord[i++];
		}
	}
	/* end off the last string */
	//if (sposs == 0) {
		/* null, ergo add a * */
		/* yuck! if we do so an @s: this ends up in doing a scratch on all files! We should better handle a zero size filename as error, Toby */
		//glob_array[snum][sposs++] = '*';
		
	//}
	glob_array[snum][sposs] = 0;

	/* Well by George, we've done it! */
	return (0);
}
